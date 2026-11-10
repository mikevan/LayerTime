> Documentation reference copy. Original repository path: `docs/ai-multimodel-research-2026-11-10.md`. Historical statements retain their original context; use the collection index and check-in note for current status.

# LayerTime multimodel AI research

November 10, 2026

LayerTime should evaluate a team of specialized models: fast wireless classifiers, temporal and novelty detectors, a language model for explanation and Builder interaction, and an offline reference retriever. A shared evidence layer should preserve frame identities, device relationships, event order, and capture coverage. The language model should receive those observations rather than independently invent counts or links.

The proposed starting experiment combines a small supervised classifier, a small temporal model, a novelty detector, and one 3B–4B language model. Compare that combination with both deterministic tools alone and a single language model. Keep the components that improve unseen-session results within measured phone resource limits. Existing Scope measurements cover only a 0.6B development model and a tiny JSON probe; they do not establish the performance of these candidates.

## Different models for different questions

| Component | Question it answers | Candidates and proposed role |
| --- | --- | --- |
| Wireless classifier | Does this observed episode resemble a known benign or suspicious pattern? | Train CatBoost or a random forest on typed episode features; compare with a small neural classifier. Separate Wi-Fi and BLE heads where the available evidence differs. |
| Temporal model | Is behavior changing, and what sequence preceded that change? | Compare change-point statistics, a small temporal convolutional network, and Chronos forecasting models on coverage-normalized rates and event sequences. |
| Novelty detector | Is this unlike the environment we have previously observed? | Evaluate Half-Space Trees and a Kitsune-style autoencoder ensemble. A novelty score is not an attack probability. |
| Language model | What does the evidence mean, and how do I investigate it? | Compare Qwen3-4B-Instruct-2507, SmolLM3-3B, and Phi-4-mini-instruct. Include an 8B reference to measure the capability lost when choosing a smaller model. |
| Reference retriever | What protocol explanation or known analytic supports this interpretation? | Compare lexical search with a multilingual embedding model over bundled, versioned documentation and analytics. |
| Evidence and workflow validator | Are the claims supported, and can this analytic execute correctly? | Deterministic schema, address, timestamp, frame-reference, and graph checks; replay generated analytics against positive and benign captures. |

The classifiers and temporal models can examine the same episode through different representations. Their disagreements should be retained. A language-model majority vote is not a substitute for diverse evidence or measured calibration.

## Language model candidates

**Qwen3-4B-Instruct-2507** is the initial language-model candidate. Its publisher describes improved instruction following, tool usage, and knowledge coverage, and it uses a non-thinking response mode. Its task performance must be tested on LayerTime evidence rather than inferred from general benchmarks. The model card lists Apache-2.0. [Qwen model card](https://huggingface.co/Qwen/Qwen3-4B-Instruct-2507)

**SmolLM3-3B** is a useful challenger because its publisher exposes training details and supports tool-oriented interaction. Its listed native languages are English, French, Spanish, German, Italian, and Portuguese. That is a material limitation for refugee-facing language coverage: Ukrainian, Arabic, and other needed languages require dedicated testing and potentially additional training. [SmolLM3 model card](https://huggingface.co/HuggingFaceTB/SmolLM3-3B)

**Phi-4-mini-instruct** is another small-model challenger from a different model family. Use the same evidence, Builder tasks, quantization checks, and phone measurements for a fair comparison. [Microsoft model card](https://huggingface.co/microsoft/Phi-4-mini-instruct)

**Qwen3-8B** should be a larger reference candidate, not an automatic phone choice. Four-bit payload arithmetic is roughly 2 GB for four billion parameters and 4 GB for eight billion, before metadata, mixed-precision tensors, tokenizer, runtime buffers, and context cache. These are estimates, not measured package or RAM sizes. [Qwen3-8B model card](https://huggingface.co/Qwen/Qwen3-8B)

One backbone can serve separate explanation and planning adapters, loaded sequentially. That would specialize behavior without duplicating the whole model. Those adapters would share underlying errors and must not be treated as independent corroborating experts. Initially compare a shared multitask model with separate adapters; adopt the latter only if the improvement justifies their operational complexity.

## Temporal and novelty candidates

**Chronos-Bolt Tiny** has approximately nine million parameters; the model card lists Apache-2.0. It forecasts series, so an experiment could compare observed, coverage-normalized radio activity with its predicted ranges. Forecast residuals would then require separate calibration for anomaly detection. It is not a pretrained wireless attack detector. [Chronos-Bolt Tiny](https://huggingface.co/amazon/chronos-bolt-tiny)

**Chronos-2** has 120 million parameters and supports multivariate series and covariates. It offers a candidate for jointly modeling Wi-Fi frame rates, BLE advertisement rates, retries, and acquisition conditions. **TimesFM 2.5 200M** is a larger forecasting comparator. Both need task and mobile-export evaluation; workstation Python inference is not evidence of Android compatibility. [Chronos-2](https://huggingface.co/amazon/chronos-2), [TimesFM](https://huggingface.co/google/timesfm-2.5-200m-pytorch)

**ADWIN** detects distribution changes. **Half-Space Trees** provide streaming anomaly scores. River supplies research implementations, but its Python package is not an Android deployment plan. Suitable algorithms would need a tested mobile implementation or an exportable replacement. [ADWIN implementation](https://github.com/online-ml/river/blob/main/river/drift/adwin.py), [Half-Space Trees implementation](https://github.com/online-ml/river/blob/main/river/anomaly/hst.py)

**Kitsune** demonstrates an ensemble of small autoencoders for online network anomaly detection. Its feature extractor and calibration assumptions need adaptation to Hunter observations. Baseline updates should be quarantined during suspicious or poorly observed periods so a persistent disruption does not simply become the new normal. [Original Kitsune implementation](https://github.com/ymirsky/Kitsune-py)

A compact temporal convolutional network or explicit sequence model remains a strong alternative to these pretrained forecasters. Compare it on disconnect, association, EAPOL, retry, and recovery sequences, including irregular timestamps and missing observations. The simplest method that meets the evidence and latency requirements should win.

## Packet representation research

**ET-BERT** supplies pretrained traffic representations and packet-level fine-tuning procedures. Its published focus is encrypted traffic classification. That does not establish recognition of 802.11 deauthentication or BLE advertisements. [ET-BERT repository](https://github.com/linwhitehat/ET-BERT)

**NetMamba** supplies a pretrained network-traffic checkpoint on Hugging Face. Its authors have also released NetMamba+ research. This is a promising way to investigate packet sequences without treating every packet as prose, but representation compatibility, checkpoint terms, and mobile operators need verification. [NetMamba checkpoint](https://huggingface.co/wangtz/NetMamba), [Author repository](https://github.com/wangtz19/NetMamba), [NetMamba+](https://github.com/wangtz19/NetMambaPlus)

**nPrintML** demonstrates standardized packet representations and automated model comparison. Its older dependency requirements and packet representations make it a research reference rather than a ready-to-bundle wireless component. [nPrintML](https://github.com/nprint/nprintml)

For these candidates, the transfer experiment should compare frozen embeddings, wireless fine-tuning, and a small model trained directly on Hunter-compatible observations. If imported checkpoints offer no measurable gain, use the direct model. Do not preserve MAC addresses, SSIDs, capture file names, or absolute timestamps as shortcuts for dataset labels.

## Wireless data sources

**AWID3** provides raw PCAP and CSV data for WPA2 Enterprise, protected management frames, and several wireless attack families. Its official access workflow requires registration. Its enterprise setting is useful but does not substitute for WPA2-Personal networks, crowded public spaces, or Hunter's channel schedule. Access and reuse terms must be confirmed before ingestion. [Official AWID site](https://icsdweb.aegean.gr/awid/), [AWID3 description](https://icsdweb.aegean.gr/awid/awid3)

**Bluetooth Low Energy Dataset: Raw-Capture for Intrusion Analysis**, version 2, includes ambient advertisement traffic and spoofed spam scenarios. Its published license is CC BY 4.0. This is closely aligned with advertisement analysis, although it represents a narrow set of conditions. [Author dataset](https://data.mendeley.com/datasets/ntmmcw6jj8/2)

**CICIoMT2024** includes benign and attack BLE PCAPs plus profiling data. Its Wi-Fi/MQTT traffic was captured through a network tap, so that portion must not be assumed to contain over-the-air 802.11 management frames. The BLE records also need a field-level comparison with Hunter's actual capture capabilities. [Official CICIoMT2024 description](https://www.unb.ca/cic/datasets/iomt-dataset-2024.html)

**ISOT BrakTooth** provides Bluetooth Classic captures. It is relevant only if LayerTime has compatible Classic observations; it should not be represented as BLE training coverage. [ISOT dataset description](https://onlineacademiccommunity.uvic.ca/isot/2023/05/30/iot-security-datasets/)

Public data should be supplemented with consented benign captures from diverse devices and environments. Useful conditions include ordinary reconnects, AP restarts, weak signal, roaming, busy channels, BLE address rotation, crowded advertising, and missed capture intervals. Controlled defensive replay and offline transformations can add missingness and timing variations without adding attack-generation tooling to the product. Original private field recordings remain outside training unless separately authorized.

## Defensive instruction data and offline references

**Trendyol's defensive instruction dataset** contains approximately 53,200 examples and is labeled Apache-2.0. **HolyCyber** reports 51,805 examples with source references and per-row terms. Both are candidates for a selected defensive instruction mixture; neither establishes packet-grounded skill by itself. Audit answer correctness, duplicate templates, source traceability, and operational relevance before selection. [Trendyol](https://huggingface.co/datasets/Trendyol/Trendyol-Cybersecurity-Instruction-Tuning-Dataset), [HolyCyber](https://huggingface.co/datasets/Reizxn/HolyCyber)

The community NIST-derived corpus is larger, but compliance-heavy examples should not overwhelm wireless and Builder tasks. Raw security collections should be treated as source inventories, not ingested wholesale. [NIST-derived corpus](https://huggingface.co/datasets/ethanolivertroy/nist-cybersecurity-training), [secdata-raw](https://huggingface.co/datasets/deardaniel/secdata-raw)

An offline reference library should contain versioned protocol explanations, detection limitations, approved protective guidance, and analytic examples. Compare simple lexical retrieval with **Qwen3-Embedding-0.6B** and **multilingual MiniLM**. The embedding model's job is retrieval, not deciding whether an event is malicious. Retrieval must preserve source identifiers, and an empty or irrelevant result should remain visible. [Qwen embedding model](https://huggingface.co/Qwen/Qwen3-Embedding-0.6B), [Multilingual MiniLM](https://huggingface.co/sentence-transformers/paraphrase-multilingual-MiniLM-L12-v2)

## Training with several teachers

Use separate teachers to propose packet interpretations, alternative explanations, and Builder workflows. A larger general instruction model can complement a cybersecurity specialist. **Cisco Foundation-Sec-1.1-8B-Instruct** is a security reference candidate, but its notice identifies both Llama 3.1 terms and Apache-licensed Cisco changes. Teacher-output and downstream model obligations must be resolved before using its responses as student training data. [Model card](https://huggingface.co/fdtn-ai/Foundation-Sec-1.1-8B-Instruct), [Publisher notice](https://huggingface.co/fdtn-ai/Foundation-Sec-1.1-8B-Instruct/raw/main/NOTICE.md)

First generate reviewed offline instruction targets from ground-truth episodes. Have one teacher identify evidence and another propose benign explanations and missing information. Verify timestamps, counts, addresses, and supporting frames with tools. Reject ungrounded claims even when teachers agree. Retain teacher identity, prompt, source license, and reviewer decisions for each accepted example.

Then train the student with supervised targets. Add preference examples contrasting accurate, concise reports with unsupported conclusions and misleading wording. Advanced distillation can later train on student-generated trajectories. Hugging Face TRL provides a Generalized Knowledge Distillation trainer, but its documented approach is not automatically compatible with arbitrary cross-family teachers or this workstation's memory budget. Text-based supervised distillation is the simpler initial cross-family approach. [TRL GKD documentation](https://huggingface.co/docs/trl/main/en/gkd_trainer)

Compare task mixtures and adapter ranks with fixed validation sets; do not select the largest rank or longest run by assumption. More extensive continued pretraining or full fine-tuning should be justified by measured failure modes and a separate compute budget. The RTX 2070's successful two-step 0.6B run does not establish that it can efficiently train a 4B student or host several teachers at once. Teachers can run sequentially on the workstation or on separately approved training hardware; deployed Scope inference remains entirely local.

## Evidence fusion and Builder integration

Each model should return a typed observation: evidence identifiers, feature window, model version, score type, missingness, and supported interpretation. Preserve the distinction between a class probability, a novelty score, a forecast residual, and an LLM's prose assessment. Do not average them as if they were equivalent probabilities.

Begin with explicit fusion rules and compare a calibrated combiner trained on out-of-fold specialist predictions. Fit calibration only on independent calibration data. Do not train the combiner on unrealistically accurate in-sample predictions or count repeated outputs of the same backbone as independent evidence. [Probability calibration guidance](https://scikit-learn.org/stable/modules/calibration)

For a deauth/EAPOL episode, the relationship engine determines endpoint and BSSID matches and chronology. A classifier assesses resemblance to learned patterns. A temporal detector examines sustained disruption or recovery. A novelty detector compares it with a suitable baseline. The language model explains the combined evidence and unresolved alternatives, then can propose a reusable analytic. No specialist's score should silently override contradictory packet evidence.

Expose specialists as versioned Builder atoms behind product-owned APIs. A model may call a read-only evidence atom, temporal atom, classifier atom, or reference atom. Validate its draft and execute it against test captures before offering it as a reusable analytic. The existing Kotlin flow engine can retain deterministic scheduling and reproducibility, while Koog handles bounded model-driven investigation. Research does not justify replacing either engine yet.

An evidence graph can join observations by device, access point, time, channel, and explicit uncertainty. Use known relationships first. A learned graph model is an exploratory branch only if relational errors persist and labeled graph data supports a meaningful comparison.

## Acquisition coverage and phone execution

Hunter alternates observation activities and Wi-Fi channels. The current EvidenceCoverage contract correctly states that a recording does not establish continuous channel coverage. A silence detector needs actual dwell intervals, scan state, successful channel changes, packet-loss indicators, clock quality, and disconnect periods. Existing counters are not equivalent to a complete time-resolved observation history.

Train temporal models on observed exposure and missingness. A channel that was not monitored is unknown, not zero activity. Time-correlated drops across Wi-Fi and BLE can support a joint observation only when both have adequate coverage. They cannot by themselves identify jamming, a physical attack, an actor, or intent. Cellular and raw RF conclusions require additional compatible sensors and evidence.

Run small detectors continuously and invoke the larger language model for investigation or explanation. Load heavy models sequentially unless phone measurements justify concurrent residency. Immediate supported observations should remain available while a model runs; urgent reporting must not depend on a long chain of agent calls.

Keep the existing llama.cpp adapter as one option for language inference. Compare ONNX Runtime Mobile for compatible small models; ExecuTorch is another candidate if its operators and device backend justify a separate integration. CatBoost documents Android support for its model applier. None of these facts establishes that a particular research checkpoint exports successfully. Verify output parity and device execution before choosing a runtime. [ONNX Runtime Mobile](https://onnxruntime.ai/docs/get-started/with-mobile.html), [ExecuTorch](https://docs.pytorch.org/executorch/stable/index.html), [CatBoost Android support](https://github.com/catboost/catboost/blob/master/RELEASE.md)

## Experiments and decisions

| Experiment | Comparison | Evidence required to keep the added component |
| --- | --- | --- |
| Known-pattern detection | Existing checks versus supervised classifier versus combined output | Better episode recall at a matched false-warning rate on unseen sessions, with correct device and access-point attribution. |
| Temporal detection | Rolling statistics and change points versus a small temporal network versus Chronos | Faster or more reliable detection of sustained change without increased warnings caused by channel hopping or missing capture. |
| Novelty detection | Supervised detection alone versus added novelty score | Useful detection on held-out pattern families, with a measured manageable burden of unexplained anomalies. |
| Packet transfer | Typed features versus frozen and fine-tuned packet encoders | Improvement on Hunter-compatible evidence after address and collection artifacts are removed. |
| Explanation | Unmodified versus specialized 3B–4B models, plus 8B reference | Correct evidence references, chronology, counts, alternatives, and plain-language significance. Measure unsupported claims separately from readability. |
| Builder authoring | Shared model versus specialized planner adapter | Valid graph, correct atom selection, successful replay on positive and benign captures, and useful requests for missing information. |
| Retrieval | No retrieval versus lexical versus embedding retrieval | Fewer factual errors and traceable relevant sources without unacceptable response delay. |
| Fusion | Each specialist alone versus combined system, then remove one specialist at a time | Measurable improvement beyond the best single component at a matched warning burden and phone resource budget. |

Split data by original capture session, environment, device family, and source as appropriate. Overlapping windows and transformed copies must remain together. Reserve unseen environments and later recordings for final evaluation. Packet-random splits can reward memorizing the same traffic rather than generalizing. [Grouped validation guidance](https://scikit-learn.org/stable/modules/cross_validation)

Counterfactual checks should rename MACs and SSIDs while preserving relationships, change only BSSID identity, reverse event order, insert retries, remove evidence, and introduce observation gaps. The expected interpretation should change only where the evidence change warrants it. Address renaming should not change the verdict; reversing EAPOL order should change the chronology claim. Validate transformations and expected outputs before treating them as labels.

Report episode-level precision and recall, false warnings per monitored hour, detection delay, target attribution, unsupported claims, abstention behavior, and analytic replay results. Stratify by protocol, acquisition coverage, environment, and language. Phone measurements should include cold and warm latency, peak memory, battery cost, package size, and sustained thermal behavior. An overall accuracy number is insufficient.

Numerical release thresholds need representative data and a stated acceptable warning burden. Establish them before final model selection. Any added expert should earn its place through these experiments; the initial research recommendation is an ensemble to test, not a claim that an ensemble has already won.

## Recommended first comparison

Prepare a provenance-tracked wireless corpus and acquisition-aware evidence format first. Train a small known-pattern classifier and a temporal baseline, then compare a selected 3B–4B language model with and without their outputs and offline references. Add a novelty detector as an independent branch. Use larger teachers to improve reviewed training targets rather than shipping every teacher on the phone. Keep NetMamba, ET-BERT, multivariate foundation forecasting, and learned graph models as explicit challenger experiments.

This gives LayerTime several complementary learning methods against the same problem, while preserving immediate reporting, inspectable evidence, reusable analytics, and fully offline execution. It also allows the research to reject an expensive component when a smaller method works better.
