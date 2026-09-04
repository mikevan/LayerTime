# Recon phase 3: render the signal confidence tier

**This gates every other Recon phase.** Nothing noisier ships before it.

`SignalConfidence` already exists as a field on every row in `kBleUuidSignatures` and nothing renders it. That was deliberate - phase 2 added the field so phase 3 becomes a UI change rather than a re-edit of every signature.

- [ ] Carry the tier onto `ReconDetection`
- [ ] Lookup function from signature to tier
- [ ] One `ReconScreen` change to show it

Matters because some detectors are honestly low-confidence - Google FMDN (0xFEAA) shares its UUID with ordinary Eddystone beacons - and presenting those identically to a registered Flock OUI is misleading.

Estimated ~2h.
