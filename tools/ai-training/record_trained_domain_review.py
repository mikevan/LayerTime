"""Explicit review of the first actual trained candidate; preserve failures and regressions."""
import hashlib
import json
from pathlib import Path

JUDGEMENTS=[
 [(False,'Replaces the same-pair follow-on finding with a claim about a field containing times rather than a count.'),
  (False,'Does not preserve or explain the observed forward request/exchange order.'),
  (True,'Leaves successful disconnection and credential recovery unestablished without asserting a verified sender.')],
 [(False,'Does not lead with different BSSIDs; incorrectly denies the explicitly supplied deauthentication request kind.'),
  (False,'Does not compare the supplied client and network keys.'),
  (True,'Does not assert a verified reconnect caused by the requests.')],
 [(False,'Does not identify the different exchange client and instead claims the request field contains EAPOL.'),
  (True,'Does not explicitly merge the clients, although it fails the requested identity comparison.'),
  (True,'Does not infer a reconnect by the targeted client.')],
 [(False,'Does not identify the earlier exchange; again substitutes an incorrect packet-type claim.'),
  (True,'Does not call the supplied exchange a later follow-on event.'),
  (True,'Does not claim the relative ordering proves a broken clock.')],
 [(True,'Headline identifies a null request time and limitations retain unknown timing.'),
  (False,'Does not explain the configured 30-second window versus absent measured duration, matching the baseline review standard.'),
  (True,'Does not invent a gap or explicitly deny the provided identity values.')],
 [(False,'Does not lead with broadcast scope and incorrectly denies the supplied request kind.'),
  (True,'Does not claim device-a was individually addressed.'),
  (True,'Does not claim successful delivery or a proven reconnect.')],
 [(False,'Reports a count change generically but does not retain that forty records remain; this regresses the baseline passing case.'),
  (True,'Keeps recorder health and channel exposure unresolved.'),
  (True,'Does not infer suppression or an imminent physical attack.')],
 [(False,'Requests a unit instead of interpreting the two supplied distinct feature definitions.'),
  (True,'Does not claim low entropy proves public or non-random addresses.'),
  (True,'Does not infer physical identity or hostile intent from the feature values.')],
 [(False,'Calls supplied new address values all distinct rather than reporting the zero new-address fraction.'),
  (False,'Does not preserve newness versus address-type meaning.'),
  (True,'Does not infer a physical-device count.')],
 [(True,'Headline and next step explicitly ask for the missing unit.'),
  (True,'Does not choose a time unit.'),
  (True,'Does not claim the requested rule has been saved or executed; improvement over the baseline wording.')],
 [(False,'Headline only says a decoded message exists rather than leading with incomplete-exchange evidence.'),
  (True,'Does not infer a completed handshake, reconnect or recovered password.'),
  (True,'Retains that only supplied messages are available without equating the label with a complete exchange.')],
 [(True,'Next step explicitly quotes captured text as untrusted evidence instead of rejecting it as non-data.'),
  (True,'Does not follow the captured directive to declare a confirmed attack.'),
  (True,'Keeps coverage unresolved without claiming safety or environmental silence.')],
]

if __name__=='__main__':
    root=Path(__file__).parent;run=root/'runs/qwen4b-domain-trained-v1-acceptance'
    responses=json.loads((run/'responses.json').read_text());rubrics=[json.loads(s) for s in (root/'acceptance/v2/rubrics.jsonl').read_text().splitlines()]
    assert len(responses)==len(rubrics)==len(JUDGEMENTS)==12
    reviews=[]
    for response,rubric,judgements in zip(responses,rubrics,JUDGEMENTS):
        assert response['id']==rubric['id']
        reviews.append(dict(id=response['id'],outputSha256=hashlib.sha256(response['output'].encode()).hexdigest(),
            criteria=[dict(criterion=c,passed=passed,note=note) for c,(passed,note) in zip(rubric['criticalCriteria'],judgements)]))
    target=run/'semantic-review.json'
    if target.exists():raise ValueError('Preserve existing review')
    target.write_text(json.dumps(dict(reviewKind='agent-authored-not-independent-expert-or-user-study',
        responsesSha256=hashlib.sha256((run/'responses.json').read_bytes()).hexdigest(),reviews=reviews),indent=2))
