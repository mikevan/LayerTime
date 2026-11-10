# Additional wireless source audit — November 10, 2026

## InSecTT BLE connection measurements

The [publisher's record](https://zenodo.org/records/7152044) and [pinned repository](https://github.com/juliankaroliny/InSecTT_BLE_Channel_Sniff_Dataset/tree/c8382b311b1d8d382241305ef0f8a7d7319b70af) provide a separate source of BLE measurements. The pinned README explicitly grants CC BY 4.0 reuse. Attribution: Julian Karoliny, Thomas Blazek, Hans-Peter Bernhard and Andreas Springer, InSecTT BLE Channel Sniff Dataset v1.1.1 (2022), DOI 10.5281/zenodo.7152044.

Downloaded only the README, six measurement metadata files and 18 CSV streams, totaling 2,005,417 bytes. These are pinned to commit `c8382b311b1d8d382241305ef0f8a7d7319b70af`; file sizes, Git blob hashes and SHA-256 hashes were verified. Full raw bitstream archives were not needed for this intake.

The source uses six controlled Nordic devices in three connection pairs, with additional ambient office traffic. HackRF and Ubertooth captures were made in separate runs. The measurement channel is BLE data channel 22; hopping therefore means many messages are not observed on that channel. The data provides connection access addresses and header/timestamp records, not an inventory of device MAC addresses or people.

The local audit matched all 66,739 CSV records with publisher metadata counts across six measurement sets and 18 streams. Its report is `tools/ai-training/runs/insectt-connection-audit-v1/report.json`. Header range checks and clock-transition counts do not establish CRC correctness, application meaning or full traffic coverage. The README calls `t_a` a timestamp without explicitly specifying its units, so rates and elapsed seconds remain unlabelled until that is verified.

This source can support coverage-limitation and connection-behavior work. It cannot simply be added to the advertisement-spam classifier: the observed protocol layer, channel and features differ. Controlled connections plus ambient traffic are not independently certified attack-free conditions. No new attack labels were invented and no examples from this source have entered the current model run.

## KRACK and Kr00k CSV candidate

The [publisher page](https://data.mendeley.com/datasets/x29ggnwghy/1) declares CC BY 4.0. The actual published file inventory inspected through Mendeley's version-1 API contains two CSVs, approximately 690 MB and 1.19 GB; no raw PCAP appeared in that inventory despite the description mentioning captures.

Inspected only bounded 65,536-byte prefixes, retaining metadata and samples under `tools/ai-training/external-data/additional-source-audit`. The header has a receiver address and packet feature fields but no sender address, BSSID, EAPOL identity or disconnect reason. Those omissions prevent reconstructing the same-client/same-network disconnect-to-security-exchange chain required here. Sample epoch timestamps are rounded to whole seconds while relative-time fields retain finer resolution. Captured data dates and provenance require further review before other modelling uses.

No full CSV download or training was performed. This is a task-compatibility exclusion, not a finding that the dataset has no other research value.

## Smart-home dataset candidate

The [publisher record](https://data.mendeley.com/datasets/x95b37z2vy/1) declares CC BY 4.0, but the version-1 root file inventory returned no files. Only metadata was retained. It is not an available source for this run.

## Wireshark examples

The official sample-capture index contains ordinary Wi-Fi joins and EAPOL examples. The inspected index did not state a blanket capture-data license. Do not infer that the Wireshark program's license automatically licenses every third-party sample. The inspected main-repository regression-capture directory contained a small EAPOL coverage sample, but it is not a source of broad independent wireless field labels. No such capture was added to training during this audit.
