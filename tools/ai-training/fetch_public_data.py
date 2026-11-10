"""Version-pinned public corpus intake with streaming hashes and retained source notices."""
import argparse
import hashlib
import json
import time
from pathlib import Path
import requests

ROOT=Path(__file__).resolve().parent
TRENDYOL_REVISION='357544e7576607d88eaeac9b0adb07e9fd8bb2bb'
QWEN_REVISION='cdbee75f17c01a7cc42f958dc650907174af0554'
SMOLLM_REVISION='a07cc9a04f16550a088caea529712d1d335b0ac1'

def download(url,destination,expected_size=None,expected_sha=None):
    destination=Path(destination)
    destination.parent.mkdir(parents=True,exist_ok=True)
    if destination.exists():
        with destination.open('rb') as stream:sha=hashlib.file_digest(stream,'sha256').hexdigest()
        if expected_size is not None and destination.stat().st_size!=expected_size:raise ValueError('Existing file size mismatch')
        if expected_sha is not None and sha!=expected_sha:raise ValueError('Existing file hash mismatch')
        return {'file':destination.name,'bytes':destination.stat().st_size,'sha256':sha,'url':url}
    pending=destination.with_suffix(destination.suffix+'.partial')
    sha=hashlib.sha256();size=0
    if expected_size and expected_size>16*1024*1024:
        if pending.exists():
            size=pending.stat().st_size
            if size>expected_size:raise ValueError('Partial file exceeds expected size')
            with pending.open('rb') as stream:
                for chunk in iter(lambda:stream.read(1024*1024),b''):sha.update(chunk)
        with pending.open('ab') as stream:
            reported=time.monotonic()
            while size<expected_size:
                end=min(size+8*1024*1024,expected_size)-1
                for attempt in range(4):
                    try:
                        # Explicit ranges avoid long CDN/proxy buffering and permit restart.
                        with requests.get(url,headers={'Range':f'bytes={size}-{end}'},timeout=(20,40)) as response:
                            response.raise_for_status()
                            if response.status_code!=206 or response.headers.get('Content-Range')!=f'bytes {size}-{end}/{expected_size}':
                                raise ValueError('Server did not honor the requested byte range')
                            chunk=response.content
                            if len(chunk)!=end-size+1:raise ValueError('Incomplete download range')
                        break
                    except requests.RequestException:
                        if attempt==3:raise
                        time.sleep(1+attempt)
                stream.write(chunk);stream.flush();sha.update(chunk);size+=len(chunk)
                if time.monotonic()-reported>25:
                    print(json.dumps({'file':destination.name,'downloadedBytes':size,'totalBytes':expected_size}),flush=True)
                    reported=time.monotonic()
    else:
        with requests.get(url,stream=True,timeout=(20,40)) as response:
            response.raise_for_status()
            with pending.open('wb') as stream:
                for chunk in response.iter_content(1024*1024):
                    if chunk:stream.write(chunk);sha.update(chunk);size+=len(chunk)
    if expected_size is not None and size!=expected_size:raise ValueError('Downloaded file size mismatch')
    if expected_sha is not None and sha.hexdigest()!=expected_sha:raise ValueError('Downloaded file hash mismatch')
    pending.replace(destination)
    print(json.dumps({'downloaded':destination.name,'bytes':size}),flush=True)
    return {'file':destination.name,'bytes':size,'sha256':sha.hexdigest(),'url':url}

def main():
    parser=argparse.ArgumentParser();parser.add_argument('source',choices=['trendyol','qwen4b','smollm3','ble'])
    args=parser.parse_args()
    if args.source=='ble':
        directory=ROOT/'external-data/ble-ntmmcw6jj8-v2';directory.mkdir(parents=True,exist_ok=True)
        snapshot=requests.get('https://data.mendeley.com/public-api/datasets/ntmmcw6jj8/snapshot/2',timeout=40)
        snapshot.raise_for_status();metadata=snapshot.json()
        if metadata.get('version')!=2 or metadata.get('id')!='ntmmcw6jj8':raise ValueError('Wrong dataset version')
        license_info=metadata.get('licence',{})
        if 'creativecommons.org/licenses/by/4.0' not in license_info.get('url',''):raise ValueError('Unexpected BLE source license')
        (directory/'publisher-metadata.json').write_text(json.dumps(metadata,indent=2),encoding='utf-8')
        result=requests.get('https://data.mendeley.com/public-api/datasets/ntmmcw6jj8/files',
                            params={'folder_id':'root','version':2,'$start':0,'$limit':1000},
                            headers={'Accept':'application/vnd.mendeley-public-dataset.1+json'},timeout=40)
        result.raise_for_status();files=result.json()
        (directory/'publisher-files.json').write_text(json.dumps(files,indent=2),encoding='utf-8')
        item=next(x for x in files if x['filename']=='BLE_Datasets.zip');content=item['content_details']
        sha='9c6e1318591403757037e0ea74f48e7c6d0b310449fe0fe9daf8b8cc6da90d66'
        if content['sha256_hash']!=sha:raise ValueError('Publisher file hash differs from pinned version')
        record=download(content['download_url'],directory/item['filename'],content['size'],sha)
        (directory/'intake-manifest.json').write_text(json.dumps({'schemaVersion':1,'sourceId':'ble-ntmmcw6jj8-v2',
            'version':2,'license':'CC-BY-4.0','doi':metadata['doi'],'files':[record]},indent=2),encoding='utf-8')
        print(json.dumps({'source':'ble-ntmmcw6jj8-v2','manifest':str(directory/'intake-manifest.json')}),flush=True)
        return
    if args.source=='trendyol':
        repo='Trendyol/Trendyol-Cybersecurity-Instruction-Tuning-Dataset';revision=TRENDYOL_REVISION
        directory=ROOT/'external-data/trendyol'/revision
        prefix='https://huggingface.co/datasets/'+repo
        files=['README.md','CyberSec-Dataset_escaped.jsonl']
        api='https://huggingface.co/api/datasets/'+repo+'/revision/'+revision+'?blobs=true'
    elif args.source=='smollm3':
        repo='HuggingFaceTB/SmolLM3-3B';revision=SMOLLM_REVISION
        directory=ROOT/'models/smollm3-3b'
        prefix='https://huggingface.co/'+repo
        files=['README.md','chat_template.jinja','config.json','generation_config.json','special_tokens_map.json',
               'tokenizer.json','tokenizer_config.json','model.safetensors.index.json',
               'model-00001-of-00002.safetensors','model-00002-of-00002.safetensors']
        api='https://huggingface.co/api/models/'+repo+'/revision/'+revision+'?blobs=true'
    else:
        repo='Qwen/Qwen3-4B-Instruct-2507';revision=QWEN_REVISION
        directory=ROOT/'models/qwen3-4b-instruct-2507'
        prefix='https://huggingface.co/'+repo
        files=['LICENSE','README.md','config.json','generation_config.json','merges.txt','vocab.json',
               'tokenizer.json','tokenizer_config.json','model.safetensors.index.json',
               'model-00001-of-00003.safetensors','model-00002-of-00003.safetensors','model-00003-of-00003.safetensors']
        api='https://huggingface.co/api/models/'+repo+'/revision/'+revision+'?blobs=true'
    response=requests.get(api,timeout=30);response.raise_for_status();metadata=response.json()
    if metadata.get('sha')!=revision:raise ValueError('Repository revision differs')
    license_name=metadata.get('cardData',{}).get('license')
    if license_name!='apache-2.0':raise ValueError('Unexpected source license')
    directory.mkdir(parents=True,exist_ok=True)
    (directory/'hub-metadata.json').write_text(json.dumps(metadata,indent=2),encoding='utf-8')
    siblings={item['rfilename']:item for item in metadata['siblings']}
    records=[]
    for name in files:
        item=siblings[name];lfs=item.get('lfs',{})
        records.append(download(prefix+'/resolve/'+revision+'/'+name,directory/name,
                                item.get('size'),lfs.get('sha256')))
    manifest={'schemaVersion':1,'sourceId':repo,'revision':revision,'license':license_name,
              'purpose':'Audit and development training; not a qualified product model','files':records}
    (directory/'intake-manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    print(json.dumps({'source':repo,'revision':revision,'manifest':str(directory/'intake-manifest.json')}),flush=True)

if __name__=='__main__':main()
