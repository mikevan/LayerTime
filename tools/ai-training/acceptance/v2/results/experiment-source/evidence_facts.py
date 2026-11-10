"""Portable evidence summaries for local AI experiments; no attack classification.

Counts are observed frames, not unique transmissions, confirmed disconnects or
affected devices. Addresses are claimed identities. No signal absence inference.
"""
import math
import re
from collections import Counter, defaultdict

MAC = re.compile(r'^[0-9a-f]{2}(?::[0-9a-f]{2}){5}$')
REASONS = {4: 'Disassociated due to inactivity',
           7: 'Class 3 frame received from nonassociated STA'}


def address(value):
    if not isinstance(value, str): return None
    value = value.lower()
    return value if MAC.fullmatch(value) else None


def unicast(value):
    return value is not None and not int(value[:2], 16) & 1


def identity(row):
    bssid = address(row.get('network'))
    source, destination = address(row.get('source')), address(row.get('destination'))
    if not unicast(bssid): return None
    if source == bssid and destination == 'ff:ff:ff:ff:ff:ff':
        return bssid, destination, 'broadcast'
    if source == bssid and unicast(destination) and destination != bssid:
        return bssid, destination, 'ap-to-client'
    if destination == bssid and unicast(source) and source != bssid:
        return bssid, source, 'client-to-ap'
    return None


def time_value(row):
    value = row.get('timeNanos')
    if row.get('timestampValid') is not True or type(value) not in (int, float): return None
    return value if math.isfinite(value) else None


def summarize(observations, follow_on_ms=30000, burst_window_ms=3000, burst_min_frames=6):
    if type(follow_on_ms) is not int or not 0 < follow_on_ms <= 300000:
        raise ValueError('Invalid follow-on window')
    if type(burst_window_ms) is not int or not 1<=burst_window_ms<=3600000 or type(burst_min_frames) is not int or not 1<=burst_min_frames<=1000000:
        raise ValueError('Invalid burst policy')
    groups = defaultdict(lambda: dict(disconnect=[], eapol=[]))
    seen, rejected = set(), Counter()
    for row in observations:
        ref = row.get('frameId')
        if type(ref) is not int or ref < 0 or ref in seen:
            raise ValueError('Invalid or duplicate frame identity')
        seen.add(ref)
        if row.get('malformed') is not False:
            rejected['malformedOrUnknownValidity'] += 1; continue
        kind = row.get('typeSubtype')
        if type(kind) is not int:
            rejected['invalidSubtype'] += 1; continue
        # Only the already-decoded explicit EAPOL flag identifies EAPOL.
        category = 'disconnect' if kind in (10,12) else 'eapol' if row.get('eapol') is True else None
        if category is None: continue
        key = identity(row)
        if key is None or category == 'eapol' and key[2] == 'broadcast':
            rejected['unresolvedIdentity'] += 1; continue
        groups[key[:2]][category].append(dict(row, direction=key[2]))

    output = []
    for (bssid, client), values in sorted(groups.items()):
        disconnects, exchanges = values['disconnect'], values['eapol']
        all_rows = disconnects + exchanges
        valid_times = all(time_value(r) is not None for r in all_rows)
        following = []
        if disconnects and exchanges and valid_times and client != 'ff:ff:ff:ff:ff:ff':
            end = max(time_value(r) for r in disconnects)
            following = [r['frameId'] for r in exchanges if 0 < time_value(r)-end <= follow_on_ms*1_000_000]
        reasons = Counter(r.get('reasonCode') for r in disconnects if type(r.get('reasonCode')) is int)
        burst=None
        if disconnects and all(time_value(r) is not None for r in disconnects):
            ordered=sorted(disconnects,key=time_value);left=0;best=[]
            for right,row in enumerate(ordered):
                while time_value(row)-time_value(ordered[left])>burst_window_ms*1_000_000:left+=1
                if right-left+1>len(best):best=ordered[left:right+1]
            burst=dict(windowMs=burst_window_ms,minimumFrames=burst_min_frames,peakObservedFrames=len(best),
                observedSpanMs=(time_value(best[-1])-time_value(best[0]))/1_000_000,
                thresholdMet=len(best)>=burst_min_frames,frameIds=[r['frameId'] for r in best],
                significance='Repeated disconnect requests warrant attention under the supplied policy; this does not establish malicious intent or successful disconnection.')
        output.append(dict(bssid=bssid, client=client, targetKind='broadcast' if client == 'ff:ff:ff:ff:ff:ff' else 'individual',
            deauthenticationFrameIds=[r['frameId'] for r in disconnects if r['typeSubtype']==12],
            disassociationFrameIds=[r['frameId'] for r in disconnects if r['typeSubtype']==10],
            observedDisconnectFrameCount=len(disconnects), observedEapolFrameCount=len(exchanges),
            eapolFrameIds=[r['frameId'] for r in exchanges],
            directionCounts=dict(Counter(r['direction'] for r in disconnects)),
            reasonCodes=[dict(code=code, observedFrames=count, meaning=REASONS.get(code,'Not mapped by this reference subset')) for code,count in sorted(reasons.items())],
            disconnectBurstPolicy=burst,
            allRelevantTimestampsValid=valid_times,
            eapolAfterLastDisconnectWithinWindow=following,
            orderingStatus='unresolved-invalid-timestamps' if not valid_times else 'based-on-supplied-timestamps',
            sameClientOtherBssids=sorted({other for other,c in groups if c == client and other != bssid}) if unicast(client) else []))
    return dict(schemaVersion=1, followOnWindowMs=follow_on_ms, groups=output, excluded=dict(rejected),
        limitations=['Counts are observed frames; retransmissions are not deduplicated.',
                     'Disconnect requests do not confirm disconnection; EAPOL does not confirm reconnection or password compromise.',
                     'Addresses and reason values are claimed frame contents, not authenticated causes or senders.',
                     'No channel coverage or capture completeness is inferred.',
                     'Chronological links use one supplied page, exact BSSID/client identity and the configured window; they do not prove causation.',
                     'Groups are not segmented into separate episodes; page boundaries can omit related frames.'])
