# Field evidence agreement

A warning is harder to trust when its supporting record exists in only one place. Independent records let people check a finding against the underlying observations. This agreement defines what Hunter, Vixen, and T-Ultra must preserve and compare.

Owner requirement, 2026-11-13: Hunter and Vixen operate together in real time.
T-Ultra is an independent verifier. Its SD card should contain nearly the same
observations as Hunter's SD card and the evidence Vixen is analyzing.

## What must be demonstrated

1. Hunter's saved observations and Vixen's received observations retain their
   source identity, producer session and event identity. UI grouping must not
   replace the underlying individual records used for comparison.
2. Repeats, updated counts, source/detail text, disconnects, change-stream gaps,
   capture truncation and missing frames must remain visible in the test record.
3. Vixen must identify which observations or raw evidence it is analyzing and
   how current that input is. An old imported capture must not appear to be the
   current Hunter's evidence. A result must retain its source/session identity.
4. T-Ultra records its own observations. Compare common radio/detector coverage
   by source identifier, detector, channel/band and an explicitly aligned time
   interval. Its local event IDs and uptimes cannot be compared directly with
   Hunter's. Counts and RSSI need not be identical for independent receivers.
5. Preserve the original device files. A comparison report must distinguish
   missing observations, unsupported coverage, different counts, uncertain
   timing, transport loss and logging failure. It must not silently discard
   differences to manufacture agreement.
6. Record latency as a distribution and worst case for receipt, evidence
   transfer and analysis separately. Do not describe a manual capture import or
   a downloaded evidence window as continuous real-time analysis.
7. Model explanations remain separate from observed facts. The current two
   domain candidates each passed only 2/12 frozen interpretation cases and are
   not qualified for ordinary assessments. Preserve those results and gates.

No quantitative RF agreement or latency threshold has yet been established by
a controlled device run. Report measured results before deciding field success.

## Initial source audit

| Component | Current finding | Needed for this test |
|---|---|---|
| Hunter | SD log includes boot, uptime, event ID, count, source and evidence-window details | Preserve this as the producer record; verify writes and drops |
| Vixen | Receives individual summaries, then groups UI rows by source address; diagnostic history is bounded | Retain an exportable, ungrouped observation record and transport gaps |
| Vixen raw evidence | GET_FRAMES / GET_FRAME_BYTES is an on-demand window pull | Bind each analyzed window to Hunter session/event and report capture/receipt time separately |
| T-Ultra | `/recon_log.csv` records new detections only, without event ID, repeats or count | Add a versioned comparable recording without mixing schemas in an existing card file |
| T-Ultra CSV | Unquoted source/detail fields can shift columns | Preserve untrusted text safely and reject incomplete rows |
| Time | Hunter log uses uptime; watch log uses a cached wall clock; phone PCAP uses download-completion wall time plus trigger offsets | Make clock origins explicit; never infer synchronized capture time from those values |

This audit is not a field-readiness claim. No devices were flashed and no
real-device parity or AI accuracy has been demonstrated by it.

## Implemented field instrumentation

The initial audit above describes the starting point. The field branches now
retain individual Hunter summaries and text in Vixen's append-only journal,
bind deterministic analysis and evidence downloads to their source/session,
and record transport gaps and storage failures. Debug Vixen can request a
manual, explicitly unqualified model explanation of a frozen individual
observation; its exact input and raw response are retained. This does not
change qualification results, alerts, or observed facts.

T-Ultra writes a separate versioned `/recon_field_v1.csv`, including local
event IDs, repeat snapshots, counts and explicit clock origins. Repeat writes
are throttled to ten seconds per event; the file is an observation journal,
not a packet mirror or a continuously identical count snapshot. The final
count of an event that stops during that interval can differ. SD short writes
are reported. Hunter's field update additionally attempts to flush sparse
queued records after two seconds; card access failures and file transfers can
delay persistence. These are implementation intervals, not measured latency
guarantees. Measure their impact before claiming the owner's real-time goal.

Both watch targets built successfully and the host preflight passed 29 groups.
Hunter's timed-flush host suite passed 529 checks. Vixen's full/debug and
Play/release unit suites passed with field instrumentation; these checks do
not establish agreement between physical receivers.

The identified Hunter card recordings were backed up and the authorized clear
was verified by a USB inventory containing only a fresh log. T-Ultra's field
firmware upload passed flash verification, and the owner confirmed normal boot,
card ready and SD logging enabled. No watch SD readback or three-device parity
result has yet been obtained. Keep private backups and run logs outside Git.

The subsequent USB bench check installed Vixen build 124 and verified its
native decoder and deterministic evidence analysis with the explicitly labeled
six-packet practice capture. Hunter was started in passive ALL mode; its live
diagnostics showed received Wi-Fi frames and BLE advertisements, no detector
matches, and zero queued/dropped records or SD write errors at the sampled
checkpoint. A Vixen export preserved 31 connection/hello/status records with
zero pending, dropped or failed writes. The live records therefore verify
status recording and reconnect, not event-summary delivery, live AI explanation,
RF detector agreement, or watch SD parity. No synthetic practice packet was
counted as a live detection. Both firmware uploads passed flash verification.

Use [the comparison tool](../../tools/field/README.md) on the three preserved
records. Record a common coverage/time interval, losses, reconnect behavior and
observed latencies. Do not treat build success as field acceptance.
