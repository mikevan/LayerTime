# LayerTime, T-Watch Ultra target: LVGL 9.4.0 ThorVG source repair.
#
# Loaded by devices/lilygo-tultra/platformio.ini as
# "pre:tools/tultra_lvgl_thorvg_patch.py". Runs before every build.
#
# The problem: LVGL 9.4.0's bundled ThorVG (src/libs/thorvg/) calls memcpy,
# memset, and strchr in tvgCompressor.cpp, tvgInitializer.cpp, and
# tvgRender.h without including <cstring>. The GCC 14.2 toolchain of Arduino
# core 3.3.6 does not pull <cstring> in by another route, so those files fail
# with "'memcpy' was not declared in this scope". ThorVG is compiled only
# because the owl SVG needs vector graphics (lv_conf_tultra.h).
#
# History: on 2026-08-23 the Ultra's downloaded copy of LVGL was repaired by
# hand, adding "#include <cstring>" to exactly those three files. The repair
# was never in the repository, so the first fresh fetch after layout step 4
# (2026-10-07) lost it. This script makes the same repair, byte for byte, on
# every fresh copy, and proves it with SHA-256: each file must hash to the
# fresh LVGL 9.4.0 file (then it is repaired) or to the repaired file (then
# it is left alone). Anything else stops the build, because it means LVGL
# changed and the repair has to be reviewed, not applied blindly.
#
# Why not force-include <cstring> for every C++ file, as the S3 Plus does:
# tried first (2026-10-07). <cstring> pulls in sdkconfig.h, which defines
# CONFIG_IDF_TARGET_ESP32S3 in files that never saw it before. ESP8266Audio's
# AudioOutputULP.cpp is guarded by "#if CONFIG_IDF_TARGET_ESP32 || ... S3"
# with no include above the guard, so in the known-good build it compiled to
# an empty object (1,368 bytes, no headers in its dependency file). With the
# force-include its body compiled, and it does not build on ESP-IDF 5.5
# (RTC_IO_PAD_DAC1_REG undeclared). The force-include changes what other
# code compiles; this repair touches only the three files that need it.

import hashlib
import pathlib

Import("env")  # noqa: F821  (SCons)

THORVG = pathlib.Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst("$PIOENV") / "lvgl" / "src" / "libs" / "thorvg"  # noqa: F821

# (file, the line the include goes in front of, fresh SHA-256, repaired SHA-256)
REPAIRS = [
    ("tvgCompressor.cpp", b"#include <string>\n",
     "fdda523535dd439cf6a77c3d07617284b5748869ded64901eaaa5df467d9b04f",
     "9accf32c3b9e6fbf2772af5fc2e361773372be797163c5ed75f548320c8e4ee6"),
    ("tvgInitializer.cpp", b'#include "tvgCommon.h"\n',
     "c7c15aee550dc8335f8fcb82f878c0af536771247709de41540693f42fa70d19",
     "aabdbd5f9b25fabfd6d1e523687629db9a3006f6d06452ad9759cb5078641fba"),
    ("tvgRender.h", b"#include <math.h>\n",
     "874267a7e5d4c0273a02ec8811333bbd7468747df34eb6678fc89e3185b53a40",
     "025b76d7d59e61cec50c609d6d0b55bc12890e2666ba695e8d32db5ad342f0ba"),
]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def stop(message):
    print("*** LayerTime ThorVG repair: " + message)
    env.Exit(1)  # noqa: F821


for name, anchor, fresh, repaired in REPAIRS:
    path = THORVG / name
    if not path.is_file():
        stop(f"{path} is missing. LVGL 9.4.0 should be installed before the build starts.")
    data = path.read_bytes()
    if sha(data) == repaired:
        continue
    if sha(data) != fresh:
        stop(f"{name} is neither the LVGL 9.4.0 file nor the repaired one. Review the repair before building.")
    if data.count(anchor) != 1:
        stop(f"{name}: the line the include belongs in front of was not found exactly once.")
    data = data.replace(anchor, b"#include <cstring>\n" + anchor, 1)
    if sha(data) != repaired:
        stop(f"{name}: the repaired file does not match the known-good copy.")
    path.write_bytes(data)
    print(f"LayerTime ThorVG repair: added #include <cstring> to {name}.")
