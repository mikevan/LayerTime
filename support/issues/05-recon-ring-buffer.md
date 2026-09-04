# Recon phase 5: ring-buffer handoff out of the promiscuous callback

Detections are currently written into `_status.detections[]` directly from the Wi-Fi driver task, while LVGL reads that same struct from the UI task. No handoff, no lock - a cross-task data race on a struct being rendered.

Symptom would be rare, unreproducible crashes under heavy RF.

- [ ] Ring buffer written by the driver task, drained on the UI side
- [ ] Free diagnostic first: toggle Early Warning off and scroll the Recon menu. If scrolling gets smooth, this race is also the cause of the sluggish menu.

Prerequisite for phase 6's frame volume. Estimated ~2h.
