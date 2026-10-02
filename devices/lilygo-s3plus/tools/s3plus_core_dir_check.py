# LayerTime, T-Watch S3 Plus target: toolchain isolation check.
#
# Loaded by devices/lilygo-s3plus/platformio.ini as "pre:tools/s3plus_core_dir_check.py".
# devices/lilygo-s3plus/platformio.ini sets core_dir = ~/.platformio-s3plus, so the S3 Plus
# platform link, packages, Python environment, and cache stay out of the
# shared ~/.platformio that the T-Watch Ultra and the T-Dongle-C5 use. A
# PLATFORMIO_CORE_DIR environment variable silently overrides core_dir. This
# script stops every S3 Plus build that is not running in the dedicated core
# directory, and says why.

import os

try:
    Import("env")  # noqa: F821  (SCons)
except NameError:  # imported by a host test
    env = None

EXPECTED = os.path.join(os.path.expanduser("~"), ".platformio-s3plus")


def same_dir(a, b):
    def norm(p):
        return os.path.normcase(os.path.normpath(os.path.abspath(os.path.expanduser(p))))
    return norm(a) == norm(b)


if env is not None:
    actual = env.subst("$PROJECT_CORE_DIR")
    if not same_dir(actual, EXPECTED):
        print("")
        print("S3 PLUS TOOLCHAIN CHECK: build stopped. This build is using the core directory")
        print("    " + actual)
        print("but the S3 Plus must use its own,")
        print("    " + EXPECTED)
        print("so it never changes the shared toolchain of the T-Watch Ultra and the T-Dongle-C5.")
        print("A PLATFORMIO_CORE_DIR environment variable overrides core_dir in devices/lilygo-s3plus/platformio.ini.")
        print("Remove that variable, restart VS Code, and build again.")
        print("")
        env.Exit(1)  # noqa: F821
    print("S3 PLUS TOOLCHAIN CHECK: using the dedicated core directory " + actual + ".")
