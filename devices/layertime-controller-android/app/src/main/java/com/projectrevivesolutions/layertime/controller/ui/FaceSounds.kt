package com.projectrevivesolutions.layertime.controller.ui

import android.content.Context
import android.media.AudioAttributes
import android.media.SoundPool
import com.projectrevivesolutions.layertime.controller.R

/**
 * The roll sound for the watch layout: gravel, then a slight thud as the face drops
 * into its groove (res/raw/face_slide_in.wav). One sound for both directions: it
 * plays when the face rolls out and when it rolls back (Michael, 2026-10-31: remove
 * the roll-out sound and use the roll-back sound for both). Loaded once into a
 * SoundPool and played on the media stream, so the watch's media volume controls it.
 * The roll-out sound, res/raw/face_slide_out.wav, was removed 2026-10-31.
 */
class FaceSounds(context: Context) {
    private val pool = SoundPool.Builder().setMaxStreams(2)
        .setAudioAttributes(AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_ASSISTANCE_SONIFICATION)
            .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION).build())
        .build()
    private val roll = pool.load(context, R.raw.face_slide_in, 1)
    private val loaded = HashSet<Int>()
    init { pool.setOnLoadCompleteListener { _, id, status -> if (status == 0) loaded.add(id) } }

    fun slideOut() = play()
    fun slideBack() = play()
    private fun play() { if (roll in loaded) pool.play(roll, 0.9f, 0.9f, 1, 0, 1f) }
    fun release() = pool.release()
}
