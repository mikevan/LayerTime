"""Pinned intake of licensed BLE connection timing measurements, not attack labels."""
import hashlib
import json
from pathlib import Path
import requests
from fetch_public_data import download

COMMIT='c8382b311b1d8d382241305ef0f8a7d7319b70af'
REPO='juliankaroliny/InSecTT_BLE_Channel_Sniff_Dataset'


def main():
    root=Path(__file__).parent/'external-data/insectt-ble-v1.1.1'
    root.mkdir(parents=True,exist_ok=True)
    base=f'https://raw.githubusercontent.com/{REPO}/{COMMIT}/'
    notice=download(base+'README.md',root/'README.md')
    if 'CC-BY 4.0' not in (root/'README.md').read_text(encoding='utf-8'):
        raise ValueError('Expected source license notice missing')
    tree=requests.get(f'https://api.github.com/repos/{REPO}/git/trees/{COMMIT}',params={'recursive':'1'},timeout=30)
    tree.raise_for_status();metadata=tree.json()
    if metadata.get('truncated'):raise ValueError('Incomplete repository tree')
    (root/'git-tree.json').write_text(json.dumps(metadata,indent=2),encoding='utf-8')
    files=[notice]
    for item in metadata['tree']:
        relative=item['path']
        if not relative.startswith(('dataset_hackrf/','dataset_ubertooth/')) or not relative.endswith(('.csv','.json')):continue
        path=root/relative
        result=download(base+relative,path,expected_size=item['size'])
        raw=path.read_bytes()
        actual=hashlib.sha1(b'blob '+str(len(raw)).encode()+b'\0'+raw).hexdigest()
        if actual!=item['sha']:raise ValueError('Git blob mismatch')
        files.append(dict(result,file=relative,gitBlobSha1=actual))
    manifest=dict(schemaVersion=1,sourceId='insectt-ble-v1.1.1',commit=COMMIT,
        sourceUrl='https://zenodo.org/records/7152044',license='CC-BY-4.0',
        attribution='Julian Karoliny, Thomas Blazek, Hans-Peter Bernhard and Andreas Springer. InSecTT BLE Channel Sniff Dataset v1.1.1 (2022), DOI 10.5281/zenodo.7152044.',
        files=files,releaseEligible=False,trainingApproved=False,
        limitations=['Single BLE data channel 22; channel-hopping messages are incompletely observed.',
                     'Connection traffic with controlled devices and ambient office traffic, not advertisement-spam ground truth.',
                     'These files cannot be pooled directly with the Nordic-v3 advertisement feature corpus.',
                     'A received access address is not a device MAC address.',
                     'Measurements and sniffer runs are different observation conditions; no independent attack-performance claim.'])
    (root/'intake-manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    print(json.dumps(dict(files=len(files),bytes=sum(f['bytes'] for f in files),commit=COMMIT)))


if __name__=='__main__':main()
