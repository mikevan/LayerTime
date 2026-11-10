# Builder behavioral reporting — implementation and verification

Updated November 9, 2026.

## Purpose

LayerTime is a defensive Wi-Fi/BLE analysis platform for civilian self-protection. The Builder lets quasi-technical operators express, test, inspect and exchange their analysis. Packet decoding and correlation support an operator's assessment of attack posture; operators decide what the observations mean for their circumstances.

Wireshark is one tool. It decodes packets and supplies packet checks. Behavior Timeline is a separate tool that correlates observations over time. Further tools can participate through the same workflow registry and typed ports. This release implements Wi-Fi capture-local behavior, not persistent trend storage or BLE correlation.

## Implemented

- Capture imports retain compact analytic observations independently of the 500-full-packet-tree display cache. The current analytic memory ceiling is 200,000 observations; exceeding it is explicitly reported. Decoding/counting continues beyond that ceiling.
- Findings carry capture identity, analyzed/decoded counts, cancellation/read-error status, malformed-frame counts and missing timestamps. Availability does not establish continuous channel coverage.
- The Capture source supplies all available observations. Detection supplies a time window; its after-window is extended to at least the downstream follow-on duration. A source window cannot manufacture unavailable observations.
- Engine features use the actual Builder block ID and retain the check type. Shipped default rules bind their canonical checks to those block IDs. Explicit user rules keep their own references. Multiple reporting blocks resolve in execution order.
- Timeline groups disconnects by client and network, normalizes case, and separates disconnect episodes after a configurable gap (default 3 seconds). Broadcast/unresolved endpoints do not acquire a particular client's EAPOL evidence. Malformed/untimed observations cannot establish temporal correlation.
- Subsequent EAPOL, authentication and association/reassociation observations must involve that same pair. Follow-on evidence is assigned once to the nearest preceding disconnect episode, within the configurable follow-on window (default 30 seconds). This describes temporal association, not successful disconnection, causal attribution, a complete handshake, or compromise.
- Target reports retain timestamps, frame positions, EAPOL delays and whether the recording's timestamp range reaches the follow-on horizon. That flag never means continuous capture coverage.
- Timeline provides structured target reports plus `episodes`, `linked_episodes`, `disconnect_frames`, `eapol_frames`, and `targets` metrics. Client/network selectors can restrict its reports. Rule JSON can test numeric metrics, compose all/any/not, and fill metric sentence blanks. Missing checks under `not` do not automatically become supporting evidence.
- Custom rules can supply a `label` for the assessment heading. Existing enum identifiers and historical verdict strings remain readable for compatibility, while default display labels and explanations describe posture and evidence. A missing matching rule becomes review/insufficient evidence.
- Rule validation catches unconnected check references and rules after an unconditional match. Numeric settings have integer/range validation; address selectors validate MAC syntax.
- The older aftermath check no longer counts every unrelated EAPOL in the window. Its same-network-name observations remain observations, not authenticated attribution. Some legacy detector heuristics still need field calibration.
- Builder supports text/rule settings, direct numeric entry, tap-to-choose Wireshark checks, rename, bounded undo/redo, capture execution, import and Android share-sheet export. Imports open as drafts without selecting them for execution. Each save retains an independently stored version; name hashes distinguish otherwise colliding file-name slugs. Editing state is retained while visiting tool screens, but unsaved state is not promised across process termination.
- Workflow execution runs off the UI thread. Replaced/cleared runs cannot publish stale results. Reports expose episode evidence, and Wireshark can reread selected packets beyond the initial display cache, keeping at most 500 full trees per selection. Large evidence selections explicitly state that display limit.
- `Wireshark and behavior posture` is a shipped combined example. Its posture depends on same-pair Timeline correlation. The packet-rate check is supplementary and does not strengthen a different target's assessment. The original Deauth triage remains available.

## Use on the phone

1. Import a capture through Tools/Wireshark (or retrieve Hunter evidence using the existing path).
2. Open Builder and find **Wireshark and behavior posture** under valid workflows.
3. Open it, then Copy to edit. Tap Wireshark/Timeline/report blocks to inspect or configure settings. Use the visual report-rule editor to select checks, Timeline counts and thresholds, compose All/Any/Not, reorder rules, and write headings and explanations. Advanced portable text remains available for deeper edits.
4. Tap Run capture. When analysis completes, tap the report block. Read the assessment, per-target episodes and coverage limitations; inspect an episode's packet evidence.
5. Rename, Save, reopen and compare. Share opens Android's chooser; importing a traded file opens it for review.

The Check button compares compatibility with the seven original deauth example scenarios. It is not a general certification of any custom workflow. The combined behavior example has its own regression tests.

## Validation and remaining work

JVM tests cover full analysis beyond the 500-tree display limit, the analytic ceiling, mixed targets/networks, direction, case normalization, window boundaries, separated episodes, missing timestamps/malformed packets, duplicate blocks, metric rule round trips, missing evidence under negation, import bounds/round trips, and unrelated traffic exclusion in the shipped combined workflow. Android diagnostic APK builds successfully.

No Android device was attached when device verification was attempted. Touch layout, keyboard interaction, gesture undo, share-sheet permissions, saved-version reopening, packet-detail rereading and live Hunter use still require verification on the Flip6. Do not treat this build as device-signed-off. No Bluetooth/GATT or Hunter firmware edits were made in this implementation pass; pre-existing working-tree changes in those areas remain intact.

Follow-up scope: general scoped packet-selection blocks, persistent/baseline trend analytics, BLE observations and identity rules, multilingual report templates, and multi-Hunter clock/capture correlation. Do not merge separate recordings or clock domains implicitly. Custom rules that combine aggregate checks across multiple targets need explicit scope; the combined example avoids cross-target inference.


## Continuation: touch report authoring

The report block now has a visual rule editor using the existing portable rule format. It supports check results, check text, selected-check counts, matching-frame counts, Timeline metrics, nested All/Any/Not, explicit fallback rules, operator-defined headings and explanations, and reorder/remove actions. New conditional rules are inserted before an existing unconditional fallback. Advanced text is optional; imported complex rules remain intact. Visual editing stops at six nested levels and preserves deeper rules for Advanced text.

Frame-count rules now require a frame connection to the report, including when nested. Tests rebuild a custom report through the same immutable edits as the touch controls, serialize/import it, and run it against matching and unrelated target observations. Stale background result publication is serialized with invalidation; cleared or replaced runs cannot overwrite a newer report.

The Android build and 270 JVM tests pass after this continuation. No phone is connected, so the new touch editor has not been visually or interactively verified on the Flip6. The next device check is: open/copy the combined example, edit a Timeline threshold and report sentence visually, add/reorder an All/Any/Not rule, save/reopen, run a capture, inspect supporting evidence, and export/reimport the flow.


## Build 110: operator-verifiable practice test

Version code 110 / 0.1.109-diag adds Builder > Practice test. Load practice capture generates a six-packet offline PCAP and sends it through the normal Wireshark importer. It replaces only the current imported recording. Training provenance is carried into findings and displayed in Wireshark and Timeline; no packets are transmitted. The fixture now writes a structured RSN EAPOL-Key descriptor instead of an unspecified all-zero descriptor.

The practice network is 02:00:00:00:00:01; the target client is 02:00:00:00:00:02; the distractor is 02:00:00:00:00:03. Frames 1/2 are disconnects at seconds 1/2, frame 3 is target EAPOL at second 3, frame 4 is distractor EAPOL at second 4, frame 5 is a target disconnect at second 10, and frame 6 is reverse-direction target EAPOL at second 11. The expected episode evidence is [1,2,3] and [5,6]; frame 4 is excluded. Expected counts: two disconnect episodes, two linked episodes, two related EAPOL frames.

Open reference workflow opens a read-only Capture + Wireshark rate + Timeline + report wiring. Copy makes it editable. The report's first rule uses Timeline linked_episodes >= 2 and heading TWO LINKED EPISODES. Change the threshold to 3 and rerun: heading THRESHOLD NOT MET. Change back to 2 and rerun: TWO LINKED EPISODES returns. Inspect packet evidence to verify the distractor was excluded. The panel also explains how to rebuild the same capability from a blank Builder without JSON.

Automated tests cover exact metrics, episode frame references, the 2/3/2 threshold experiment, generated PCAP record layout and RSN descriptors, training provenance, and workflow exchange. Passing JVM tests does not establish that native Wireshark decoding or touch interactions have been verified on the phone; the practice test is designed to exercise those remaining boundaries directly.


## Build 111: Run capture crash fix and Flip6 verification

The Flip6 crash log showed ExceptionInInitializerError in Summary caused by Android ICU rejecting an unescaped closing brace in the placeholder regex. Both literal braces are now escaped. Desktop JVM matching had accepted the old pattern, so a device regression was added rather than relying only on those tests.

The Android test runner is now BuilderSmokeInstrumentation (a dependency-free Instrumentation subclass in androidTest). Build/install app-full-debug.apk and app-full-debug-androidTest.apk, then run `adb -s R5CX62Y9ANH shell am instrument -w com.projectrevivesolutions.layerscope.diag.test/com.projectrevivesolutions.layertime.controller.BuilderSmokeInstrumentation`. Its result bundle reports PASS/FAIL for Android template matching, native capture decoding and the 2/3/2 posture test; it is a smoke runner rather than an AndroidX/JUnit runner.

Verified on the connected SM-F741W Flip6: templates PASS; native Wireshark decoded six packets, three EAPOL and zero malformed; exact two-episode/two-related-EAPOL finding PASS; unrelated fourth packet excluded; changing threshold 2 to 3 and back to 2 PASS. Build 111 (0.1.110-diag) was installed over build 110 preserving app data. The actual Builder UI was then used to load the practice recording, open the reference and press Run capture; the app remained running. Full visual/gesture verification of every editor action remains separate.


## Build 112: question-first design iteration

The operator's feedback was that the canvas was unintuitive, deauth packets were not explicitly identified, and the report text was too technical. The first screen now starts with a question: Was a device told to leave a Wi-Fi network? Try the practice example loads the labeled six-packet recording and runs a ready-made analysis without requiring wiring. Custom tools and the canvas remain available through an explicit custom-builder path.

The guided view exposes evidence, tap-to-add Wireshark/Timeline tools, report settings and Run analysis. Tap-to-add only connects an unambiguous single source/report and does not replace existing input connections. Report results appear directly in the Builder. Technical packet statistics and capture limits are expandable.

Target reports preserve separate deauthentication/disassociation frame references and deauth direction. The reading layer groups episodes by network and device and states the MAC address, number of deauth packets and later EAPOL setup messages, followed by potential significance and a threat-posture recommendation. Client-originated deauth is not described as an attack directed at that client. Packet buttons explicitly identify deauth and connection-setup/EAPOL evidence.

The default question report describes disconnect requests and later setup messages; it does not claim an observed successful disconnect/reconnect solely from deauth and EAPOL. The user's suggested explanation is the model for the reading layer: subject, sequence, consequence, action. Password-risk wording is conditional because it depends on network security and a usable captured exchange. Packet capture interpretation reference: https://documentation.meraki.com/Wireless/Troubleshooting_and_Support/Troubleshooting/Analyzing_Wireless_Packet_Captures (deauthentication, association, and successful WPA authentication sections).

Build 112 (0.1.111-diag) was installed over build 111. JVM tests cover guided connections, placeholder assignment, explicit deauth counts, distractor exclusion, direction-sensitive reporting and the question-analysis workflow. On the Flip6, native decoding and the threshold 2/3/2 smoke regression still pass. Final visual review was interrupted by the phone's lock screen; the user was asked to unlock it. This is a first design iteration for operator review, not a claim of audience usability validation.


After the user unlocked the Flip6, the build 112 UI was checked directly: Builder opened on the plain question screen; Try the practice example loaded and ran the recording with one tap; the report displayed client 02:00:00:00:00:02, three deauth packets across two bursts, two later EAPOL messages, the significance paragraph and threat-posture recommendation. Show deauth packets (3) opened Wireshark showing three selected frames; the packet list explicitly labeled frame 1 Deauthentication (0x000c). The phone was returned to the Builder report for user review. This confirms the interaction path and visible text, not audience comprehension; operator feedback remains the next design input.


## Build 113: action first

The user clarified that a person may have very little time to assess a warning. Report order is now: training marker when applicable, recommended action, one short plain-language reason, expandable observations, then expandable technical details and recording limits. MAC addresses, packet counts and longer risk explanations no longer precede the recommendation.

Installed build 113 (0.1.112-diag) on the Flip6. JVM build/tests and the native Android smoke regression pass. The one-tap practice UI was run and its visible first screen confirmed: Increase your threat posture and review this activity, followed by the short reason, What was observed?, and Technical details and recording limits. Both action and reason are visible together at the phone's current text size. The practice warning remains explicitly marked as training. This order is a presentation change, not an assertion of a predictable time-to-attack from radio observations.


## Builds 114–115: shared Back navigation and frame rails

All phone/tablet routes now use a shared sci-fi section housing with Back controls at its top and bottom. Navigation history records the actual opening path, so nested tools return to Builder, Timeline, Events or Tools as appropriate. Device Back is consumed at the root rather than minimizing the app. Builder question/tool selection, report expansions and advanced-rule text close locally before their enclosing route. Hunter-card log selection returns to its list first. Back during Hunt invokes the existing Stop Hunt callback, returning to the previous application view without disconnecting the Hunter. The hardware watch retains its separate layout and physical face-parking handler.

The same frame material is used around the report and the card-clear confirmation. Clear Card still requires its explicit Erase action; Back/Cancel dismisses it. The confirmation and Wireshark selection windows have local Back controls.

The user clarified that Back must be part of the frame, not a box inside it. Build 115 (0.1.114-diag) uses attached notched rail tabs at the outer top/bottom edges. A press triggers a brief cyan double pulse, a haptic virtual-key tick and a 70 ms two-note system-stream blip when UI sounds are enabled. Device sound volume/settings are not changed, and navigation does not wait for the effect. Pulse state carries across the route transition.

284 JVM tests pass, including nested return paths, duplicate navigation, root Back, Events/tool return paths and bounded history. On the Flip6, device Back returned Builder to Ops and kept the app foreground on another Back at root; both top and bottom controls returned Wireshark to Tools. The shared housing and rail placement were inspected in device screenshots. The existing Android native practice smoke test also passed during this update. Final sound audibility depends on the phone's UI-sound setting and system volume and is not claimed as acoustically verified.


## Build 116 and design-thinking review

Back rail tabs were reduced from 142x48 dp to 96x32 dp, attached at the frame's left corners with no 18 dp inset. Reserved vertical space was reduced from 52 to 34 dp at each edge, reclaiming 36 dp for content. Installed build 116 (0.1.115-diag), with JVM build/tests passing; inspected its placement on the Flip6.

The user asked that the next Builder review focus on flow, UI/UX and design-thinking principles. Treat field use and analysis authoring as distinct tasks to investigate with the operator, not assumed interchangeable roles. Start with user goals and observed friction, define the problem, explore alternatives, prototype a small journey, then test task completion and comprehension.

Directly observed on the Flip6: tapping Use this question with my recording led to Build an analysis, New/Save/Practice, an editable name and tool controls. The Evidence section said no recording was loaded and directed the user away to Tools to import it; it did not provide an import action there. Thus a request to run an existing question currently opens authoring controls and leaves the evidence-acquisition step unresolved in place. This is a concrete flow problem, not an inferred preference about colors or styling. The starting question also consumes most of a screen before its actions, and a single deauth question can make the broader Builder capability hard to discover.

Candidate design hypotheses for the next iteration: separate Use an analysis, Build an analysis and Practice entry paths; resolve evidence selection inside the running flow; present one dominant next action per step; keep advanced wiring optional; preserve drafts and a clear return path. These are hypotheses to prototype and evaluate with the user, not a claim that the UX has been validated with refugees or field operators.


## Build 117: intent-separated journeys

Design hypothesis: an operator running a ready-made analysis should not encounter tool authoring before choosing evidence. The Builder entry now has three explicit paths: Use an analysis, Build an analysis and Practice. Use offers a preset and saved/included workflows, then loads a recording directly in that journey. The recording step exposes empty/loading/failed/ready/empty-file states, cancellation, retry and Run. Results remain action-first; Back from a result returns to the loaded recording, then to analysis choice. Edit a copy is an explicit transition into authoring.

Build keeps the tool editor, offers recording import in place for testing, and retains its own in-memory draft/undo/redo while the operator uses or practices with another analysis. A temporary capture run can now take an explicit workflow without changing the workflow selected for automatic detection triage; Use for triage remains an explicit action.

Installed build 117 (0.1.116-diag). Android build and 284 JVM tests pass. The existing native Flip6 smoke regression passes. Direct device walkthrough: Home > Use > preset > Choose recording opens Android's picker; cancelling returns to the same selected analysis. Imported only the generated test capture through that picker, yielding six available packets and Run analysis. Running produced the action-first report; device Back returned to the recording's Run step. Edit a copy opened authoring; switching to Use and back to Build restored the unsaved copy. The phone was left on the three-intent entry screen.

A generated test-only capture was copied to Download/TRAINING-LayerScope-build117-review.pcap to test this route; the app recognizes the TRAINING- name and labels the report as practice data. A copy is retained at diagnostic-artifacts/builder117/TRAINING-LayerScope-build117-review.pcap. No other user files were selected during the walkthrough. This verifies the intended task path and recovery behavior; comprehension and preference still need operator feedback on this prototype.

## Build 118 - blocks first

Builder now opens directly into a Scratch-inspired workspace. Evidence, Wireshark, Timeline, and Report categories expose tap-to-add / hold-to-drag blocks. Canvas blocks use category colors and input/output shoulders, retaining typed ports and connections. New, Load, Save, Run are primary controls; recording, undo, redo, and more are secondary. Saved and included analytics live in Load; included analytics open as editable copies. Settings and extra actions open in framed dialogs. Results open separately and return to blocks. New analytic guidance describes the next action rather than showing an internal wiring error.

Verified on the Flip6: workspace, Load menu, practice analytic load, and Run to action-first report. All 284 JVM tests pass. On-device native smoke passes six packets, three EAPOL, zero malformed, threshold 2/3/2, unrelated packet excluded. Build 118 installed preserving data.

Architecture direction: small deterministic atoms remain responsible for observations and immediate findings. Optional local AI assessment/explanation/translation atoms are future work; no model or AI runtime was installed in this build. Device support and offline performance need measurement before choosing a model.

## Build 119 - first AI toolkit increment

Implemented product-owned model contracts under controller/ai with no Android or framework types in the request/provider API. Tasks include capture review, explanation, analytic authoring, atom definition, and agent definition. Contracts support structured responses, tool definitions, tool call identities, and tokenizer-derived context budgeting. The coordinator serializes inference, propagates cancellation, applies deadlines, checks returned model identity, and rejects unsupported tasks and undeclared tool calls. The orchestration host remains responsible for tool-argument schema validation and dispatch. The installed provider explicitly reports no product model; no fake inference is shipped.

CaptureEvidence indexes the full available capture before paging and correlates episodes across page boundaries. It validates capture/page identities, duplicate frame references, and supplied coverage. CaptureReviewer exposes the current disconnect checks as an explicitly rule-based baseline. Builder Review / AI opens an action-first recording review, supporting-frame links, optional explanations and recording limits, and a prompt field. Prompt authoring stays unavailable until a compatible bundled model exists. Generated analytic wiring is validated before becoming an editable draft; prompt and model provenance survive save, exchange, and copy. Sharing warns that the authoring prompt is included. Existing GATT and Hunter code was not changed by this increment.

Validation: 500-observation tests cover complete paging, cross-page deauth/EAPOL linkage, unrelated targets, zero-based native references, incomplete coverage, malformed observations, and invalid model evidence. Workflow proposal tests cover invalid/orphan/unknown steps, fractional versions, capture-specific address defaults, provenance round trips, tokenizer limits, cancellation and model timeouts. JSON decoding now limits nesting and rejects duplicate keys, invalid numbers, and raw control characters.

Koog 1.3.0 was evaluated in tools/ai-toolkit-evaluation with an isolated Kotlin 2.3.10 project. An actual Koog graph executes three local fixture nodes over 500 test integers without invoking a model. The main app remains Kotlin 2.0.21. This is a JVM feasibility test, not Android embedding or model-quality evidence. No bespoke agent execution engine was added. Android framework integration remains a separate compatibility gate.

Training: the user authorized existing scenarios and licensed sources. tools/ai-training exports 17 synthetic seed pages from seven scenario families, split as 9 train / 3 validation / 5 test with no family overlap. Source provenance, hashes, and label status are recorded; six Python validation tests and the preparation/training dry run pass. A local-only two-step LoRA smoke-training entry point is provided, but its GPU training path has not run. No training libraries, base weights, or trained product model were installed. The workstation has an RTX 2070 with 8192 MiB VRAM; real memory and speed measurements remain necessary. NIST SP 800-153 is registered as a reference candidate; its text is not ingested.

Flip6 checks: updated build installed preserving data; native practice decoding and analytic thresholds pass; capture review checks all six practice frames and identifies two patterns. The first evidence link opens exactly three supporting frames in Wireshark, and hardware Back returns to Builder.

Manual verification: Builder > Load > Load practice analytic > Review / AI > Review disconnect patterns. Expect two patterns. The first has two disconnect requests and one later network access message; its evidence link shows three frames. Practice remains visibly labeled throughout.

## Build 120 - field comparison and embedded AI development path

Network identity and temporal order are now separate report fields. The comparison checks device endpoints and BSSID; identical SSIDs do not establish identity, and different BSSIDs may still belong to one roaming network. EAPOL from the same device/access point before a disconnect is displayed as earlier context, never counted as a later reconnect. Broadcast disconnects can match an access point without establishing an affected client. Missing identifiers and missing traffic remain explicit. Wireshark's decoded SSID text is used instead of raw hexadecimal values.

Triage, Timeline, and Builder recording review display comparison, event order, network name, target, and access-point identifiers before expanded explanations. Supporting frame links retain native zero-based references; a separate comparison link includes contextual earlier or different-access-point EAPOL. AI evidence pages carry the same typed context and validate references against it.

The phone's native decoder rechecked both private field recordings: one contained two EAPOL messages preceding a single deauth for the same observed device/BSSID; the other contained 33 deauth frames and no EAPOL. Neither identifies the gaming network, which the user said was neither observed SSID. These recordings do not establish a cause for the reported ping increase. Original recordings and identifiable incident output remain in ignored local diagnostics and were not added to training.

Android embeds Koog 1.3.0 on Kotlin 2.3.10/AGP 8.13.2. Its graph provides the rule-based baseline immediately, then optionally runs qualified local model page assessments. Product contracts remain framework independent. The prompt executor is local-only, with no remote fallback. Phone instrumentation verifies the actual graph and retained baseline without a qualified model.

Pinned llama.cpp is compiled as an additional embedded CPU library alongside Wireshark. The provider implements exact tokenizer budgeting, the pinned non-thinking Qwen3 template, JSON grammar, serialized inference, cancellation, and SHA-256-verified model extraction. Output schema and evidence validation remain the coordinator's responsibility; generic JSON grammar alone does not enforce the task schema. Unqualified smoke weights are debug-only and rejected by the product coordinator.

The first successful local training smoke run used 17 fixture-derived examples split by scenario family: nine train, three validation, five held out. Two LoRA updates took 17.51 seconds, validation took 4.74 seconds, and peak allocated GPU memory was approximately 1.84 GiB. Merged Q8_0 development weights measure 639,446,880 bytes. This verifies the pipeline, not field accuracy. Reviewed defensive examples, independent evaluations, prompt-to-analytic tests, atom/agent tool execution, and product qualification remain necessary. See `tools/ai-training/README.md` for reproduction.

Validation: 327 Android JVM tests and nine Python tests pass, including loss/gradient equivalence and the RTX 2070 attention-kernel comparison. Native phone practice decoding, rule threshold changes, complete recording review, Koog Android execution, and both private field comparison checks pass.

Flip6 embedded-model benchmark: packaged Q8_0 weights loaded/tokenized in 922 ms, the first 59-token-input JSON probe took 11,643 ms, and a subsequent probe after cancellation took 1,641 ms. Cancellation returned in 104 ms and the provider remained reusable. This is a tiny structured-output probe, not a capture-review latency benchmark. Unoptimized debug kernels previously took 34-42 seconds; only the inference-library targets now use portable `-O3` optimization. The final probe passed without a model service, and the unqualified candidate remained rejected by the production coordinator.
