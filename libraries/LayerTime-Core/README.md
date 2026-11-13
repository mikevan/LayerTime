# LayerTime-Core

Shared C++17 library for Hunter, LayerTime Ultra and LayerTime S3 Plus. The
application model, application logic, platform ports and LayerTime Link wire
codec live here. Core does not depend on Arduino, ESP-IDF, a display, an SD
card, or a radio driver. Public detector types and classification are supplied
by the separately versioned **LayerTime-Sensors** library.

Public include paths remain `core/model/...`, `core/logic/...`, `core/link/...`,
`core/ports/...` and `core/app/...`. Consumers add this library's `src` to
their include search path. PlatformIO consumers use the included `library.json`
and declare both this library and LayerTime-Sensors in `lib_deps`; core is no
longer also compiled as application source.

For a native CMake build, configure this directory with
`-DLAYERTIME_SENSORS_DIR=/absolute/path/to/LayerTime-Sensors` and build the
`LayerTimeCore` target. A parent CMake project may instead supply its own
`LayerTimeSensors` target and link against `LayerTime::Core`.

The prepared standalone repository includes the core-only unit suites. Add
`-DLAYERTIME_BUILD_TESTS=ON` and
`-DLAYERTIME_CONTRACTS_DIR=/absolute/path/to/LayerTime-Contracts`, build, and run `ctest --test-dir <build-folder>
--output-on-failure`. Cross-device boundary/model conformance tests and the
historical sensor-extraction fixture remain workspace integration tests.

The canonical language-independent wire and model definitions remain in the
workspace's `contracts/`, with byte-exact vectors consumed by the C++, Kotlin
and Monkey C implementations. Vixen's Android application has its own language
binding; it is not claimed to link this C++ application library.

From the LayerTime workspace root, run `tools/ci/preflight.sh --host` under
Linux/WSL. `test/` contains the existing unit, conformance, boundary and replay
suites. Some conformance suites deliberately compare core with watch adapters
and therefore remain workspace integration tests. A source relocation is not
a new firmware hardware-validation claim.

GPL-3.0-or-later. Original source copyright and attribution are retained.
