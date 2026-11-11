# Build 122 phone runtime experiment

This experiment uses the unchanged `domain-v1-1090-q4km` neural weights. It does not represent another training run or qualification of the model's answers.

Two frozen cases took 74,838 and 73,720 ms before the compound runtime change, then 28,846 and 23,034 ms afterward. A repeat took 30,291 and 24,673 ms. The different-network answer remained incorrect.

The changes combine ARM dot-product/FP16 instructions, a 512-token physical prefill batch, bounded context allocation, four decoding threads, and six prefill threads. These measurements do not isolate the contribution of any individual change. The optimized debug build checks CPU support before loading the model; the release configuration keeps its portable target.

In the repeat, cancellation returned in 116 ms. A queued token-count request completed 87 ms later, establishing worker availability after cancellation; it does not establish successful subsequent full generation. Process PSS was 4,277,224 KiB after that barrier. This is not a demonstrated memory improvement or sustained thermal/field qualification.

`phone-results.json`, `phone-cancellation-retest.json`, and `summary.json` preserve measurements. `runtime-source/` preserves the relevant source. The candidate remains development-only and excluded from ordinary product assessments.
