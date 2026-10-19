package com.haylen.plugins.{{PACKAGE}}

import android.Manifest
import android.annotation.SuppressLint
import android.content.pm.PackageManager
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import dev.haylen.HaylenAudioStream
import dev.haylen.HaylenBridge
import dev.haylen.HaylenPluginContext
import org.json.JSONObject

// The microphone of the {{TITLE}} plugin on Android: `AudioRecord` into the audio stream `microphone`, after the person allowed the microphone.
class {{NAME}}Microphone(private val context: HaylenPluginContext, private val permissions: {{NAME}}Permissions) {
    private var recording: Recording? = null

    fun register() {
        context.register("startMicrophone") { _, reply ->
            if (!context.application().packageManager.hasSystemFeature(PackageManager.FEATURE_MICROPHONE)) {
                throw HaylenBridge.Failure("This device has no microphone.", "unsupported", null)
            }
            permissions.withPermission(context, Manifest.permission.RECORD_AUDIO, reply) {
                recording?.stop()
                recording = null
                val record = Recording.open() ?: throw HaylenBridge.Failure("The microphone of this device does not record.", "unsupported", null)
                val stream = context.openAudioStream("microphone", Recording.SAMPLE_RATE, 1, HaylenAudioStream.Format.FLOAT32, Recording.SAMPLE_RATE)
                recording = Recording(record, stream)
                reply.success(JSONObject().put("sampleRate", Recording.SAMPLE_RATE).put("channels", 1))
            }
        }

        context.register("stopMicrophone") { _, reply ->
            recording?.stop()
            recording = null
            reply.success(null)
        }
    }

    // The recording of `AudioRecord`, whose mono floats a thread of its own reads in blocks of a hundredth of a second and pushes into the stream.
    private class Recording(private val record: AudioRecord, private val stream: HaylenAudioStream) {
        @Volatile
        private var running = true

        init {
            Thread({
                val block = FloatArray(SAMPLE_RATE / 100)
                record.startRecording()
                while (running) {
                    val frames = record.read(block, 0, block.size, AudioRecord.READ_BLOCKING)
                    if (frames > 0) {
                        stream.push(block, frames)
                    }
                }
                record.stop()
                record.release()
            }, "{{ID}}-microphone").start()
        }

        fun stop() {
            running = false
        }

        companion object {
            const val SAMPLE_RATE = 44100

            // Returns a recorder of the microphone, or nothing when the device does not record, as an emulator without audio input.
            @SuppressLint("MissingPermission")
            fun open(): AudioRecord? {
                val size = maxOf(AudioRecord.getMinBufferSize(SAMPLE_RATE, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_FLOAT), SAMPLE_RATE / 5 * 4)
                val record = AudioRecord(MediaRecorder.AudioSource.MIC, SAMPLE_RATE, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_FLOAT, size)
                if (record.state == AudioRecord.STATE_INITIALIZED) {
                    return record
                }
                record.release()
                return null
            }
        }
    }
}
