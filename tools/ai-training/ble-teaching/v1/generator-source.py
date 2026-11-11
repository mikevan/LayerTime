"""Candidate explanations of observed BLE features, not incident verdicts.

Selected windows come only from publisher-labelled benign reference recordings.
They are teaching selections, not a random performance sample or new environments.
No new model is trained and no production task decoder is enabled here.
"""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
from ble_features import FEATURES


def select_rows(x,y,groups,predictions):
    by_row={r['row']:r for r in predictions}
    if len(by_row)!=len(predictions) or set(by_row)!=set(range(len(y))):
        raise ValueError('Prediction row coverage differs from feature matrix')
    if not np.isfinite(x).all():raise ValueError('Nonfinite features')
    for i in range(len(y)):
        if by_row[i]['label']!=int(y[i]) or by_row[i]['session']!=str(groups[i]):
            raise ValueError('Prediction label/session differs from feature row')
    selected=[]
    for session in sorted(set(str(g) for g in groups[y==0])):
        available=[i for i in range(len(y)) if y[i]==0 and str(groups[i])==session]
        used=set()
        for topic,feature in [('repeated_data','payload_repeat_fraction'),('address_format','random_address_fraction')]:
            eligible=[i for i in available if i not in used]
            if not eligible:raise ValueError('Insufficient distinct benign windows')
            row=min(eligible,key=lambda i:(-float(x[i,FEATURES.index(feature)]),i));used.add(row)
            selected.append((session,topic,row))
        disagreement=[i for i in available if i not in used and by_row[i]['catboostScore']<0.5 and
                      by_row[i]['anomalyScore']>=by_row[i]['anomalyThreshold']]
        if not disagreement:raise ValueError('No distinct benign disagreement window')
        selected.append((session,'model_disagreement',min(disagreement)))
    return selected


def build(root,output):
    if output.exists():raise ValueError('Use a new output version')
    feature_dir=root/'runs/ble-features-v1';model_dir=root/'runs/ble-specialists-v1'
    manifest=json.loads((feature_dir/'manifest.json').read_text(encoding='utf-8'))
    model_report=json.loads((model_dir/'report.json').read_text(encoding='utf-8'))
    digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    if model_report['featuresManifestSha256']!=digest(feature_dir/'manifest.json'):
        raise ValueError('Feature manifest differs from recorded specialist experiment')
    if manifest['source']['license']!='CC-BY-4.0' or manifest['features']!=FEATURES:
        raise ValueError('Unsupported source permission or feature layout')
    data=np.load(feature_dir/'features.npz');x,y,groups=data['x'],data['y'],data['groups']
    predictions=[json.loads(s) for s in (model_dir/'held-out-predictions.jsonl').read_text(encoding='utf-8').splitlines()]
    selected=select_rows(x,y,groups,predictions);by_row={r['row']:r for r in predictions}
    sessions={s['sessionId']:s for s in manifest['sessions']};records=[]
    source=json.loads((root/'sources.json').read_text(encoding='utf-8'))['sources']['ble-ntmmcw6jj8-v2']
    for session,topic,row in selected:
        if sessions[session]['label']!=0:raise ValueError('Selected source is not labelled benign')
        reference=dict(sourceId='ble-ntmmcw6jj8-v2',sessionId=session,featureRow=row)
        features={name:float(x[row,i]) for i,name in enumerate(FEATURES)};prediction=by_row[row]
        facts=dict(reference=reference,windowSeconds=manifest['windowSeconds'],features=features,
            referenceSessionLabel='publisher-labelled benign; not individual-packet ground truth',
            modelOutputs=dict(catboostScore=prediction['catboostScore'],catboostExperimentalThreshold=0.5,
                anomalyScore=prediction['anomalyScore'],anomalyProvisionalThreshold=prediction['anomalyThreshold']),
            coverage='Accepted captured legacy advertising records only; continuous channel coverage is not established.')
        if topic=='repeated_data':
            question='Do repeated Bluetooth advertising data values establish an attack?'
            headline='Repeated data also appears in this recording labelled normal.'
            explanation=(f"The feature extractor estimates a repeated-data fraction of {features['payload_repeat_fraction']:.3f} "
                'among accepted records in this ten-second reference window. The source recording was labelled benign by its publisher. '
                'Repetition alone does not establish harmful intent or successful disruption. This label does not make similar activity elsewhere automatically safe.')
        elif topic=='address_format':
            question='Does the random-address flag identify the person behind a Bluetooth device?'
            headline='An address-format flag does not identify a person.'
            explanation=(f"The decoder marked a fraction of {features['random_address_fraction']:.3f} of accepted records as using a random-address format. "
                'That flag does not identify a person, establish intent, or show that one device rotated through all observed addresses. '
                'The source recording was labelled benign; that reference label is not an identity or authorization check.')
        else:
            question='How should I interpret these conflicting model flags?'
            headline='The models disagree on a window from a recording labelled normal.'
            explanation=(f"The recorded classifier score {prediction['catboostScore']:.3f} is below its experimental 0.5 threshold, "
                f"while the anomaly score {prediction['anomalyScore']:.3f} exceeds its provisional threshold {prediction['anomalyThreshold']:.3f}. "
                'The models disagree on a publisher-labelled benign reference window. Neither score is a calibrated probability of an attack. '
                'A flag needs supporting evidence and coverage review before an incident conclusion.')
        answer=dict(headline=headline,explanation=explanation,evidenceReferences=[reference])
        records.append(dict(id=f'ble-reference-v1-{session}-{topic}',family=session,topic=topic,
            task='EXPLAIN_BLE_REFERENCE_WINDOW',sourceId='layertime-ble-reference-teaching-v1',
            sourceCaptureSha256=sessions[session]['sha256'],trainingApproved=False,releaseEligible=False,
            labelScope='Authored feature interpretation; publisher session label is not an incident verdict.',
            messages=[dict(role='system',content='Explain the supplied reference-window evidence in plain language. Start with the important implication. Return JSON with headline, explanation and evidenceReferences. Do not claim field safety, physical identity, device connection, or a proven attack from these features.'),
                dict(role='user',content=question+'\nREFERENCE WINDOW\n'+json.dumps(facts,sort_keys=True)),
                dict(role='assistant',content=json.dumps(answer,sort_keys=True))]))
    output.mkdir(parents=True)
    path=output/'candidates.jsonl';path.write_text(''.join(json.dumps(r,sort_keys=True)+'\n' for r in records),encoding='utf-8')
    notice=dict(attribution=source['attribution'],sourceUrl=source['url'],doi=source['doi'],
        license='CC BY 4.0',licenseUrl='https://creativecommons.org/licenses/by/4.0/',
        changes='Derived feature windows, selected teaching cases and authored explanations; no endorsement by source authors is implied.')
    (output/'SOURCE-NOTICE.json').write_text(json.dumps(notice,indent=2),encoding='utf-8')
    result=dict(examples=len(records),originalTeachingQuestions=3,sourceRecordings=len({r['family'] for r in records}),
        independentEnvironments=1,sha256=digest(path),trainingApproved=False,releaseEligible=False,
        inputHashes={str(p.relative_to(root)):digest(p) for p in [feature_dir/'manifest.json',feature_dir/'features.npz',model_dir/'held-out-predictions.jsonl']},
        limitations=['Selected windows are not a performance sample or additional independent recordings.',
            'Whole-recording family isolation is required for future splits; no split or training intake is approved here.',
            'Feature rows identify accepted windows by matrix row, not a guaranteed packet-time interval or raw frame list.',
            'Candidate wording is same-agent authored, not independent expert or user-comprehension review.',
            'This distinct task is not enabled in the production decoder or the current language-model training/evaluation.'])
    (output/'manifest.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    (output/'generator-source.py').write_bytes(Path(__file__).read_bytes())
    return result


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    print(json.dumps(build(Path(__file__).parent,args.output)))
