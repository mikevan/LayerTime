"""Preserve the authored correction set and add positive confirmations sharing each context."""
import hashlib
import json
from pathlib import Path
import shutil

CONFIRMATIONS=[
('Can the rule window be known while the observed duration remains unknown?','Yes. Configuration and measurement are separate: the rule value is known, while the supplied event times remain unknown.'),
('Do the other captured records establish that some activity was observed?','Yes. Other records were captured even though the selected disconnect and EAPOL patterns did not match.'),
('May the report identify the target while leaving the sender unknown?','Yes. The addressed target is supplied; the sender is not. Preserve that distinction.'),
('Should the report lead with the different network identities?','Yes. The network mismatch is the key reason the matching client alone does not establish the same-pair sequence.'),
('May the report describe an earlier exchange without calling it a follow-on event?','Yes. Describe its observed position before the request and keep it distinct from a later linked exchange.'),
('May the report describe the matching correlation without asserting a cause?','Yes. The supplied identities and timing support the configured relationship, while causation and completed outcomes remain unresolved.'),
('Should the report show the observed pattern together with its evidence limits?','Yes. Present the requests and linked exchange, then distinguish those observations from unverified disconnect, reconnect and credential outcomes.'),
('May the report explain the supplied rule with its values unchanged?','Yes. Explain the existing settings and their meaning; keep an unrequested policy change out of the explanation.')]


def build(root):
    source=root/'domain-data/evidence-meaning-negative-only-draft';out=root/'domain-data/evidence-meaning-teaching-v1';out.mkdir(exist_ok=False)
    records=[json.loads(s) for s in (source/'counterexamples.jsonl').read_text().splitlines()]
    if len(records)!=len(CONFIRMATIONS):raise ValueError('Unexpected authored correction set')
    paired=[]
    for original,(question,answer) in zip(records,CONFIRMATIONS):
        challenge=dict(original,id=original['id']+'-challenge',family=original['id']);paired.append(challenge)
        confirm=json.loads(json.dumps(challenge));confirm['id']=original['id']+'-confirm'
        confirm['messages'][1]['content']=question+'\n'+original['messages'][1]['content'].split('\n',1)[1]
        target=json.loads(confirm['messages'][-1]['content']);target['answer']=answer;confirm['messages'][-1]['content']=json.dumps(target);paired.append(confirm)
    shutil.copy2(source/'principles.jsonl',out/'principles.jsonl')
    (out/'counterexamples.jsonl').write_text('\n'.join(json.dumps(r,sort_keys=True) for r in paired)+'\n')
    manifest=dict(principles=8,contrastContexts=8,pairedTeachingQuestions=len(paired),trainingApproved=False,modelTrainingPerformed=False,
        note='Positive and corrective questions share contexts; these are post-diagnostic teaching candidates, not new independent evaluation.',
        parentManifestSha256=hashlib.sha256((source/'manifest.json').read_bytes()).hexdigest(),files={})
    for p in out.iterdir():manifest['files'][p.name]=hashlib.sha256(p.read_bytes()).hexdigest()
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2));return manifest


if __name__=='__main__':print(json.dumps(build(Path(__file__).parent),indent=2))
