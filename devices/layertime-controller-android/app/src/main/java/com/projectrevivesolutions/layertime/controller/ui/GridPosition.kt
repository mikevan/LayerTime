package com.projectrevivesolutions.layertime.controller.ui

import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.floor
import kotlin.math.pow
import kotlin.math.sin
import kotlin.math.sqrt
import kotlin.math.tan

/**
 * Position entry by hand: MGRS, UTM, or decimal latitude and longitude, on the
 * WGS84 ellipsoid. Pure Kotlin, so it is unit-tested on the JVM against reference
 * values from the GeoTrans-derived `mgrs` and PROJ (`pyproj`) libraries.
 *
 * Transverse Mercator uses the series in Snyder, "Map Projections: A Working
 * Manual" (USGS Professional Paper 1395, 1987), accurate to well under a metre
 * inside a UTM zone. MGRS uses the WGS84 ("AA") 100 km lettering. Polar regions
 * (UPS, south of 80 S and north of 84 N) are not supported and are refused.
 */
object GridPosition {
    data class LatLon(val lat: Double, val lon: Double)
    data class Utm(val zone: Int, val band: Char, val easting: Double, val northing: Double)

    /** A parse result: the position, or the reason it was refused. */
    sealed class Parsed {
        data class Ok(val position: LatLon, val format: String) : Parsed()
        data class Error(val message: String) : Parsed()
    }

    private const val A = 6378137.0
    private const val F = 1 / 298.257223563
    private val E2 = F * (2 - F)
    private val EP2 = E2 / (1 - E2)
    private const val K0 = 0.9996
    private const val BANDS = "CDEFGHJKLMNPQRSTUVWX"
    private const val ROWS = "ABCDEFGHJKLMNPQRSTUV"
    private val COLS = arrayOf("ABCDEFGH", "JKLMNPQR", "STUVWXYZ")

    private fun rad(d: Double) = Math.toRadians(d)
    private fun deg(r: Double) = Math.toDegrees(r)

    /** The UTM zone for a position, with the Norway (32V) and Svalbard (31X to 37X) exceptions. */
    fun zoneFor(lat: Double, lon: Double): Int {
        var z = floor((lon + 180) / 6).toInt() + 1
        if (z > 60) z = 60
        if (lat >= 56 && lat < 64 && lon >= 3 && lon < 12) z = 32
        if (lat >= 72 && lat < 84) {
            z = when {
                lon >= 0 && lon < 9 -> 31; lon >= 9 && lon < 21 -> 33
                lon >= 21 && lon < 33 -> 35; lon >= 33 && lon < 42 -> 37; else -> z
            }
        }
        return z
    }

    fun bandFor(lat: Double): Char = BANDS[((lat + 80) / 8).toInt().coerceIn(0, 19)]

    private fun meridionalArc(phi: Double): Double {
        val e4 = E2 * E2; val e6 = e4 * E2
        return A * ((1 - E2 / 4 - 3 * e4 / 64 - 5 * e6 / 256) * phi -
            (3 * E2 / 8 + 3 * e4 / 32 + 45 * e6 / 1024) * sin(2 * phi) +
            (15 * e4 / 256 + 45 * e6 / 1024) * sin(4 * phi) -
            (35 * e6 / 3072) * sin(6 * phi))
    }

    /** Latitude and longitude to UTM in the given zone (default: the position's own zone). */
    fun toUtm(lat: Double, lon: Double, zone: Int = zoneFor(lat, lon)): Utm {
        val phi = rad(lat)
        val lon0 = rad((zone - 1) * 6.0 - 180 + 3)
        val n = A / sqrt(1 - E2 * sin(phi).pow(2))
        val t = tan(phi).pow(2)
        val c = EP2 * cos(phi).pow(2)
        val a = cos(phi) * (rad(lon) - lon0)
        val m = meridionalArc(phi)
        val x = K0 * n * (a + (1 - t + c) * a.pow(3) / 6 + (5 - 18 * t + t * t + 72 * c - 58 * EP2) * a.pow(5) / 120) + 500000.0
        var y = K0 * (m + n * tan(phi) * (a * a / 2 + (5 - t + 9 * c + 4 * c * c) * a.pow(4) / 24 +
            (61 - 58 * t + t * t + 600 * c - 330 * EP2) * a.pow(6) / 720))
        if (lat < 0) y += 10_000_000.0
        return Utm(zone, bandFor(lat), x, y)
    }

    /** UTM to latitude and longitude; [south] is true for the southern hemisphere (bands C to M). */
    fun fromUtm(zone: Int, south: Boolean, easting: Double, northing: Double): LatLon {
        val x = easting - 500000.0
        val y = if (south) northing - 10_000_000.0 else northing
        val lon0 = rad((zone - 1) * 6.0 - 180 + 3)
        val m = y / K0
        val e4 = E2 * E2; val e6 = e4 * E2
        val mu = m / (A * (1 - E2 / 4 - 3 * e4 / 64 - 5 * e6 / 256))
        val e1 = (1 - sqrt(1 - E2)) / (1 + sqrt(1 - E2))
        val phi1 = mu + (3 * e1 / 2 - 27 * e1.pow(3) / 32) * sin(2 * mu) +
            (21 * e1 * e1 / 16 - 55 * e1.pow(4) / 32) * sin(4 * mu) +
            (151 * e1.pow(3) / 96) * sin(6 * mu) + (1097 * e1.pow(4) / 512) * sin(8 * mu)
        val n1 = A / sqrt(1 - E2 * sin(phi1).pow(2))
        val t1 = tan(phi1).pow(2)
        val c1 = EP2 * cos(phi1).pow(2)
        val r1 = A * (1 - E2) / (1 - E2 * sin(phi1).pow(2)).pow(1.5)
        val d = x / (n1 * K0)
        val lat = phi1 - (n1 * tan(phi1) / r1) * (d * d / 2 - (5 + 3 * t1 + 10 * c1 - 4 * c1 * c1 - 9 * EP2) * d.pow(4) / 24 +
            (61 + 90 * t1 + 298 * c1 + 45 * t1 * t1 - 252 * EP2 - 3 * c1 * c1) * d.pow(6) / 720)
        val lon = lon0 + (d - (1 + 2 * t1 + c1) * d.pow(3) / 6 +
            (5 - 2 * c1 + 28 * t1 - 3 * c1 * c1 + 8 * EP2 + 24 * t1 * t1) * d.pow(5) / 120) / cos(phi1)
        return LatLon(deg(lat), deg(lon))
    }

    private fun setOf(zone: Int) = ((zone - 1) % 6) + 1

    /** Latitude and longitude to MGRS at 1 m precision, for example "18S UJ 23478 06483". */
    fun toMgrs(lat: Double, lon: Double): String {
        val u = toUtm(lat, lon)
        val set = setOf(u.zone)
        val colIdx = floor(u.easting / 100000).toInt() - 1
        val col = COLS[(set - 1) % 3][colIdx.coerceIn(0, 7)]
        val rowOffset = if (set % 2 == 0) 5 else 0
        val row = ROWS[((floor(u.northing / 100000).toInt() + rowOffset) % 20 + 20) % 20]
        val e = (floor(u.easting).toLong() % 100000).toInt()
        val n = (floor(u.northing).toLong() % 100000).toInt()
        return "%d%c %c%c %05d %05d".format(u.zone, u.band, col, row, e, n)
    }

    /** Reads MGRS, UTM, or decimal latitude and longitude. */
    fun parse(input: String): Parsed {
        val s = input.trim().uppercase().replace(",", " ").replace(Regex("\\s+"), " ")
        if (s.isEmpty()) return Parsed.Error("Enter a position.")
        parseMgrs(s)?.let { return it }
        parseUtm(s)?.let { return it }
        parseDecimal(s)?.let { return it }
        return Parsed.Error("Use MGRS (18S UJ 23478 06483), UTM (18S 323478 4306483), or latitude and longitude (38.8895 -77.0353).")
    }

    private fun checkBand(zone: Int, band: Char): String? = when {
        zone !in 1..60 -> "The UTM zone must be 1 to 60."
        band !in BANDS -> "The latitude band must be a letter from C to X, without I or O. Polar areas are not supported."
        else -> null
    }

    private fun parseMgrs(s: String): Parsed? {
        val m = Regex("^(\\d{1,2}) ?([A-Z]) ?([A-Z])([A-Z]) ?(\\d+) ?(\\d*)$").find(s) ?: return null
        val zone = m.groupValues[1].toInt(); val band = m.groupValues[2][0]
        checkBand(zone, band)?.let { return Parsed.Error(it) }
        var digits = m.groupValues[5] + m.groupValues[6]
        if (m.groupValues[6].isNotEmpty() && m.groupValues[5].length != m.groupValues[6].length)
            return Parsed.Error("The MGRS easting and northing must have the same number of digits.")
        if (digits.length % 2 != 0 || digits.length !in 2..10)
            return Parsed.Error("An MGRS grid needs 2, 4, 6, 8, or 10 digits.")
        val half = digits.length / 2
        val scale = 10.0.pow(5 - half)
        val eDigits = digits.substring(0, half).toInt() * scale
        val nDigits = digits.substring(half).toInt() * scale
        val set = setOf(zone)
        val colIdx = COLS[(set - 1) % 3].indexOf(m.groupValues[3][0])
        if (colIdx < 0) return Parsed.Error("The 100 km column letter ${m.groupValues[3]} is not used in zone $zone.")
        val rowIdx = ROWS.indexOf(m.groupValues[4][0])
        if (rowIdx < 0) return Parsed.Error("The 100 km row letter ${m.groupValues[4]} is not valid.")
        val easting = (colIdx + 1) * 100000.0 + eDigits
        val rowOffset = if (set % 2 == 0) 5 else 0
        var northing = (((rowIdx - rowOffset) % 20 + 20) % 20) * 100000.0 + nDigits
        // The row letters repeat every 2,000 km; the band fixes which repeat is meant. The
        // band's lowest northing is on the central meridian.
        val bandLat = -80.0 + BANDS.indexOf(band) * 8
        val south = band < 'N'
        val minN = toUtm(bandLat, (zone - 1) * 6.0 - 180 + 3, zone).northing - 1000.0
        while (northing < minN) northing += 2_000_000.0
        val p = fromUtm(zone, south, easting, northing)
        if (bandFor(p.lat) != band && abs(p.lat - (bandLat + 4)) > 5)
            return Parsed.Error("That grid square is not in band $band.")
        return Parsed.Ok(p, "MGRS")
    }

    private fun parseUtm(s: String): Parsed? {
        val m = Regex("^(\\d{1,2}) ?([A-Z]) (\\d{3,7}(?:\\.\\d+)?)E? (\\d{1,8}(?:\\.\\d+)?)N?$").find(s) ?: return null
        val zone = m.groupValues[1].toInt(); val band = m.groupValues[2][0]
        checkBand(zone, band)?.let { return Parsed.Error(it) }
        val e = m.groupValues[3].toDouble(); val n = m.groupValues[4].toDouble()
        if (e !in 100_000.0..900_000.0) return Parsed.Error("A UTM easting is between 100000 and 900000 metres.")
        if (n !in 0.0..10_000_000.0) return Parsed.Error("A UTM northing is between 0 and 10000000 metres.")
        return Parsed.Ok(fromUtm(zone, band < 'N', e, n), "UTM")
    }

    private fun parseDecimal(s: String): Parsed? {
        val m = Regex("^(-?\\d{1,3}(?:\\.\\d+)?) ?([NS])? (-?\\d{1,3}(?:\\.\\d+)?) ?([EW])?$").find(s) ?: return null
        var lat = m.groupValues[1].toDouble(); var lon = m.groupValues[3].toDouble()
        if (m.groupValues[2] == "S") lat = -abs(lat)
        if (m.groupValues[4] == "W") lon = -abs(lon)
        if (lat !in -80.0..84.0) return Parsed.Error("Latitude must be between 80 S and 84 N. Polar areas are not supported.")
        if (lon !in -180.0..180.0) return Parsed.Error("Longitude must be between -180 and 180.")
        return Parsed.Ok(LatLon(lat, lon), "LAT/LON")
    }
}
