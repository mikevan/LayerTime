package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.ui.GridPosition
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * Reference values from the Python `mgrs` package (GeoTrans-derived) and PROJ via
 * `pyproj` (UTM, WGS84), generated 2026-10-29. Covers both hemispheres, the
 * equator, the Norway (32V) and Svalbard (33X) zone exceptions, and northern Europe.
 */
class GridPositionTest {
    private val cases = listOf(
        Triple(38.8895, -77.0353, "18S UJ 23478 06483"),
        Triple(36.0822, -94.1719, "15S UV 94486 93701"),
        Triple(-33.8688, 151.2093, "56H LH 34368 50948"),
        Triple(69.65, 18.96, "34W DC 20838 28164"),
        Triple(34.5553, 69.2075, "42S WD 19036 23748"),
        Triple(0.5, 0.5, "31N BA 21734 55318"),
        Triple(-0.5, -0.5, "30M YE 78265 44681"),
        Triple(60.39, 5.32, "32V KN 97230 00510"),
        Triple(78.22, 15.65, "33X WG 14813 83004"),
    )

    @Test fun latLonToMgrsMatchesReference() {
        for ((lat, lon, mgrs) in cases) assertEquals("$lat,$lon", mgrs, GridPosition.toMgrs(lat, lon))
    }

    @Test fun mgrsParsesBackWithinTwoMetres() {
        for ((lat, lon, mgrs) in cases) {
            val p = GridPosition.parse(mgrs) as GridPosition.Parsed.Ok
            // 1 m grid squares name the south-west corner, so allow about 2 m (0.00002 degrees).
            assertEquals(mgrs, lat, p.position.lat, 0.00002)
            assertEquals(mgrs, lon, p.position.lon, 0.00003)
        }
    }

    @Test fun mgrsWithoutSpacesAndAtLowerPrecision() {
        val p = GridPosition.parse("18SUJ2347806483") as GridPosition.Parsed.Ok
        assertEquals(38.8895, p.position.lat, 0.00002)
        val coarse = GridPosition.parse("18S UJ 23 06") as GridPosition.Parsed.Ok   // 1 km square
        assertEquals(38.8895, coarse.position.lat, 0.01)
        assertEquals(-77.0353, coarse.position.lon, 0.015)
    }

    @Test fun utmMatchesProj() {
        val u = GridPosition.toUtm(38.8895, -77.0353)
        assertEquals(18, u.zone); assertEquals(323478.1, u.easting, 0.5); assertEquals(4306483.2, u.northing, 0.5)
        val s = GridPosition.toUtm(-33.8688, 151.2093)
        assertEquals(334368.6, s.easting, 0.5); assertEquals(6250948.3, s.northing, 0.5)
        val n = GridPosition.toUtm(69.65, 18.96)
        assertEquals(420838.9, n.easting, 0.5); assertEquals(7728164.3, n.northing, 0.5)
    }

    @Test fun utmTextParses() {
        val p = GridPosition.parse("18S 323478 4306483") as GridPosition.Parsed.Ok
        assertEquals(38.8895, p.position.lat, 0.00002); assertEquals(-77.0353, p.position.lon, 0.00002)
        val south = GridPosition.parse("56H 334369E 6250948N") as GridPosition.Parsed.Ok
        assertEquals(-33.8688, south.position.lat, 0.00002); assertEquals(151.2093, south.position.lon, 0.00002)
    }

    @Test fun decimalForms() {
        val a = GridPosition.parse("38.8895, -77.0353") as GridPosition.Parsed.Ok
        assertEquals(-77.0353, a.position.lon, 1e-9)
        val b = GridPosition.parse("33.8688S 151.2093E") as GridPosition.Parsed.Ok
        assertEquals(-33.8688, b.position.lat, 1e-9)
    }

    @Test fun badInputIsRefusedWithAReason() {
        assertTrue(GridPosition.parse("") is GridPosition.Parsed.Error)
        assertTrue(GridPosition.parse("18I UJ 23478 06483") is GridPosition.Parsed.Error)      // no band I
        assertTrue(GridPosition.parse("18S UJ 2347 064830") is GridPosition.Parsed.Error)      // unequal digits
        assertTrue(GridPosition.parse("88.0 10.0") is GridPosition.Parsed.Error)                // polar
        assertTrue(GridPosition.parse("hello") is GridPosition.Parsed.Error)
    }
}
