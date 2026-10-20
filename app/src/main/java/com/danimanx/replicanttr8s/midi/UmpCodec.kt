package com.danimanx.replicanttr8s.midi

/**
 * Minimal Universal MIDI Packet codec used for USB MIDI 2.0 devices.
 *
 * The engine speaks MIDI 1.0 messages internally and over the byte-stream
 * transport. On a UMP device we translate:
 *  - MIDI 2.0 Channel Voice (MT=4) -> MIDI 1.0 note/CC messages (7-bit)
 *  - MIDI 1.0 Channel Voice (MT=2) -> the same internal messages
 *  - MIDI 1.0 System Real-Time (MT=1) -> clock/start/stop
 *
 * Outgoing notes are sent as MIDI 2.0 Channel Voice with 16-bit velocity so
 * controllers with high-resolution pads keep their dynamics.
 */
object UmpCodec {

    data class Message(val status: Int, val data1: Int, val data2: Int)

    fun decode(bytes: ByteArray, offset: Int = 0, count: Int = bytes.size): List<Message> {
        val out = ArrayList<Message>(8)
        var pos = offset
        val end = (offset + count).coerceAtMost(bytes.size)
        while (pos + 4 <= end) {
            val w0 = readBe32(bytes, pos)
            val mt = (w0 ushr 28) and 0x0F
            when (mt) {
                0x0 -> pos += 4
                0x1 -> {
                    val status = (w0 ushr 16) and 0xFF
                    val d1 = (w0 ushr 8) and 0x7F
                    val d2 = w0 and 0x7F
                    if (status in 0xF0..0xFF) out += Message(status, d1, d2)
                    pos += 4
                }
                0x2 -> {
                    val status = (w0 ushr 16) and 0xFF
                    val d1 = (w0 ushr 8) and 0x7F
                    val d2 = w0 and 0x7F
                    out += Message(status, d1, d2)
                    pos += 4
                }
                0x3 -> pos += Math.min(8, end - pos)
                0x4 -> {
                    if (pos + 8 > end) break
                    val w1 = readBe32(bytes, pos + 4)
                    decodeMidi2(w0, w1)?.let { out += it }
                    pos += 8
                }
                0x5 -> pos += Math.min(16, end - pos)
                0x6, 0x7 -> pos += 4
                else -> pos += 4
            }
        }
        return out
    }

    private fun decodeMidi2(w0: Int, w1: Int): Message? {
        val status = (w0 ushr 16) and 0xFF
        if (status < 0x80) return null
        val index = (w0 ushr 8) and 0x7F
        return when (status and 0xF0) {
            0x80, 0x90 -> {
                val velocity16 = (w1 ushr 16) and 0xFFFF
                val velocity7 = (velocity16 * 127 + 32767) / 65535
                Message(status, index, velocity7)
            }
            0xA0 -> {
                val pressure = (w1 ushr 16) and 0xFFFF
                Message(status, index, (pressure * 127 + 32767) / 65535)
            }
            0xB0 -> {
                val value32 = w1.toLong() and 0xFFFFFFFFL
                val value7 = (value32 * 127L / 0xFFFFFFFFL).toInt()
                Message(status, index, value7)
            }
            else -> null
        }
    }

    /** Encode one engine message for a UMP output port. */
    fun encode(status: Int, data1: Int, data2: Int): ByteArray {
        if (status >= 0xF0) {
            val word = (0x1 shl 28) or ((status and 0xFF) shl 16) or
                ((data1 and 0x7F) shl 8) or (data2 and 0x7F)
            return be32(word)
        }
        return when (status and 0xF0) {
            0x80, 0x90 -> {
                val velocity16 = ((data2.coerceIn(0, 127) * 65535) + 63) / 127
                val w0 = (0x4 shl 28) or ((status and 0xFF) shl 16) or
                    ((data1 and 0x7F) shl 8)
                val w1 = (velocity16 and 0xFFFF) shl 16
                be64(w0, w1)
            }
            0xB0 -> {
                val value32 = (data2.coerceIn(0, 127).toLong() * 0xFFFFFFFFL / 127L)
                val w0 = (0x4 shl 28) or ((status and 0xFF) shl 16) or
                    ((data1 and 0x7F) shl 8)
                be64(w0, value32.toInt())
            }
            else -> {
                val word = (0x2 shl 28) or ((status and 0xFF) shl 16) or
                    ((data1 and 0x7F) shl 8) or (data2 and 0x7F)
                be32(word)
            }
        }
    }

    fun encodeMidi1(status: Int, data1: Int, data2: Int): ByteArray {
        if (status >= 0xF0) return byteArrayOf((status and 0xFF).toByte())
        return byteArrayOf(
            (status and 0xFF).toByte(),
            (data1 and 0x7F).toByte(),
            (data2 and 0x7F).toByte()
        )
    }

    private fun readBe32(bytes: ByteArray, at: Int): Int {
        return ((bytes[at].toInt() and 0xFF) shl 24) or
            ((bytes[at + 1].toInt() and 0xFF) shl 16) or
            ((bytes[at + 2].toInt() and 0xFF) shl 8) or
            (bytes[at + 3].toInt() and 0xFF)
    }

    private fun be32(word: Int): ByteArray = byteArrayOf(
        ((word ushr 24) and 0xFF).toByte(),
        ((word ushr 16) and 0xFF).toByte(),
        ((word ushr 8) and 0xFF).toByte(),
        (word and 0xFF).toByte()
    )

    private fun be64(w0: Int, w1: Int): ByteArray {
        val out = ByteArray(8)
        out[0] = ((w0 ushr 24) and 0xFF).toByte()
        out[1] = ((w0 ushr 16) and 0xFF).toByte()
        out[2] = ((w0 ushr 8) and 0xFF).toByte()
        out[3] = (w0 and 0xFF).toByte()
        out[4] = ((w1 ushr 24) and 0xFF).toByte()
        out[5] = ((w1 ushr 16) and 0xFF).toByte()
        out[6] = ((w1 ushr 8) and 0xFF).toByte()
        out[7] = (w1 and 0xFF).toByte()
        return out
    }
}
