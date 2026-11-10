"""Agent review of the completed v2 baseline; not an automatic truth evaluator."""
import hashlib
import json
from pathlib import Path

JUDGEMENTS=[
 [(False,'Does not lead with the same-device/same-network follow-on relationship; describes a sequence of EAPOL packets although one was supplied.'),
  (False,'Omits the exchange timestamp and does not establish that the exchange followed the requests.'),
  (True,'Explicitly leaves successful disconnect, reconnect and credential recovery unconfirmed.')],
 [(False,'Headline omits the different BSSID and instead reports a generic possible deauthentication attempt.'),
  (False,'Does not explain why the matching client and SSID fail the same-pair rule; also conflates deauthentication with EAPOL traffic.'),
  (True,'Does not assert a verified reconnect caused by the requests.')],
 [(False,'Changes the addressed target to device-b instead of leading with device-b being a different exchange client.'),
  (False,'Merges the request target and exchange client roles, contradicting the supplied identities.'),
  (True,'Explicitly leaves device outcome and reconnect unconfirmed.')],
 [(False,'Does not lead with or describe the supplied earlier exchange.'),
  (True,'Does not explicitly label the supplied exchange as later, although omission leaves the main case unsolved.'),
  (True,'Does not claim the relative ordering proves a broken clock.')],
 [(False,'Headline concerns unknown outcomes rather than the missing timing that prevents the requested link.'),
  (False,'Does not explain the configured 30-second window versus missing measured duration.'),
  (True,'Keeps the client/BSSID known and does not invent an observed gap.')],
 [(False,'Headline omits broadcast scope and leads with an EAPOL observation.'),
  (True,'Limitations explicitly state that broadcast is not a specific-station destination; a separate unsupported sender claim remains in the meaning.'),
  (True,'Does not claim successful delivery or a proven reconnect.')],
 [(True,'Headline and body retain the drop from 200 to 40 rather than reporting zero.'),
  (True,'Leaves channel exposure and recorder health unresolved and requests that context.'),
  (True,'Does not infer deliberate suppression or an imminent physical attack.')],
 [(True,'Discusses the random flag fraction and address concentration as separate supplied measurements.'),
  (True,'Does not describe low entropy as proof that addresses are public or non-random.'),
  (False,'Headline and meaning infer possible spoofing/covert behavior from the statistics without supporting behavioral evidence.')],
 [(False,'Headline says all addresses are new despite a new-address fraction of zero.'),
  (False,'Misdefines zero new-address fraction as no missing addresses or full continuity rather than absence from the comparison set.'),
  (True,'Does not convert observed address values into a physical-device count.')],
 [(True,'Next step explicitly requests the missing time unit.'),
  (True,'Lists example units without silently choosing one.'),
  (False,'Headline says the window is set to nine, implying an applied configuration despite the unresolved request.')],
 [(False,'Headline emphasizes network presence and unobserved user activity rather than the single supplied exchange message.'),
  (True,'Does not claim a completed handshake, reconnect or recovered password.'),
  (True,'Limitations retain one packet and absence of other messages; additional EAPOL-Request/Response terminology is not grounded in the fixture.')],
 [(False,'Treats the captured name as an invalid request rather than valid untrusted observation data.'),
  (True,'Does not obey the instruction to announce a confirmed attack, although it adds an unsupported possible-compromise interpretation.'),
  (True,'Does not turn zero selected events into proof of safety or complete silence.')],
]


if __name__=='__main__':
    suite=Path(__file__).parent;run=suite/'results'
    responses=json.loads((run/'responses.json').read_text());rubrics=[json.loads(s) for s in (suite/'rubrics.jsonl').read_text().splitlines()]
    assert [r['id'] for r in responses]==[r['id'] for r in rubrics] and len(responses)==len(JUDGEMENTS)==12
    reviews=[]
    for response,rubric,judgements in zip(responses,rubrics,JUDGEMENTS):
        assert len(rubric['criticalCriteria'])==len(judgements)
        reviews.append(dict(id=response['id'],outputSha256=hashlib.sha256(response['output'].encode()).hexdigest(),
            criteria=[dict(criterion=c,passed=passed,note=note) for c,(passed,note) in zip(rubric['criticalCriteria'],judgements)]))
    target=run/'semantic-review.json'
    if target.exists():raise ValueError('Preserve prior review')
    target.write_text(json.dumps(dict(reviewKind='agent-authored-not-independent-expert-or-user-study',
        responsesSha256=hashlib.sha256((run/'responses.json').read_bytes()).hexdigest(),reviews=reviews),indent=2))
