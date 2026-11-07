package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.link.LinkCodec
import com.projectrevivesolutions.layertime.controller.mission.SensorSelection
import com.projectrevivesolutions.layertime.controller.mission.SensorSelection.PresetState
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

/** The MISSION sensor selection (2026-11-04): groups, presets, migration, and the wire mask. */
class SensorSelectionTest {
    // ReconTarget numbers, contracts/vectors/enums.json.
    private val deauth = 5; private val pwnagotchi = 6; private val multiSsid = 7; private val flock = 8
    private val pineapple = 9; private val airTag = 10; private val flipper = 11; private val meta = 12
    private val axon = 13; private val tile = 14; private val smartTag = 15; private val googleTag = 16
    private val eapol = 18

    @Test fun groupsMatchTheHunterGroupTableInOrder() {
        // src/core/logic/ReconSelection.cpp, kTrackerMembers and the two after it.
        assertEquals(listOf(airTag, tile, smartTag, googleTag), SensorSelection.group(2)!!.members)
        assertEquals(listOf(flock, axon, meta), SensorSelection.group(3)!!.members)
        assertEquals(listOf(deauth, pwnagotchi, multiSsid, pineapple, flipper, eapol), SensorSelection.group(4)!!.members)
        assertEquals(13, SensorSelection.ALL_SENSORS.toSet().size) // every sensor once, EAPOL included
    }

    @Test fun allMaskIsTheHuntersAllSensorsMask() {
        assertEquals(0x5FFE0L, SensorSelection.mask(SensorSelection.ALL_SENSORS.toSet()))
        assertEquals(SensorSelection.ALL_MASK, SensorSelection.mask(SensorSelection.ALL_SENSORS.toSet()))
    }

    @Test fun presetTapChecksEveryMemberThenClearsThem() {
        val on = SensorSelection.togglePreset(2, setOf(deauth))
        assertEquals(setOf(deauth, airTag, tile, smartTag, googleTag), on)
        assertEquals(PresetState.ALL, SensorSelection.presetState(2, on))
        val off = SensorSelection.togglePreset(2, on)
        assertEquals(setOf(deauth), off)
        assertEquals(PresetState.NONE, SensorSelection.presetState(2, off))
    }

    @Test fun aPartialPresetShowsPartialAndATapFillsIt() {
        val some = setOf(flock)
        assertEquals(PresetState.PARTIAL, SensorSelection.presetState(3, some))
        assertEquals(setOf(flock, axon, meta), SensorSelection.togglePreset(3, some))
    }

    @Test fun uncheckingOneSensorBreaksTheFullPreset() {
        val all = SensorSelection.togglePreset(4, emptySet())
        assertEquals(PresetState.PARTIAL, SensorSelection.presetState(4, all - eapol))
    }

    @Test fun normalizeExpandsStoredPresetsAndDropsNonSensors() {
        assertEquals(setOf(airTag, tile, smartTag, googleTag, deauth),
            SensorSelection.normalize(setOf(2, deauth, 1, 17, 99)))
    }

    @Test fun migrationTurnsAnEmptyOrMissingOldListIntoEverySensorOn() {
        // Nothing stored starts with the trackers off (2026-11-06).
        val start = SensorSelection.DEFAULT_SENSORS
        assertEquals(start, SensorSelection.migrate(null))
        assertEquals(start, SensorSelection.migrate(emptySet()))
        assertEquals(start, SensorSelection.migrate(setOf(1, 17))) // nothing real in it
        assertEquals(setOf(flock, axon, meta, deauth), SensorSelection.migrate(setOf(3, deauth)))
    }

    @Test fun maskForNoTrackersMatchesTheFirmwareTest() {
        val noTrackers = SensorSelection.ALL_SENSORS.toSet() - SensorSelection.group(2)!!.members.toSet()
        assertEquals(0x43BE0L, SensorSelection.mask(noTrackers)) // test_link_server, set_sensors_...
        assertEquals(0L, SensorSelection.mask(emptySet()))
    }

    @Test fun setSensorsFrameIsSevenBytesLittleEndian() {
        // test_link_server sends 03 60 0F E0 3B 04 00 for this mask.
        assertArrayEquals(byteArrayOf(0x03, 0x60, 0x0F, 0xE0.toByte(), 0x3B, 0x04, 0x00),
            LinkCodec.setSensors(0x60, 0x43BE0L))
    }

    @Test fun trackersStartOff() {
        val trackers = SensorSelection.group(SensorSelection.TRACKERS)!!.members.toSet()
        assertEquals(setOf(10, 14, 15, 16), trackers) // AirTag, Tile, SmartTag, Google Tag
        assertEquals(9, SensorSelection.DEFAULT_SENSORS.size)
        assertTrue(SensorSelection.DEFAULT_SENSORS.none { it in trackers })
        assertEquals(SensorSelection.ALL_SENSORS.toSet() - trackers, SensorSelection.DEFAULT_SENSORS)
        // The same set the Hunter boots with (kHunterDefaultSensorMask in BringUpLogic.h).
        assertEquals(0x43BE0L, SensorSelection.mask(SensorSelection.DEFAULT_SENSORS))
    }

    @Test fun theOneTimeSwitchOffUnchecksOnlyTheTrackers() {
        val all = SensorSelection.ALL_SENSORS.toSet()
        assertEquals(SensorSelection.DEFAULT_SENSORS, SensorSelection.withoutTrackers(all))
        assertEquals(0x43BE0L, SensorSelection.mask(SensorSelection.withoutTrackers(all)))
        // A hand-picked list keeps everything but the trackers.
        assertEquals(setOf(deauth, flock), SensorSelection.withoutTrackers(setOf(deauth, flock, airTag, tile)))
        // A stored preset id expands first, so "Trackers" alone becomes nothing.
        assertEquals(emptySet<Int>(), SensorSelection.withoutTrackers(setOf(SensorSelection.TRACKERS)))
    }
}
