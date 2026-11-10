"""Copy a verified review snapshot to a new local ZIP; no upload or sync claim."""
import argparse
import hashlib
import json
import shutil
import zipfile
from pathlib import Path
from export_training_transparency import verify


def backup(archive,destination,document=None):
    verify(archive);destination.mkdir(parents=True,exist_ok=True)
    target=destination/(archive.name+'.zip')
    if target.exists():raise ValueError('Preserve the existing backup')
    manifest=json.loads((archive/'manifest.json').read_text())
    with zipfile.ZipFile(target,'x',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as output:
        for source in sorted(archive.rglob('*')):
            if source.is_file():output.write(source,source.relative_to(archive).as_posix())
    with zipfile.ZipFile(target) as saved:
        assert saved.testzip() is None
        for entry in manifest['files']+[manifest['index']]:
            assert hashlib.sha256(saved.read(entry['path'])).hexdigest()==entry['sha256']
        assert saved.read('manifest.json')==(archive/'manifest.json').read_bytes()
    result=dict(zip=str(target.resolve()),bytes=target.stat().st_size,sha256=hashlib.sha256(target.read_bytes()).hexdigest(),
        archiveManifestSha256=hashlib.sha256((archive/'manifest.json').read_bytes()).hexdigest(),
        archiveContentVerified=True,publicationStatus='local-backup-only',oneDriveSyncConfirmed=False)
    if document:
        copy=destination/(archive.name+'-report.md')
        if copy.exists():raise ValueError('Preserve the existing readable report')
        shutil.copy2(document,copy)
        assert copy.read_bytes()==document.read_bytes()
        result['readableReport']=str(copy.resolve())
    (destination/(archive.name+'-backup.json')).write_text(json.dumps(result,indent=2))
    return result


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--archive',type=Path,required=True)
    parser.add_argument('--destination',type=Path,required=True);parser.add_argument('--document',type=Path)
    args=parser.parse_args();print(json.dumps(backup(args.archive,args.destination,args.document),indent=2))
