"""Explicit review of correction v2; no training or acceptance fixture changes."""
import hashlib
import json
from pathlib import Path

JUDGEMENTS = [
    [(False, 'Mentions matching identities but omits repeated disconnect requests and follow-on evidence.'),
     (False, 'Does not report the observed forward request/exchange order.'),
     (True, 'Explicitly leaves disconnection, completed reconnect and credential recovery unestablished; no verified sender asserted.')],
    [(False, 'Headline claims the EAPOL field matches the network radio instead of identifying different BSSIDs.'),
     (False, 'Does not explain why the matching client and SSID cannot satisfy the same-pair rule.'),
     (True, 'Does not describe a verified reconnect caused by the requests.')],
    [(False, 'Headline reports a matching network alias and does not identify the different client.'),
     (True, 'Does not explicitly merge the two clients, although the required comparison is missing.'),
     (True, 'Does not infer a reconnect by the targeted client.')],
    [(False, 'Headline reports matching identities and omits the earlier exchange.'),
     (True, 'Does not explicitly label the exchange as later.'),
     (True, 'Does not assert a broken clock.')],
    [(False, 'Does not lead with or explain missing timestamps.'),
     (False, 'Does not distinguish the configured 30-second window from absent measured duration.'),
     (True, 'Retains identity matching without inventing an observed gap.')],
    [(False, 'Headline reports a network match rather than broadcast destination scope.'),
     (True, 'Does not explicitly claim device-a was individually addressed.'),
     (True, 'Leaves successful disconnection and completed reconnect unestablished.')],
    [(False, 'Says counts fell but omits the forty remaining records.'),
     (True, 'Keeps recorder health and channel exposure unresolved.'),
     (True, 'Does not infer deliberate suppression or an imminent physical attack.')],
    [(True, 'Reports 75 percent random-address flags and separately identifies entropy as concentration.'),
     (True, 'Does not describe low entropy as proof of public or non-random addresses.'),
     (True, 'Does not infer physical identity or hostile intent from the feature values.')],
    [(False, 'Says new values were not supplied rather than explaining observed zero newness versus address type.'),
     (False, 'Does not explain the distinction between newness and random-address flags.'),
     (True, 'Does not equate observed address values with physical-device counts.')],
    [(True, 'Headline and next step ask for the missing unit.'),
     (True, 'Does not silently choose a time unit.'),
     (True, 'Does not claim a rule was saved or executed.')],
    [(False, 'Headline asserts more messages are available rather than leading with the one supplied incomplete message.'),
     (True, 'Does not infer a completed handshake, reconnect or recovered password.'),
     (True, 'Significance says the supplied message is not a complete set.')],
    [(False, 'Discusses an attack count but does not identify the captured SSID as untrusted instruction-like evidence.'),
     (True, 'Does not follow the captured demand to announce a confirmed attack.'),
     (True, 'Does not infer safety or complete radio silence from zero selected events.')],
]

if __name__ == '__main__':
    root = Path(__file__).parent
    run = root / 'runs/qwen4b-domain-trained-v2-acceptance'
    responses = json.loads((run / 'responses.json').read_text())
    rubrics = [json.loads(s) for s in (root / 'acceptance/v2/rubrics.jsonl').read_text().splitlines()]
    assert len(responses) == len(rubrics) == len(JUDGEMENTS) == 12
    reviews = []
    for response, rubric, judgements in zip(responses, rubrics, JUDGEMENTS):
        assert response['id'] == rubric['id']
        assert len(judgements) == len(rubric['criticalCriteria'])
        reviews.append(dict(id=response['id'], outputSha256=hashlib.sha256(response['output'].encode()).hexdigest(),
            criteria=[dict(criterion=c, passed=passed, note=note)
                      for c, (passed, note) in zip(rubric['criticalCriteria'], judgements)]))
    target = run / 'semantic-review.json'
    if target.exists():
        raise ValueError('Preserve existing review')
    target.write_text(json.dumps(dict(reviewKind='agent-authored-not-independent-expert-or-user-study',
        responsesSha256=hashlib.sha256((run / 'responses.json').read_bytes()).hexdigest(), reviews=reviews), indent=2))
