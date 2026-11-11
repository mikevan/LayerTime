"""Copy only the requested app-private field-review output and capture, without shell text conversion."""
import argparse
import json
import subprocess
from pathlib import Path

parser=argparse.ArgumentParser()
parser.add_argument('--event',type=int,required=True)
args=parser.parse_args()
if args.event not in (2,3):raise SystemExit('This incident collector is limited to the two identified captures.')
root=Path(__file__).resolve().parents[2]
out=root/'diagnostic-artifacts'/'field-review-2026-11-09'
out.mkdir(parents=True,exist_ok=True)
base=[r'C:\Users\vange\AppData\Local\Android\Sdk\platform-tools\adb.exe','-s','R5CX62Y9ANH','exec-out','run-as','com.projectrevivesolutions.layerscope.diag','cat']
for source,target in [
    ('files/diagnostics/field-network-review.json',f'event-{args.event}-review.json'),
    (f'cache/evidence/hunter_event_{args.event}.pcap',f'hunter_event_{args.event}.pcap')]:
    with (out/target).open('wb') as stream:subprocess.run(base+[source],stdout=stream,check=True)
review=json.loads((out/f'event-{args.event}-review.json').read_text())
print(json.dumps(review,indent=2))
