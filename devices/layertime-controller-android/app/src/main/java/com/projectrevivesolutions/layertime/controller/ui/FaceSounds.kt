package com.projectrevivesolutions.layertime.controller.ui

import android.content.Context
import android.media.AudioAttributes
import android.media.SoundPool
import com.projectrevivesolutions.layertime.controller.R

/**
 * The two slide sounds for the watch layout: a pneumatic hiss that sweeps down
 * and ends in a latch clank when the face slides out, and the reverse with a
 * heavier double latch when it slides back. Both are synthesized WAV files in
 * res/raw (about 25 KB each), loaded once into a SoundPool and played on the
 * media stream, so the watch's media volume controls them.
 */
class FaceSounds(context: Context) {
    private val pool = SoundPool.Builder().setMaxStreams(2)
        .setAudioAttributes(AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_ASSISTANCE_SONIFICATION)
            .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION).build())
        .build()
    private val out = pool.load(context, R.raw.face_slide_out, 1)
    private val back = pool.load(context, R.raw.face_slide_in, 1)
    private val loaded = HashSet<Int>()
    init { pool.setOnLoadCompleteListener { _, id, status -> if (status == 0) loaded.add(id) } }

    fun slideOut() { if (out in loaded) pool.play(out, 0.9f, 0.9f, 1, 0, 1f) }
    fun slideBack() { if (back in loaded) pool.play(back, 0.9f, 0.9f, 1, 0, 1f) }
    fun release() = pool.release()
}
