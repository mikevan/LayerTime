# Running the tests

No IDE, no framework, no platform. One compiler invocation.

```
g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -o tests test_geogrid/test_geogrid.cpp ../src/services/GeoGrid.cpp
./tests
```

One case in isolation, which is how the density tooling works:

```
./tests zone_arctic_west_of_prime_meridian_is_not_svalbard
```

## Coverage and density

```
g++ -std=c++17 -O0 -g --coverage -I. -I../src -c ../src/services/GeoGrid.cpp -o GeoGrid.o
g++ -std=c++17 -O0 -g --coverage -I. -I../src -c test_geogrid/test_geogrid.cpp -o tests.o
g++ --coverage GeoGrid.o tests.o -o test_cov
python3 density.py
```

`density.py` runs every case in its own process, keeps a separate gcov profile
for each, and reports how many distinct cases touched each line.

The deployable firmware build never sees any of this. Nothing in
`platformio.ini` references `test/`, and the test binary is built by a
standalone g++ call that knows nothing about the device.

## Core model conformance

Checks `src/core/model` against `contracts/vectors`, the T-Ultra capability
profile against its JSON, and the new model against the existing T-Ultra
types it will be adapted from. Run from `test/`, because the vectors are read
from `../contracts/vectors/`.

```
g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -o tests_core test_core_model/test_core_model.cpp
./tests_core
```

## Characterization suites

These record what existing code does today, before any of it moves. They
compile the production files unchanged. Where a file needs Arduino, LVGL,
LilyGoLib, NimBLE, or ESP-IDF Wi-Fi, the headers in `stubs/` stand in for
them. The stubs exist only here; the firmware build never sees them.

A case whose name ends in `KNOWN_DEFECT` pins behaviour that looks wrong. It
is recorded, not fixed, so a later fix shows up as a deliberate change.

The production files emit a few compiler warnings of their own, so these
commands do not use `-Werror`. Run each from `test/`.

```
g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -o tests_ubx test_ubx/test_ubx.cpp ../src/services/UbxParser.cpp
./tests_ubx

g++ -std=c++17 -O0 -Wall -Wextra -I. -Istubs -I../src -o tests_recon test_recon/test_recon.cpp
./tests_recon

g++ -std=c++17 -O0 -Wall -Wextra -I. -Istubs -I../src -o tests_chats test_meshtastic_chats/test_meshtastic_chats.cpp ../src/ui/MeshtasticScreen.cpp
./tests_chats

g++ -std=c++17 -O0 -Wall -Wextra -I. -Istubs -I../src -o tests_decl test_declination_text/test_declination_text.cpp ../src/ui/MappingScreen.cpp ../src/services/DeclinationCalculator.cpp ../src/services/GeoGrid.cpp
./tests_decl

g++ -std=c++17 -O0 -Wall -Wextra -I. -Istubs -I../src -o tests_log test_detection_log/test_detection_log.cpp ../src/app/WatchApp.cpp ../src/services/ReconService.cpp
./tests_log
```

`test_recon.cpp` includes `ReconService.cpp` directly, so its file-local
classifiers and tables can be checked. Do not also pass `ReconService.cpp` on
that command line.
