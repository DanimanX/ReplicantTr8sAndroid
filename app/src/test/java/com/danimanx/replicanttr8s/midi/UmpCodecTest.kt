package com.danimanx.replicanttr8s.midi

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class UmpCodecTest {

    @Test
    fun `decodes midi 1 channel voice packet in mt2`() {
        // MT=2, group 0, note on channel 1, note 36, velocity 100.
        val word = (0x2 shl 28) or (0x90 shl 16) or (36 shl 8) or 100
        val bytes = byteArrayOf(
            ((word ushr 24) and 0xFF).toByte(),
            ((word ushr 16) and 0xFF).toByte(),
            ((word ushr 8) and 0xFF).toByte(),
            (word and 0xFF).toByte()
        )
        val decoded = UmpCodec.decode(bytes)
        assertEquals(1, decoded.size)
        assertEquals(0x90, decoded[0].status)
        assertEquals(36, decoded[0].data1)
        assertEquals(100, decoded[0].data2)
    }

    @Test
    fun `decodes system realtime clock from mt1`() {
        val bytes = UmpCodec.encode(0xF8, 0, 0)
        val decoded = UmpCodec.decode(bytes)
        assertEquals(1, decoded.size)
        assertEquals(0xF8, decoded[0].status)
    }

    @Test
    fun `midi 2 note round trips with 16 bit velocity`() {
        val bytes = UmpCodec.encode(0x92, 43, 127)
        assertEquals(8, bytes.size)
        val decoded = UmpCodec.decode(bytes)
        assertEquals(1, decoded.size)
        assertEquals(0x92, decoded[0].status)
        assertEquals(43, decoded[0].data1)
        assertEquals(127, decoded[0].data2)
    }

    @Test
    fun `midi 2 cc is scaled back to seven bits`() {
        val bytes = UmpCodec.encode(0xB5, 74, 64)
        val decoded = UmpCodec.decode(bytes)
        assertEquals(1, decoded.size)
        assertEquals(0xB5, decoded[0].status)
        assertEquals(74, decoded[0].data1)
        assertTrue(decoded[0].data2 in 63..65)
    }

    @Test
    fun `start and stop are one word system messages`() {
        assertEquals(4, UmpCodec.encode(0xFA, 0, 0).size)
        assertEquals(4, UmpCodec.encode(0xFC, 0, 0).size)
        assertEquals(0xFA, UmpCodec.decode(UmpCodec.encode(0xFA, 0, 0))[0].status)
    }

    @Test
    fun `truncated packets do not throw`() {
        val decoded = UmpCodec.decode(byteArrayOf(0x40, 0x90.toByte(), 0x24, 0x64))
        assertTrue(decoded.isEmpty())
    }
}
