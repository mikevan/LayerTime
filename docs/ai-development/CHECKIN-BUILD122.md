Checkpoint Builder integration, trained AI candidates and faster phone runtime

Preserve the Android Builder/reporting integration, passive client/BSSID and timeline correlation, navigation and editing work, GATT queue/connection diagnostics, Hunter source changes, and embedded local inference interfaces. Pin the external llama.cpp source and include its fetch helper rather than vendoring its build tree.

Build 122 on the Flip6 retains the first trained Qwen3-4B domain candidate in the explicit debug test screen. Runtime changes reduced two selected responses from about 74 seconds each to 23–29 seconds; a repeat measured 25–30 seconds. Cancellation returned in 116 ms and a subsequent token-count request completed. These measurements do not qualify sustained performance, memory improvement or subsequent full generation.

Preserve the admitted development data, rejected/draft data with their labels, ontology/source provenance, repair and training tools, and exact acceptance outputs with hash-bound semantic judgments. The first candidate completed 1,090 updates. Its correction continuation completed another 682 updates with no skips and finite saved weights. Both passed only 2/12 frozen acceptance cases. The correction improved BLE address-type explanation but regressed captured-instruction explanation, and still missed required Wi-Fi identity/timing findings. It was not packaged or installed. Neither candidate is operationally qualified; ordinary product assessments exclude them.

Validation: 168 Python tests completed, one skipped; 364 Android unit tests passed with no failures or errors; the Android validation build succeeded. Two real-model phone cases and cancellation recovery were measured. Hunter changes are preserved as a development checkpoint; this commit makes no new firmware field-validation claim.

Model weights, private field captures, local build outputs and unrelated research artifacts are excluded. Two synthetic-observation JSONL files exceed GitHub's regular size limit and are stored through Git LFS without changing their bytes. Clones need Git LFS to materialize those data files. The remaining training/evaluation evidence stays in regular Git.

The owner explicitly authorized this commit and normal push in chat. Earlier statements reserving commits to the owner describe the preceding checkpoint workflow. No acceptance thresholds were relaxed, and no acceptance outputs were used as optimizer targets.

See CURRENT-AI-STATUS.md for current limitations, exact evidence locations and the proposed next controlled experiment.
