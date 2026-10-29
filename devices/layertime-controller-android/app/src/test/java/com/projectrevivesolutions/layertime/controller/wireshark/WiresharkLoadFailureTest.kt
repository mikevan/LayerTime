package com.projectrevivesolutions.layertime.controller.wireshark

import com.projectrevivesolutions.layertime.controller.wireshark.WiresharkDecoder.FailureKind
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

/** The diagnostic must name the right failure and never alter the loader's message. */
class WiresharkLoadFailureTest {

    @Test
    fun missingTransitiveDependency_observedOnTclBuild12() {
        // Verbatim from the TCL K24, build 0.1.11 (12), 2026-10-11.
        val raw = "dlopen failed: library \"libintl.so\" not found: needed by " +
            "/data/app/~~T9H2JyodTIEjK8CEJ767Xw==/com.projectrevivesolutions.layertime.controller.diag-" +
            "3hvtmqz8QzkI5U6gRE8UTw==/lib/arm64/libglib-2.0.so in namespace clns-4"
        val f = WiresharkDecoder.classify(raw)
        assertEquals(FailureKind.MISSING_TRANSITIVE_DEPENDENCY, f.kind)
        assertEquals("libintl.so", f.library)
        assertEquals("libglib-2.0.so", f.neededBy)
        assertEquals(raw, f.raw)
    }

    @Test
    fun missingPrimaryLibrary_dlopenForm() {
        val f = WiresharkDecoder.classify("dlopen failed: library \"libltws.so\" not found")
        assertEquals(FailureKind.MISSING_PRIMARY_LIBRARY, f.kind)
        assertEquals("libltws.so", f.library)
        assertNull(f.neededBy)
    }

    @Test
    fun missingPrimaryLibrary_classLoaderForm() {
        val raw = "dalvik.system.PathClassLoader[...] couldn't find \"libltws.so\""
        val f = WiresharkDecoder.classify(raw)
        assertEquals(FailureKind.MISSING_PRIMARY_LIBRARY, f.kind)
        assertEquals("libltws.so", f.library)
    }

    @Test
    fun unresolvedSymbol() {
        val raw = "dlopen failed: cannot locate symbol \"g_libintl_gettext\" referenced by " +
            "\"/data/app/x/lib/arm64/libglib-2.0.so\"..."
        val f = WiresharkDecoder.classify(raw)
        assertEquals(FailureKind.UNRESOLVED_SYMBOL, f.kind)
        assertEquals("g_libintl_gettext", f.symbol)
        assertEquals("libglib-2.0.so", f.neededBy)
    }

    @Test
    fun otherErrorKeepsRawText() {
        val raw = "dlopen failed: \"/data/app/x/lib/arm64/libltws.so\" is 32-bit instead of 64-bit"
        val f = WiresharkDecoder.classify(raw)
        assertEquals(FailureKind.OTHER_LOADER_ERROR, f.kind)
        assertEquals(raw, f.raw)
    }
}
