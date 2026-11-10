"""Persist agent review of the exact completed diagnostic outputs; no automatic truth score."""
import hashlib
import json
from pathlib import Path

NOTES=[
 ('needs_revision','Treats disconnect requests as actual disconnections, then denies the explicitly supplied same-pair temporal link; adds unsupported absence claims and is too verbose.'),
 ('needs_revision','Headline distinguishes clients, but the meaning still suggests a reconnection from unrelated-client events without establishing the same client/network pair.'),
 ('needs_revision','Distinguishes networks in headline but reports a request duration using the whole sequence span, claims actual disconnections and misattributes the failure to time correlation.'),
 ('needs_revision','Calls an earlier relative timestamp invalid, treats requests as actual disconnections and fails to lead with the exchange preceding the requests.'),
 ('needs_revision','Changes a target into a sender, denies supplied identity and exchange evidence, and does not make missing timing the primary interpretation limit.'),
 ('needs_revision','Preserves broadcast destination but blurs the full sequence span with the request interval and speculates about reconnection without establishing the client link.'),
 ('acceptable_for_this_diagnostic','Retains the latency observation and lack of causal evidence. Suggested checks remain broad; this is not proof of field or user comprehension quality.'),
 ('needs_revision','Appropriately asks for recorder-health checks, but changes an observed activity drop into zero recorded activity.'),
 ('needs_revision','Adds unsupported absolutes that observations are unaltered and never filtered or labelled; the supplied definition only establishes an observed telemetry value rather than intent.'),
 ('needs_revision','Headline implies established connection although a client/network grouping does not by itself establish connection state.'),
 ('acceptable_for_this_diagnostic','Preserves that an observed exchange alone does not establish credential loss. The diagnostic does not independently qualify all credential-exposure guidance.'),
 ('acceptable_for_this_diagnostic','Distinguishes a policy review posture from intent or a probability; still requires product usability review.'),
 ('needs_revision','Expands the narrow supplied source fact into unsupported specific threat examples and intent wording; retrieved-source reference does not support every generated claim.'),
 ('needs_revision','Adds unsupported or incorrect claims about derivation from identity address and nondeterministic mapping; exceeds the supplied historical source fact.'),
 ('needs_revision','Still confuses entropy/newness with address type, treats a logged inter-arrival feature as seconds, calls variable timing regular and infers device stability without a baseline.'),
 ('needs_revision','Still confuses entropy with address randomness and infers a stable or single physical source; normality and absent-connection claims exceed the supplied features.'),
]


def record(root):
    run=root/'runs/qwen4b-domain-pilot-v3-base';path=run/'responses.json';out=run/'prose-review.json'
    if out.exists():raise ValueError('Preserve existing review')
    responses=json.loads(path.read_text());assert len(responses)==len(NOTES)==16
    reviews=[dict(id=r['id'],outputSha256=hashlib.sha256(r['output'].encode()).hexdigest(),
        reviewKind='agent-authored-not-independent-expert-or-user-study',status=status,note=note)
        for r,(status,note) in zip(responses,NOTES)]
    result=dict(responsesSha256=hashlib.sha256(path.read_bytes()).hexdigest(),reviews=reviews,
        structuralPassDoesNotEstablishProseAccuracy=True,releaseEligible=False,
        reportMetadataCorrection='Original report generic limitation says synthetic-only; this diagnostic also includes two real BLE feature windows and source-backed questions. Original report and experiment source are preserved unchanged.')
    out.write_text(json.dumps(result,indent=2));return result


if __name__=='__main__':print(json.dumps(record(Path(__file__).parent),indent=2))
