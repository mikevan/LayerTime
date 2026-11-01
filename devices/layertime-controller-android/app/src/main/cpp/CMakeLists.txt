# JNI wrapper for the on-device Wireshark decoder.
# Links the arm64 libwireshark/GLib set cross-compiled by
# wireshark-spike/build_wireshark_arm64.sh (staged in ../jniLibs/arm64-v8a),
# and compiles against the headers staged in wireshark-prebuilt/include.
cmake_minimum_required(VERSION 3.22.1)
project(ltws C)

set(WS "${CMAKE_SOURCE_DIR}/wireshark-prebuilt/include")

add_library(ltws SHARED ws_jni.c ws_core.c)

target_include_directories(ltws PRIVATE
    "${WS}"              # epan/, wiretap/, wsutil/, config.h, ws_version.h, ws_symbol_export.h
    "${WS}/wsutil"       # some wsutil headers include siblings by bare name
    "${WS}/glib-2.0")    # glib.h + glibconfig.h

# Prebuilt shared objects (unversioned sonames, Android-friendly) by real name.
foreach(lib wireshark wiretap wsutil glib-2.0 gobject-2.0 gmodule-2.0 gio-2.0
            gcrypt gpg-error cares z ffi pcre2-8)
    add_library(ws_${lib} SHARED IMPORTED)
    set_target_properties(ws_${lib} PROPERTIES
        IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}/lib${lib}.so")
endforeach()

# 16 KB page alignment (2026-11-01). The app now builds with NDK r30, which links 16 KB-aligned by default;
# these flags stay as a safety net in case ndkVersion is ever lowered below r28.
# https://developer.android.com/guide/practices/page-sizes
target_link_options(ltws PRIVATE "-Wl,-z,max-page-size=16384" "-Wl,-z,common-page-size=16384")

find_library(log-lib log)
# Link only what ws_jni references directly; libwireshark's own NEEDED pulls the rest at runtime.
target_link_libraries(ltws ws_wireshark ws_wiretap ws_wsutil ws_glib-2.0 ws_gobject-2.0 ${log-lib})
