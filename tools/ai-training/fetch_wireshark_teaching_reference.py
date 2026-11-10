"""Download bounded, pinned decoder source as reference data. Never execute it."""
import argparse
import hashlib
import json
import urllib.request
from pathlib import Path
from datetime import datetime,timezone

REPOSITORY='https://github.com/wireshark/wireshark'
FILES=('COPYING','epan/dissectors/packet-ieee80211.c','epan/dissectors/packet-eapol.c',
       'epan/dissectors/packet-eapol.h')


def fetch(url,limit):
    request=urllib.request.Request(url,headers={'User-Agent':'LayerTime-defensive-reference-intake','Accept':'application/vnd.github+json'})
    with urllib.request.urlopen(request,timeout=45) as response:
        data=response.read(limit+1)
    if len(data)>limit:raise ValueError('Reference exceeds intake bound')
    return data


def intake(root):
    out=root/'external-data/wireshark-teaching-reference-v1'
    if out.exists():raise ValueError('Preserve existing intake; use its manifest rather than refetching a moving branch')
    commit=json.loads(fetch('https://api.github.com/repos/wireshark/wireshark/commits/master',2_000_000))
    revision=commit['sha']
    if len(revision)!=40 or any(c not in '0123456789abcdef' for c in revision):raise ValueError('Invalid revision')
    files=[];payload={}
    for name in FILES:
        metadata=json.loads(fetch(f'https://api.github.com/repos/wireshark/wireshark/contents/{name}?ref={revision}',5_000_000))
        raw=fetch(f'https://raw.githubusercontent.com/wireshark/wireshark/{revision}/{name}',5_000_000)
        blob=hashlib.sha1(b'blob '+str(len(raw)).encode()+b'\0'+raw).hexdigest()
        if blob!=metadata['sha'] or len(raw)!=metadata['size']:raise ValueError('Git blob verification failed')
        payload[name]=raw;files.append(dict(path=name,bytes=len(raw),gitBlobSha1=blob,sha256=hashlib.sha256(raw).hexdigest(),
            url=f'{REPOSITORY}/blob/{revision}/{name}'))
    assert sum(len(p) for p in payload.values())<12_000_000
    out.mkdir(parents=True)
    for name,raw in payload.items():
        path=out/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(raw)
    manifest=dict(repository=REPOSITORY,revision=revision,createdUtc=datetime.now(timezone.utc).isoformat(),files=files,
        purpose='Passive decoder semantics and source-linked teaching review; no source execution',
        status='reference-intake-not-training-admission',trainingApproved=False,trainingPerformed=False,
        limitations=['Inspect file-level licence notices and preserve attribution before deriving teaching cards.',
            'Decoder implementation is not itself a complete protocol standard or evidence of hostile intent.',
            'Do not include attack scripts, exploit examples, decryptor routines or key recovery instruction in teaching exports.'])
    (out/'intake-manifest.json').write_text(json.dumps(manifest,indent=2))
    return manifest


if __name__=='__main__':print(json.dumps(intake(Path(__file__).parent),indent=2))
