package com.projectrevivesolutions.layertime.controller.link

/**
 * Recon mode and count wording for the face, copied from the Garmin binding
 * (devices/garmin-tactix/source/ReconNames.mc), which in turn copies
 * contracts/vectors/enums.json. Keep in step with that table.
 */
object ReconText {
    val SHORT = listOf(
        "STOPPED", "ALL", "TRACKERS", "SURVEIL", "INTRUSION", "DEAUTH", "PWNAGOTCHI", "MULTISSID",
        "FLOCK", "PINEAPPLE", "AIRTAG", "FLIPPER", "META", "AXON", "TILE", "SMARTTAG", "GOOGLE TAG",
        "EARLY WARN"
    )

    fun shortName(target: Int): String = SHORT.getOrElse(target) { "?" }

    /** The Recon mode line from Status flags and the selection, as the face shows it. */
    fun modeText(flags: Int, selected: Int): String {
        if (flags and LinkConstants.StatusFlag.MONITORING != 0) return "RECON " + shortName(selected)
        if (flags and LinkConstants.StatusFlag.EARLY_WARNING_ENABLED != 0) {
            return if (flags and LinkConstants.StatusFlag.EARLY_WARNING_RESTING != 0) "EARLY WARN  REST" else "EARLY WARN"
        }
        return "RECON OFF"
    }

    fun countText(n: Int): String = if (n == 1) "1 DETECTION" else "$n DETECTIONS"
}
