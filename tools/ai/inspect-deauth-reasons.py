"""Offline cross-check of unprotected deauth reasons in the two copied radiotap PCAPs."""
import json
import struct
from pathlib import Path

root=Path(__file__).resolve().parents[2]/'diagnostic-artifacts'/'field-review-2026-11-09'
for event in (2,3):
    path=root/f'hunter_event_{event}.pcap'
    if not path.exists():continue
    data=path.read_bytes()
    if data[:4]!=b'\xd4\xc3\xb2\xa1' or struct.unpack_from('<I',data,20)[0]!=127:
        raise ValueError('Expected a little-endian radiotap PCAP')
    offset=24;index=0;deauth=[]
    while offset<len(data):
        sec,micro,length,original=struct.unpack_from('<IIII',data,offset);offset+=16
        packet=data[offset:offset+length];offset+=length
        if len(packet)!=length:raise ValueError('Truncated PCAP record')
        rt=struct.unpack_from('<H',packet,2)[0]
        frame=packet[rt:]
        if len(frame)<24: index+=1;continue
        control=struct.unpack_from('<H',frame)[0]
        kind=((control>>2)&3)*16+((control>>4)&15)
        if kind in (0x0c,0x0a):
            mac=lambda b:':'.join(f'{v:02x}' for v in b)
            protected=bool(control&0x4000)
            reason=struct.unpack_from('<H',frame,24)[0] if len(frame)>=26 and not protected else None
            deauth.append(dict(frame=index,timeNanos=sec*1_000_000_000+micro*1000,source=mac(frame[10:16]),
                destination=mac(frame[4:10]),bssid=mac(frame[16:22]),protected=protected,reason=reason))
        index+=1
    print(json.dumps(dict(event=event,frames=index,disconnects=deauth),indent=2))
