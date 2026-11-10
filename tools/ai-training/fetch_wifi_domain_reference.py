"""Bounded passive PCAP intake from a publisher repository; never fetch/run attack programs."""
import hashlib
import json
from pathlib import Path
from urllib.request import Request,urlopen
from urllib.parse import quote
import time

REPO='hsngccu/WPA3-dataset'
PREFIX='The_WPA3_Dataset/2. Deauth/'


def get(url):
    for attempt in range(3):
        try:
            with urlopen(Request(url,headers={'User-Agent':'LayerTime-source-intake/1.0','Accept':'application/vnd.github+json'}),timeout=45) as r:return r.read()
        except OSError:
            if attempt==2:raise
            time.sleep(2)


def fetch(root):
    out=root/'external-data/wpa3-hsngccu-deauth-v1';out.mkdir(parents=True,exist_ok=True)
    if (out/'intake-manifest.json').exists():raise ValueError('Preserve completed intake; use its manifest')
    commit_path=out/'commit-metadata.json';tree_path=out/'repository-tree.json'
    commit=json.loads(commit_path.read_text()) if commit_path.exists() else json.loads(get(f'https://api.github.com/repos/{REPO}/commits/main'));revision=commit['sha']
    tree=json.loads(tree_path.read_text()) if tree_path.exists() else json.loads(get(f'https://api.github.com/repos/{REPO}/git/trees/{revision}?recursive=1'))
    if tree.get('truncated'):raise ValueError('Incomplete repository tree')
    selected=[e for e in tree['tree'] if e['type']=='blob' and (e['path'] in ('LICENSE','README.md') or (e['path'].startswith(PREFIX) and e['path'].endswith('.pcap')))]
    total=sum(e['size'] for e in selected)
    if not 3<=len(selected)<=128 or total>128*1024*1024:raise ValueError('Intake exceeds bounded file/byte budget')
    (out/'commit-metadata.json').write_text(json.dumps(commit,indent=2));(out/'repository-tree.json').write_text(json.dumps(tree,indent=2))
    files=[]
    for entry in selected:
        relative=entry['path'];target=out/relative
        if not target.resolve().is_relative_to(out.resolve()):raise ValueError('Escaping intake path')
        url=f'https://raw.githubusercontent.com/{REPO}/{revision}/'+quote(relative,safe='/')
        raw=target.read_bytes() if target.exists() else get(url)
        blob=hashlib.sha1(f'blob {len(raw)}\0'.encode()+raw).hexdigest()
        if len(raw)!=entry['size'] or blob!=entry['sha']:raise ValueError('File differs from pinned Git object: '+relative)
        target.parent.mkdir(parents=True,exist_ok=True)
        if not target.exists():target.write_bytes(raw)
        files.append(dict(path=relative,bytes=len(raw),gitBlobSha1=blob,sha256=hashlib.sha256(raw).hexdigest(),url=url))
        print(json.dumps(dict(file=relative,bytes=len(raw),gitBlobVerified=True)),flush=True)
    license_text=(out/'LICENSE').read_text()
    if 'MIT License' not in license_text or 'Copyright (c) 2024 HSNG' not in license_text:raise ValueError('Unexpected source licence')
    manifest=dict(schemaVersion=1,sourceId='hsngccu-wpa3-deauth-reference-v1',repository=f'https://github.com/{REPO}',revision=revision,
        repositoryDeclaredLicense='MIT',copyright='Copyright (c) 2024 HSNG',files=files,
        trainingApproved=False,releaseEligible=False,attackProgramsDownloaded=False,
        labelScope='Publisher Deauth directory describes collection condition, not independent per-packet incident truth.',
        recordingGrouping='Treat all selected chunks as one source campaign until documentation establishes independent sessions.',
        limitations=['No independently labelled benign control in this intake.','Repository licence declaration is preserved; task/source review remains required.',
        'Capture chunks are not independent environments or random train/test units.','Only observed fields can support sequence/identity claims.'])
    (out/'intake-manifest.json').write_text(json.dumps(manifest,indent=2));return manifest


if __name__=='__main__':
    m=fetch(Path(__file__).parent);print(json.dumps(dict(files=len(m['files']),bytes=sum(f['bytes'] for f in m['files']),revision=m['revision'],trainingApproved=False)))
