# Watch, Hunter and Vixen hardware acceptance

People relying on field tools need evidence of ordinary operation on the actual hardware. This checklist tests the capture, monitoring, and analysis chain without mistaking a successful build or a controlled component demonstration for field readiness.

Status: prepared; no hardware results are claimed by this checklist.

Use product CI artifacts labelled `pinned`, from an entirely successful run.
Record the run URL, product commit, submodule commits, firmware/APK SHA-256,
device model, board revision, Android version and tester/date. Keep private
captures and device addresses outside public Git. Candidate-library artifacts
are experimental until their pins are adopted and normal product CI passes.

## Preparation

- [ ] Preserve existing recordings and settings that matter before installing.
- [ ] Charge both watches and Hunter; use known data-capable USB cables.
- [ ] Record the current installed versions and how to restore them.
- [ ] Confirm the physical model before using that target's established upload
      tooling. Retain the S3 Plus registration/upload guards. This checklist
      does not authorize an automatic flash or a storage format.
- [ ] Use a spare SD card with known free space, plus a second card for a
      repeatable missing/invalid-card check. Never format a card holding data.
- [ ] Use owned test devices and known captures for reproducible observations.

## Run separately on the Ultra and S3 Plus

- [ ] Install the correct watch build; record the firmware hash and serial log.
- [ ] Cold boot three times; the expected screen appears without reset loops.
- [ ] Touch/buttons, screen sleep/wake, charging and time display work.
- [ ] Open the relevant sensor screens; confirm present sensors update and
      absent hardware is reported without a crash. Check GNSS outdoors when
      fitted; record conditions rather than inventing a universal fix deadline.
- [ ] Discover the Hunter, connect, receive updates and issue an ordinary
      supported command. Repeat after watch sleep and Hunter restart.
- [ ] Disconnect/reconnect ten times; record failures and whether recovery
      needs a device restart. Observe for 15 minutes with the screen off/on.

## Hunter sensors and SD evidence

- [ ] Cold boot with a card and without a card; no boot loop in either case.
- [ ] Confirm fitted sensors show plausible changing values or an explicit
      unavailable state. Record the sensor models actually fitted.
- [ ] Start a short recording; stop and close it normally; inspect the card
      on a computer and open the generated CSV/PCAP with the intended tools.
- [ ] Confirm timestamps, headers/packet structure and non-empty observations
      match the known test session. Verify existing unrelated card files remain.
- [ ] Exercise listing and transfer using Vixen; compare the transferred
      file's SHA-256 with the card copy. An empty card/list is handled clearly.
- [ ] Try an absent/unusable card through the supported workflow; the product
      reports the problem and stays usable. Do not remove a card during a write.

## Vixen connection and Play variant

- [ ] Install the diagnostic APK on the test phone; grant only requested
      permissions needed for the tested feature. Deny permissions once and
      confirm clear recovery without a crash.
- [ ] Discover/connect Hunter, receive live updates, issue a command, list and
      transfer a recorded file; disconnect/reconnect ten times.
- [ ] Repeat after Bluetooth off/on, phone screen off, app restart and Hunter
      restart. Check notification/service behaviour during a sustained session.
- [ ] Import the known PCAP, decode it and compare key fields with desktop
      Wireshark. Try a malformed file and cancellation.
- [ ] Check the no-model state and local inference with an approved test model;
      CI does not package personal weights or training datasets.
- [ ] Repeat essential flows with the Play variant on supported 4 KB and
      16 KB-page devices. The Play build must not expose the watch accessibility
      service; record installation, native loading and crash-log evidence.

## Result record

Copy one record per product/device combination. A checkbox without evidence
does not certify a release. Mark unavailable hardware tests NOT RUN.

| Field | Value |
|---|---|
| Product, device and OS | |
| Commit, dependency pins and CI run | |
| Artifact name and SHA-256 | |
| Installed prior version / restore method | |
| Tester and date | |
| Passed cases | |
| Failed or NOT RUN cases | |
| Private evidence location / public issue links | |
| Ready for internal testing? | |

The Play submission checklist belongs to the
[Vixen repository](https://github.com/mikevan/Vixen/blob/main/docs/play-candidate.md).
