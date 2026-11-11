"""Offline, bounded Nordic-v3 legacy BLE advertisement features. No capture/transmit tooling."""
import argparse
import collections
import hashlib
import json
import math
from pathlib import Path
import numpy as np

FEATURES=['log_observed_rate','unique_address_fraction','address_entropy','new_address_fraction',
          'payload_repeat_fraction','random_address_fraction','manufacturer_ad_fraction',
          'adv_ind_fraction','adv_nonconn_fraction','scan_response_fraction','adv_scan_fraction',
          'iat_cv','log_mean_iat','log_max_iat','mean_ad_data_length','ad_data_length_std']

def decode_advertisement(raw,linktype=272):
    if linktype!=272:return None,'unsupported_linktype'
    if len(raw)<26:return None,'short_record'
    if raw[3]!=3:return None,'unsupported_nordic_version'
    if raw[6]!=2:return None,'not_advertising_event'
    if len(raw)!=7+int.from_bytes(raw[1:3],'little'):return None,'record_length_mismatch'
    if raw[7]<10 or 7+raw[7]+6>len(raw):return None,'invalid_event_header'
    flags=raw[8]
    if not flags&1:return None,'crc_failed'
    if (flags>>4)&7:return None,'unsupported_phy'
    channel=raw[9]
    if channel not in (37,38,39):return None,'extended_advertising_channel'
    pdu=raw[7+raw[7]:]
    if pdu[:4]!=bytes.fromhex('d6be898e'):return None,'not_advertising_access_address'
    kind=pdu[4]&15;length=pdu[5]&63
    if len(pdu)<6+length+3:return None,'truncated_pdu'
    if kind not in (0,1,2,4,6):return None,'unsupported_advertising_pdu'
    if length<6:return None,'short_advertiser_address'
    payload=pdu[6:6+length]
    address=payload[:6]
    data=payload[6:] if kind!=1 else b''
    manufacturer=False;offset=0
    while offset<len(data):
        size=data[offset]
        if size==0:break
        if offset+1+size>len(data):return None,'invalid_ad_structure'
        manufacturer|=data[offset+1]==255
        offset+=1+size
    return {'address':address,'kind':kind,'random':bool(pdu[4]&64),'manufacturer':manufacturer,
            'data':data,'channel':channel,'signature':pdu},None

def summarize(records,seconds,previous_addresses):
    times=np.array([t for t,p in records],dtype=np.float64)
    packets=[p for t,p in records];n=len(packets)
    if not n:raise ValueError('Empty observation window')
    counts=collections.Counter(p['address'] for p in packets)
    probabilities=np.array(list(counts.values()),dtype=float)/n
    entropy=float(-(probabilities*np.log2(probabilities)).sum())
    normalized_entropy=entropy/math.log2(n) if n>1 else 0.0
    payloads={hashlib.blake2s(p['data'],digest_size=8).digest() for p in packets}
    iat=np.diff(times);mean=float(iat.mean()) if len(iat) else 0.0
    lengths=np.array([len(p['data']) for p in packets],dtype=float)
    values=[math.log1p(n/seconds),len(counts)/n,normalized_entropy,
            sum(p['address'] not in previous_addresses for p in packets)/n,
            1-len(payloads)/n,sum(p['random'] for p in packets)/n,
            sum(p['manufacturer'] for p in packets)/n]
    values.extend(sum(p['kind']==kind for p in packets)/n for kind in (0,2,4,6))
    values.extend([float(iat.std())/mean if mean>0 else 0.0,math.log1p(mean),
                   math.log1p(float(iat.max())) if len(iat) else 0.0,float(lengths.mean()),float(lengths.std())])
    return np.asarray(values,dtype=np.float32),set(counts)

def extract(path,seconds=10):
    from scapy.utils import RawPcapNgReader
    counters=collections.Counter();windows=[];previous=set();records=[];bucket=None;first_bucket=None
    seen=set();last_timestamp=None;first_time=None;last_time=None
    reader=RawPcapNgReader(str(path))
    with reader:
        for raw,meta in reader:
            counters['total_records']+=1
            timestamp=((meta.tshigh<<32)+meta.tslow)/meta.tsresol
            if last_timestamp is not None and timestamp<last_timestamp:raise ValueError('Nonmonotonic session timestamps')
            first_time=timestamp if first_time is None else first_time;last_time=timestamp;last_timestamp=timestamp
            current=math.floor(timestamp/seconds)
            if bucket is None:bucket=current;first_bucket=current
            if current!=bucket:
                if records:
                    vector,addresses=summarize(records,seconds,previous)
                    if bucket!=first_bucket:windows.append((bucket,vector,len(records)))
                    previous=addresses
                records=[];seen=set();bucket=current
            packet,reason=decode_advertisement(raw,meta.linktype)
            if reason:counters[reason]+=1;continue
            key=(timestamp,packet['channel'],packet['signature'])
            if key in seen:counters['duplicate_records']+=1;continue
            seen.add(key);records.append((timestamp,packet));counters['accepted_advertisements']+=1
    # The last bucket is incomplete. Gaps are not filled with false zero-activity windows.
    if not windows:raise ValueError('No full advertisement windows')
    return windows,dict(counters),{'firstTimestamp':first_time,'lastTimestamp':last_time,
        'durationSeconds':last_time-first_time,'coverage':'Only captured advertisements; continuous exposure is not established.'}

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--source',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    args.output.mkdir(parents=True,exist_ok=False)
    source_manifest=json.loads((args.source/'intake-manifest.json').read_text())
    if source_manifest.get('license')!='CC-BY-4.0':raise ValueError('Unexpected source license')
    sessions=[];all_x=[];all_y=[];groups=[]
    for path in sorted(args.source.rglob('*.pcapng')):
        benign='NOspam' in path.name or 'Baseline_NoAttacker' in path.name
        label=0 if benign else 1
        if not benign and not ('spam' in path.name or path.name.startswith('Attacker_')):raise ValueError('Unmapped session label')
        windows,counters,coverage=extract(path)
        session_id=hashlib.sha256(path.relative_to(args.source).as_posix().encode()).hexdigest()[:16]
        matrix=np.stack([v for t,v,n in windows]);all_x.append(matrix);all_y.extend([label]*len(windows));groups.extend([session_id]*len(windows))
        with path.open('rb') as stream:sha=hashlib.file_digest(stream,'sha256').hexdigest()
        sessions.append({'sessionId':session_id,'file':path.relative_to(args.source).as_posix(),'sha256':sha,
            'label':label,'labelMeaning':'benign-session' if benign else 'spam-present-session',
            'labelStatus':'publisher-session-label-not-individual-packet-ground-truth','windows':len(windows),
            'decoderCounts':counters,**coverage})
        print(json.dumps({'session':path.name,'windows':len(windows),'records':counters['total_records']}),flush=True)
    np.savez_compressed(args.output/'features.npz',x=np.concatenate(all_x),y=np.array(all_y),groups=np.array(groups))
    manifest={'schemaVersion':1,'featureVersion':'ble-observed-legacy-v1','windowSeconds':10,'features':FEATURES,
        'source':source_manifest,'sessions':sessions,'releaseEligible':False,
        'limitations':['Session labels indicate spam was present, not that every packet was malicious.',
            'Seven sessions from one testbed do not establish field generalization.',
            'RSSI, literal MACs, payload values, channels, file names and absolute timestamps are excluded from model inputs.',
            'First and last partial windows are excluded; empty intervals are not treated as radio silence.',
            'Sniffer encapsulation is removed; Hunter feature parity and packet decoder parity remain separate gates.']}
    (args.output/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')

if __name__=='__main__':main()
