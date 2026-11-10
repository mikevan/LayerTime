"""Acquire primary references and extract page-bounded candidate passages.

Run with the bundled Python containing pypdf. Passage extraction is not approval
for supervised training; no answers, labels or dataset splits are invented here.
"""
import argparse
import hashlib
import json
import re
import urllib.request
from pathlib import Path

POLICY='https://www.nist.gov/open/copyright-fair-use-and-licensing-statements-srd-data-software-and-technical-series-publications'
SOURCES=[
    dict(id='nist-sp800-153',url='https://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-153.pdf',
         date='2012-02',title='Guidelines for Securing Wireless Local Area Networks',
         scope='Historical WLAN monitoring guidance; not current WPA3 or Wi-Fi 6/7 coverage.'),
    dict(id='nist-sp800-94',url='https://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-94.pdf',
         date='2007-02',title='Guide to Intrusion Detection and Prevention Systems',
         scope='Historical detection concepts; do not treat obsolete technology or configuration advice as current.'),
    dict(id='nist-sp800-121r2-upd1',url='https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-121r2-upd1.pdf',
         date='2022-01-19',title='Guide to Bluetooth Security',
         scope='Publication covers Bluetooth through 4.2; external coauthors require rights review. Not Bluetooth 5/6 coverage.')]


def sha(raw):return hashlib.sha256(raw).hexdigest()


def acquire(url,path):
    if path.exists():return path.read_bytes()
    request=urllib.request.Request(url,headers={'User-Agent':'LayerTime-research-reference-intake/1.0'})
    with urllib.request.urlopen(request,timeout=45) as response:raw=response.read(32*1024*1024+1)
    if len(raw)>32*1024*1024:raise ValueError('Reference exceeds intake size limit')
    pending=path.with_suffix(path.suffix+'.partial');pending.write_bytes(raw);pending.replace(path)
    return raw


def passages(text,max_words=220):
    if not 50<=max_words<=1000:raise ValueError('Invalid passage budget')
    words=re.sub(r'\s+',' ',text).strip().split()
    return [' '.join(words[start:start+max_words]) for start in range(0,len(words),max_words)
            if len(words[start:start+max_words])>=20]


def main(out):
    from pypdf import PdfReader
    out.mkdir(parents=True,exist_ok=True)
    policy=acquire(POLICY,out/'nist-rights.html')
    rows=[];docs=[]
    for source in SOURCES:
        path=out/(source['id']+'.pdf');raw=acquire(source['url'],path)
        if not raw.startswith(b'%PDF'):raise ValueError('Expected a PDF document')
        reader=PdfReader(path);pages=[]
        for number,page in enumerate(reader.pages,1):
            text=page.extract_text() or ''
            pages.append(dict(pdfPage=number,text=text))
            for ordinal,chunk in enumerate(passages(text)):
                rows.append(dict(id=f"{source['id']}:p{number}:c{ordinal}",sourceId=source['id'],
                    sourceSha256=sha(raw),pdfPage=number,passage=chunk,passageSha256=sha(chunk.encode()),
                    splitGroup=source['id'],trainingApproved=False,
                    reviewStatus='extracted-pending-relevance-factual-currency-and-rights-review'))
        (out/(source['id']+'.pages.json')).write_text(json.dumps(pages,indent=2),encoding='utf-8')
        docs.append(dict(source,pages=len(pages),bytes=len(raw),sha256=sha(raw),rightsReference=POLICY,
            attribution='Republished courtesy of the National Institute of Standards and Technology.',
            rightsStatus='Reference intake only; review third-party authors and notices before selecting training text.'))
    body=''.join(json.dumps(r,ensure_ascii=False,sort_keys=True)+'\n' for r in rows)
    (out/'passages.jsonl').write_text(body,encoding='utf-8')
    manifest=dict(schemaVersion=1,documents=docs,pages=sum(d['pages'] for d in docs),passages=len(rows),
        passagesSha256=sha((out/'passages.jsonl').read_bytes()),rightsSnapshotSha256=sha(policy),releaseEligible=False,
        trainingApproved=False,limitations=['Passages are source material, not supervised training examples.',
        'No independent packet captures or new evaluation environments are provided by these documents.',
        'Document families must not be randomly split at passage level.',
        'Page numbers refer to PDF positions, not printed page labels. Extraction may lose tables and equations.',
        'Historical references must retain publication dates and protocol scope.'])
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    print(json.dumps(dict(documents=len(docs),pages=manifest['pages'],passages=len(rows),trainingApproved=False)))


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True)
    main(parser.parse_args().output)
