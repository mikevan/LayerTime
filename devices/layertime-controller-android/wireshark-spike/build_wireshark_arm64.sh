#!/usr/bin/env bash
# Reproducible arm64-v8a build of Wireshark (libwireshark/epan) and its full
# native dependency chain with the Android NDK, staged into the app.
# This is the exact sequence that produced the working set on 2026-10-11.
# 2026-11-01: every shared object is linked 16 KB-page aligned (Google's two
# linker flags, needed on NDK r27c; r28 does it by default) and step 11 fails
# the build if any staged library is not. Android 15+ devices with 16 KB pages
# (Galaxy Z Flip6) ran the old 4 KB set only in compatibility mode.
# https://developer.android.com/16kb-page-size
# Wireshark is the decoder; no dissectors are pruned.
#
# Host: Linux x86_64 (Cowork device VM: Ubuntu 22.04, no root).
# Needs: $NDK = Linux NDK r27c root (extract to a real Linux fs, not a Windows
#        mount: the NDK uses symlinks). Network: github.com, archive.ubuntu.com,
#        pypi.org. Re-running is safe: every step skips or resumes finished work.
#
# PINNED VERSIONS
WIRESHARK_TAG=v4.4.19
GLIB_TAG=2.80.5
GVDB_REV=0854af0fdb6d527a8d1999835ac2c5059976c210   # GLib 2.80.5 gvdb.wrap revision
PCRE2_TAG=pcre2-10.44
LIBFFI_TAG=v3.4.6
ZLIB_TAG=v1.3.1
GPGERR_VER=1.51          # Ubuntu pool .orig (ships ./configure)
GCRYPT_VER=1.9.4         # Ubuntu pool .orig (ships ./configure)
CARES_TAG=v1.34.4
API=28                   # bionic provides iconv (GLib needs it) from API 28; TCL K24 is 34
ABI=arm64-v8a; TRIPLE=aarch64-linux-android
PG16="-Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384"   # 16 KB page alignment
set -euo pipefail
: "${NDK:?set NDK to the extracted Linux NDK root}"

ROOT="${1:-$HOME/ws_arm64}"; SRC="$ROOT/src"; PREFIX="$ROOT/prefix"; TOOLS="$ROOT/tools"
mkdir -p "$SRC" "$PREFIX/bin" "$TOOLS/debs"
TC="$NDK/toolchains/llvm/prebuilt/linux-x86_64"
CC="$TC/bin/${TRIPLE}${API}-clang"; CXX="$TC/bin/${TRIPLE}${API}-clang++"
AR="$TC/bin/llvm-ar"; RANLIB="$TC/bin/llvm-ranlib"; STRIP="$TC/bin/llvm-strip"
TCF="$NDK/build/cmake/android.toolchain.cmake"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig" PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig" PKG_CONFIG_SYSROOT_DIR=""
export PATH="$PREFIX/bin:$TOOLS/root/usr/bin:$HOME/.local/bin:$PATH"
CMX=(-G Ninja -DCMAKE_TOOLCHAIN_FILE="$TCF" -DANDROID_ABI=$ABI -DANDROID_PLATFORM=android-$API
     -DCMAKE_INSTALL_PREFIX="$PREFIX" -DCMAKE_BUILD_TYPE=Release
     -DCMAKE_SHARED_LINKER_FLAGS="$PG16" -DCMAKE_MODULE_LINKER_FLAGS="$PG16")
tarball() { [ -d "$SRC/$2" ] || { curl -sSL -o "$SRC/$2.tar" "$1"; tar -C "$SRC" -xf "$SRC/$2.tar"; }; }

echo "### 0 host build tools (no root): cmake/ninja/meson via pip; flex/bison/m4 via apt-get download"
pip3 install --user --quiet cmake ninja meson packaging   # packaging: GLib 2.80 meson.build needs it
( cd "$TOOLS/debs"; for p in flex bison m4 libfl2; do ls ${p}_*.deb >/dev/null 2>&1 || apt-get download "$p"; done )
for d in "$TOOLS"/debs/*.deb; do dpkg -x "$d" "$TOOLS/root"; done

echo "### 1 zlib"
tarball https://github.com/madler/zlib/archive/refs/tags/$ZLIB_TAG.tar.gz zlib-${ZLIB_TAG#v}
( cd "$SRC/zlib-${ZLIB_TAG#v}" && cmake "${CMX[@]}" -B b && cmake --build b && cmake --install b )

echo "### 2 libffi (release tarball has ./configure)"
tarball https://github.com/libffi/libffi/releases/download/$LIBFFI_TAG/libffi-${LIBFFI_TAG#v}.tar.gz libffi-${LIBFFI_TAG#v}
( cd "$SRC/libffi-${LIBFFI_TAG#v}" && { [ -f Makefile ] || ./configure --host=$TRIPLE --prefix="$PREFIX" \
    --enable-shared --disable-static CC="$CC" AR="$AR" RANLIB="$RANLIB" LDFLAGS="$PG16"; } && make -j"$(nproc)" && make install )

echo "### 3 PCRE2 (shared: GLib links it into a shared object)"
tarball https://github.com/PCRE2Project/pcre2/archive/refs/tags/$PCRE2_TAG.tar.gz pcre2-$PCRE2_TAG
( cd "$SRC/pcre2-$PCRE2_TAG" && cmake "${CMX[@]}" -B b -DBUILD_SHARED_LIBS=ON \
    -DPCRE2_BUILD_PCRE2GREP=OFF -DPCRE2_BUILD_TESTS=OFF && cmake --build b && cmake --install b )

echo "### 4 GLib (tarball omits the gvdb submodule: fetch it from the GNOME GitHub mirror)"
tarball https://github.com/GNOME/glib/archive/refs/tags/$GLIB_TAG.tar.gz glib-$GLIB_TAG
G="$SRC/glib-$GLIB_TAG"
[ -f "$G/subprojects/gvdb/meson.build" ] || { rm -rf "$G/subprojects/gvdb";
  curl -sSL https://github.com/GNOME/gvdb/archive/$GVDB_REV.tar.gz | tar -C "$G/subprojects" -xz;
  mv "$G/subprojects/gvdb-$GVDB_REV" "$G/subprojects/gvdb"; }
cat > "$ROOT/android-cross.ini" <<INI
[binaries]
c = '$CC'
cpp = '$CXX'
ar = '$AR'
strip = '$STRIP'
pkg-config = 'pkg-config'
[host_machine]
system = 'android'
cpu_family = 'aarch64'
cpu = 'aarch64'
endian = 'little'
[built-in options]
c_args = ['-I$PREFIX/include','-O2','-fPIC']
c_link_args = ['-L$PREFIX/lib','-Wl,-z,max-page-size=16384','-Wl,-z,common-page-size=16384']
cpp_link_args = ['-L$PREFIX/lib','-Wl,-z,max-page-size=16384','-Wl,-z,common-page-size=16384']
INI
# no gettext on Android: GLib builds its bundled proxy-libintl (libintl.so stub), which must be packaged
( cd "$G" && { [ -d b ] || meson setup b --cross-file "$ROOT/android-cross.ini" --prefix="$PREFIX" \
    --default-library=shared -Dtests=false -Dglib_assert=false -Dglib_checks=false \
    -Dnls=disabled -Dlibmount=disabled -Dselinux=disabled; } && meson compile -C b && meson install -C b )

echo "### 5 libgpg-error $GPGERR_VER (add the missing aarch64-Android lock object)"
tarball http://archive.ubuntu.com/ubuntu/pool/main/libg/libgpg-error/libgpg-error_$GPGERR_VER.orig.tar.bz2 libgpg-error-$GPGERR_VER
E="$SRC/libgpg-error-$GPGERR_VER"
# bionic LP64 pthread_mutex_t: 40 bytes, zero-initialised == glibc x86_64 layout
cp -n "$E/src/syscfg/lock-obj-pub.x86_64-unknown-linux-gnu.h" "$E/src/syscfg/lock-obj-pub.aarch64-unknown-linux-android.h"
( cd "$E" && { [ -f Makefile ] || ./configure --host=$TRIPLE --prefix="$PREFIX" --enable-shared --disable-static \
    --disable-doc --disable-tests CC="$CC" LDFLAGS="$PG16"; } && make -j"$(nproc)" && make install )
# libgcrypt 1.9 still calls the old gpg-error-config; 1.51 only installs gpgrt-config
printf '#!/bin/sh\nexec "%s/bin/gpgrt-config" gpg-error "$@"\n' "$PREFIX" > "$PREFIX/bin/gpg-error-config"
chmod +x "$PREFIX/bin/gpg-error-config"

echo "### 6 libgcrypt $GCRYPT_VER (lld rejects stale version-script names without --undefined-version)"
tarball http://archive.ubuntu.com/ubuntu/pool/main/libg/libgcrypt20/libgcrypt20_$GCRYPT_VER.orig.tar.bz2 libgcrypt-$GCRYPT_VER
( cd "$SRC/libgcrypt-$GCRYPT_VER" && { [ -f Makefile ] || ./configure --host=$TRIPLE --prefix="$PREFIX" \
    --enable-shared --disable-static --disable-doc --with-libgpg-error-prefix="$PREFIX" CC="$CC"; } \
  && make LDFLAGS="-Wl,--undefined-version $PG16" -j"$(nproc)" && make install )

echo "### 7 c-ares (required by Wireshark 4.4)"
tarball https://github.com/c-ares/c-ares/archive/refs/tags/$CARES_TAG.tar.gz c-ares-${CARES_TAG#v}
( cd "$SRC/c-ares-${CARES_TAG#v}" && cmake "${CMX[@]}" -B b -DCARES_STATIC=OFF -DCARES_SHARED=ON \
    -DCARES_BUILD_TESTS=OFF -DCARES_BUILD_TOOLS=OFF && cmake --build b && cmake --install b )

echo "### 8 Wireshark $WIRESHARK_TAG: libwireshark + libwiretap + libwsutil"
WSD="$SRC/wireshark-${WIRESHARK_TAG#v}"
tarball https://github.com/wireshark/wireshark/archive/refs/tags/$WIRESHARK_TAG.tar.gz wireshark-${WIRESHARK_TAG#v}
# lemon is a build-time generator and must run on the host. LEMON_C_COMPILER exists for
# this, but the NDK's target triple/compiler id leak into it; clear them.
grep -q CMAKE_C_COMPILER_TARGET "$WSD/tools/lemon/CMakeLists.txt" || python3 - "$WSD/tools/lemon/CMakeLists.txt" <<'PY'
import sys; p=sys.argv[1]; s=open(p).read()
s=s.replace('\tset(CMAKE_SYSROOT "")\n','\tset(CMAKE_SYSROOT "")\n\tset(CMAKE_C_COMPILER_TARGET "")\n\tset(CMAKE_C_COMPILER_ID "GNU")\n\tset(CMAKE_C_COMPILER_LAUNCHER "")\n\tset(CMAKE_C_COMPILER_EXTERNAL_TOOLCHAIN "")\n\tset(CMAKE_C_FLAGS_RELEASE "")\n',1)
open(p,"w").write(s)
PY
( cd "$WSD" && { [ -f b/build.ninja ] || cmake "${CMX[@]}" -B b -DCMAKE_PREFIX_PATH="$PREFIX" \
    -DCMAKE_FIND_ROOT_PATH="$PREFIX" -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=BOTH \
    -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=BOTH -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH \
    -DLEMON_C_COMPILER=/usr/bin/gcc \
    -DBUILD_wireshark=OFF -DBUILD_tshark=OFF -DBUILD_dumpcap=OFF -DBUILD_capinfos=OFF \
    -DBUILD_editcap=OFF -DBUILD_mergecap=OFF -DBUILD_text2pcap=OFF -DBUILD_randpkt=OFF \
    -DBUILD_androiddump=OFF -DBUILD_sharkd=OFF -DBUILD_reordercap=OFF -DBUILD_dftest=OFF \
    -DENABLE_LUA=OFF -DENABLE_SMI=OFF -DENABLE_GNUTLS=OFF -DENABLE_KERBEROS=OFF \
    -DENABLE_NGHTTP2=OFF -DENABLE_BROTLI=OFF -DENABLE_ZSTD=OFF -DENABLE_SNAPPY=OFF \
    -DENABLE_LZ4=OFF -DENABLE_SPANDSP=OFF -DENABLE_BCG729=OFF -DENABLE_ILBC=OFF \
    -DENABLE_PCAP=OFF -DENABLE_MINIZIP=OFF -DENABLE_MINIZIPNG=OFF; } \
  && ninja -C b libwsutil.so libwiretap.so libwireshark.so )

echo "### 9 stage into the app: 14 libraries (stripped locally, then copied) + headers"
APP="$(cd "$(dirname "$0")/.." && pwd)/app/src/main"
JNI="$APP/jniLibs/$ABI"; INC="$APP/cpp/wireshark-prebuilt/include"; ST="$ROOT/stage"
rm -rf "$ST"; mkdir -p "$ST" "$JNI" "$INC"
for so in libwireshark libwiretap libwsutil; do cp -L "$WSD/b/run/$so.so" "$ST/"; done
for so in libglib-2.0 libgobject-2.0 libgmodule-2.0 libgio-2.0 libintl libgcrypt libgpg-error \
          libcares libz libffi libpcre2-8; do cp -L "$PREFIX/lib/$so.so" "$ST/"; done
for f in "$ST"/*.so; do "$STRIP" --strip-unneeded "$f"; cp "$f" "$JNI/"; done
( cd "$WSD" && find epan wiretap wsutil -name '*.h' -print0 | cpio -0 -pdm --quiet "$INC" )
cp "$WSD"/include/*.h "$WSD/b/config.h" "$WSD/b/ws_version.h" "$INC/"
mkdir -p "$INC/glib-2.0"; cp -r "$PREFIX/include/glib-2.0/." "$INC/glib-2.0/"
cp "$PREFIX/lib/glib-2.0/include/glibconfig.h" "$INC/glib-2.0/"
ls -l "$JNI"/lib*.so; du -ch "$JNI"/lib*.so | tail -1

echo "### 10 native closure check (fails the build if anything would not load on API 34)"
bash "$(dirname "$0")/check_native_closure.sh" "$JNI" "$NDK" 34

echo "### 11 16 KB page alignment (fails the build if any staged library is not aligned)"
"$TC/bin/llvm-readelf" --version >/dev/null
bad=0
for f in "$JNI"/lib*.so; do
  a=$("$TC/bin/llvm-readelf" -lW "$f" | awk '$1=="LOAD"{print $NF}' | sort -u | head -1)
  if [ "$a" = "0x4000" ] || [ "$a" = "0x10000" ]; then echo "ok    $a $(basename "$f")"; else echo "FAIL  $a $(basename "$f")"; bad=1; fi
done
[ $bad -eq 0 ] || { echo "16 KB alignment check FAILED"; exit 1; }
echo "DONE"
