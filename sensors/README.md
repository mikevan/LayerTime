# LayerTime-Sensors

Passive wireless threat detectors for small radios. You hand it a BLE
advertisement or an 802.11 frame your radio already captured, plus the set of
detectors you want, and it reports which detectors matched. It does not scan,
hop channels, store anything, or raise alerts. Those are the caller's job.

Plain C++17. No Arduino, no ESP-IDF, no platform headers, no dynamic
allocation in the Wi-Fi path. It builds with any host compiler and inside
any embedded build that accepts a library.

## Detectors

| Number | Detector | Radio | What it matches |
|---|---|---|---|
| 5 | Deauth | Wi-Fi | A burst of deauth or disassoc frames from one transmitter (6 in 3 s, 15 s cooldown) |
| 6 | Pwnagotchi | Wi-Fi | The published Pwnagotchi beacon BSSID |
| 7 | MultiSSID | Wi-Fi | Two or more confirmed network names from one BSSID |
| 8 | Flock | BLE and Wi-Fi | Flock vendor prefixes, and the XUNTONG BLE module gated on a Flock-shaped name |
| 9 | Pineapple | Wi-Fi | Hak5 Pineapple vendor prefixes |
| 10 | AirTag | BLE | Apple Find My offline-finding beacons |
| 11 | Flipper | BLE | Flipper Zero service UUIDs |
| 12 | Meta | BLE | Meta Ray-Ban and Meta service UUIDs |
| 13 | Axon | BLE and Wi-Fi | Axon vendor prefixes and body-camera SSID prefixes |
| 14 | Tile | BLE | Tile service UUIDs |
| 15 | SamsungTag | BLE | Samsung SmartTag service UUID |
| 16 | GoogleTag | BLE | Google Find My Device (Eddystone) UUID |

The numbers are stable. Consumers store and transmit them, so a number is
never reused or changed. They are sparse and start at 5; iterate
`lts::kAllDetectors` and test with `lts::isKnownDetector()` instead of
assuming a range.

Every match carries a `Confidence` (Low, Medium, High). Low means "worth
logging, not worth an alert": for example, the generic Espressif prefixes that
some Flock units use but every ESP32 dev board also uses.

## Use

```cpp
#include <lts/BleAdvertClassifier.h>
#include <lts/WifiFrameClassifier.h>

lts::DetectorSet enabled;                 // empty: nothing runs
enabled.add(lts::DetectorId::AirTag);
enabled.add(lts::DetectorId::Deauth);     // or lts::DetectorSet::all()

void onCandidate(const lts::Candidate &c, void *context) { /* copy what you keep */ }

// BLE: fill an lts::BleAdvertSource from your stack's advertisement.
lts::classifyBleAdvert(source, enabled, onCandidate, nullptr);

// Wi-Fi: one classifier per radio; it keeps burst and multi-SSID state.
lts::WifiFrameClassifier wifi;
wifi.classify(frame, length, rssi, channel, nowMs, enabled, onCandidate, nullptr);
```

A detector outside `enabled` is not evaluated and keeps no state.

## Tests

```
tools/run_tests.sh
```

Copies the library into an empty scratch folder and builds and runs every
suite there with `g++` (set `CXX` to use another compiler), so a passing run
also proves the library needs nothing outside itself. `test_architecture`
checks that directly and proves the check works by planting violations.

## PlatformIO and pioarduino

Add the library to `lib_deps`, by git URL pinned to a commit, or by a
`symlink://` path to a checked-out copy.

## License

GPL-3.0-or-later. See `LICENSE`.
