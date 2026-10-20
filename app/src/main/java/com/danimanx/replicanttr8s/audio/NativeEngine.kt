package com.danimanx.replicanttr8s.audio

/**
 * Thin JNI surface for the native rhythm engine. All functions are cheap and
 * non-blocking except [nativeLoadFirmware], which parses a user-supplied
 * firmware image on the calling thread.
 */
object NativeEngine {
    init {
        System.loadLibrary("replicant_engine")
    }

    external fun nativeCreate(): Long
    external fun nativeDestroy(handle: Long)
    external fun nativeVersion(): String

    external fun nativeStartAudio(handle: Long): Boolean
    external fun nativeStopAudio(handle: Long)
    external fun nativeIsAudioRunning(handle: Long): Boolean
    external fun nativeRenderSilent(handle: Long, frames: Int)
    external fun nativeGetAudioStats(handle: Long, floats: FloatArray, ints: IntArray)

    external fun nativeGetParams(handle: Long, out: FloatArray)
    external fun nativeGetStatus(handle: Long, out: IntArray)
    external fun nativeGetScope(handle: Long, out: FloatArray)
    external fun nativeGetMeters(handle: Long, out: FloatArray)
    external fun nativeGetPatternRow(handle: Long, variation: Int, track: Int, out: ByteArray)
    external fun nativeGetStep(handle: Long, variation: Int, track: Int, step: Int): Int

    external fun nativeSetPlaying(handle: Long, playing: Boolean)
    external fun nativeSetTempo(handle: Long, bpm: Double)
    external fun nativeSetShuffle(handle: Long, value: Float)
    external fun nativeSetAccent(handle: Long, value: Float)
    external fun nativeSetVariation(handle: Long, variation: Int)
    external fun nativeSetFill(handle: Long, fill: Boolean)
    external fun nativeToggleFill(handle: Long)
    external fun nativeSetSelectedChannel(handle: Long, channel: Int)
    external fun nativeSetChannelParam(handle: Long, channel: Int, param: Int, value: Float)
    external fun nativeSetMasterParam(handle: Long, param: Int, value: Float)
    external fun nativeSetTone(handle: Long, channel: Int, toneId: Int)
    external fun nativeSetMute(handle: Long, channel: Int, muted: Boolean)
    external fun nativeSetSolo(handle: Long, channel: Int, soloed: Boolean)
    external fun nativeLoadKit(handle: Long, index: Int)
    external fun nativeLoadPattern(handle: Long, index: Int, loadKit: Boolean)
    external fun nativeCopyVariation(handle: Long, src: Int, dst: Int)
    external fun nativeClearVariation(handle: Long, variation: Int)
    external fun nativeSetStep(handle: Long, variation: Int, track: Int, step: Int, velocity: Int, flags: Int)
    external fun nativeTrigger(handle: Long, channel: Int, velocity: Float, flam: Boolean)
    external fun nativeSetScatterActive(handle: Long, active: Boolean)
    external fun nativeSetScatterType(handle: Long, type: Int)

    external fun nativePushMidi(handle: Long, status: Int, data1: Int, data2: Int, timestampNanos: Long)
    external fun nativePollMidiOut(handle: Long, out: ByteArray): Int
    external fun nativeSetMidiOutEnabled(handle: Long, enabled: Boolean)
    external fun nativeSetClockSend(handle: Long, enabled: Boolean)
    external fun nativeSetClockReceive(handle: Long, enabled: Boolean)
    external fun nativeSetTransportSend(handle: Long, enabled: Boolean)
    external fun nativeSetMidiChannel(handle: Long, channel: Int)

    external fun nativeLoadFirmware(handle: Long, data: ByteArray): String
    external fun nativeClearFirmware(handle: Long)
    external fun nativeIsFirmwareLoaded(handle: Long): Boolean
    external fun nativeFirmwareName(handle: Long): String
    external fun nativeKitName(handle: Long, index: Int): String
    external fun nativePatternName(handle: Long, index: Int): String
    external fun nativePatternCode(handle: Long, index: Int): String
    external fun nativeToneName(handle: Long, index: Int): String
    external fun nativeKitToneId(handle: Long, kitIndex: Int, channel: Int): Int
}
