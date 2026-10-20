package com.danimanx.replicanttr8s.midi

/**
 * Running-status-aware MIDI 1.0 byte-stream parser. Sysex is consumed and
 * discarded; real-time bytes are emitted immediately even when they interrupt
 * another message.
 */
class MidiStreamParser {
    private var runningStatus = 0
    private val data = IntArray(2)
    private var dataCount = 0
    private var expected = 0
    private var inSysEx = false

    fun reset() {
        runningStatus = 0
        dataCount = 0
        expected = 0
        inSysEx = false
    }

    fun feed(value: Int, timestampNanos: Long, onMessage: (Int, Int, Int, Long) -> Unit) {
        val b = value and 0xFF
        if (b >= 0xF8) {
            onMessage(b, 0, 0, timestampNanos)
            return
        }
        if (b >= 0x80) {
            if (b == 0xF0) {
                inSysEx = true
                runningStatus = 0
                dataCount = 0
                expected = 0
                return
            }
            if (b == 0xF7) {
                inSysEx = false
                runningStatus = 0
                dataCount = 0
                expected = 0
                return
            }
            if (inSysEx) return
            if (b >= 0xF1 && b <= 0xF6) {
                runningStatus = b
                dataCount = 0
                expected = systemLength(b)
                if (expected == 0) {
                    onMessage(b, 0, 0, timestampNanos)
                    runningStatus = 0
                }
                return
            }
            runningStatus = b
            dataCount = 0
            expected = channelLength(b)
            if (expected == 0) {
                onMessage(b, 0, 0, timestampNanos)
                runningStatus = 0
            }
            return
        }

        if (inSysEx || runningStatus == 0) return
        if (dataCount < data.size) data[dataCount] = b
        dataCount++
        if (dataCount >= expected) {
            onMessage(
                runningStatus,
                data[0],
                if (expected > 1) data[1] else 0,
                timestampNanos
            )
            dataCount = 0
            if (runningStatus and 0xF0 == 0xF0) runningStatus = 0
        }
    }

    private fun channelLength(status: Int): Int = when (status and 0xF0) {
        0xC0, 0xD0 -> 1
        else -> 2
    }

    private fun systemLength(status: Int): Int = when (status) {
        0xF1, 0xF3 -> 1
        0xF2 -> 2
        0xF6 -> 0
        else -> 0
    }
}
