# LayerTime, T-Watch S3 Plus target: LVGL 9.4.0 ThorVG build fix.
#
# Loaded by lilygo-s3plus/platformio.ini as "pre:tools/s3plus_lvgl_thorvg_cstring.py".
#
# The problem: LVGL 9.4.0's bundled ThorVG (src/libs/thorvg/) calls memcpy,
# memset, and strchr in tvgRender.h, tvgCompressor.cpp, and tvgInitializer.cpp
# without including <cstring>. The GCC 14.2 toolchain of Arduino core 3.3.6
# does not pull <cstring> in by another route, so those files fail with
# "'memcpy' was not declared in this scope". ThorVG is only compiled when
# vector graphics are on, which LayerTime needs for the owl SVG
# (lv_conf_s3plus.h: LV_USE_VECTOR_GRAPHIC, LV_USE_THORVG_INTERNAL).
#
# How the T-Watch Ultra gets away with it: its .pio/libdeps copy of LVGL was
# edited by hand on 2026-08-23 to add "#include <cstring>" to exactly those
# three files. A fresh fetch of LVGL 9.4.0 (registry package, identical to the
# v9.4.0 git tag) does not have the edit, and this target never edits
# downloaded libraries.
#
# The fix: force-include <cstring> for every C++ compile of this target
# (CXXFLAGS, so C and assembler sources are untouched). <cstring> is a
# standard header; including it first changes nothing for code that already
# includes it.
#
# Why not only the ThorVG files: a first version did that with a per-file
# build middleware, and it worked on Linux but not on Windows. On Windows the
# pioarduino 55.03.36-1 platform (builder/frameworks/arduino.py, the
# "if IS_WINDOWS:" block that integrates user middlewares with its include-path
# shortening) applies a middleware's per-file flags by changing the shared
# build environment, compiling, and changing it back. SCons expands the
# command line later, at build time, so the per-file flags are gone by then.
# Reproduced on Linux by forcing that block on; the ThorVG files then fail
# exactly as on Windows. Flags set in the environment for the whole build are
# not affected.

Import("env")  # noqa: F821  (SCons)

env.Append(CXXFLAGS=["-include", "cstring"])  # noqa: F821
