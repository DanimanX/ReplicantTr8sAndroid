package com.danimanx.replicanttr8s.midi

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class MidiStreamParserTest {

    @Test
    fun `parses channel messages with running status`() {
        val parser = MidiStreamParser()
        val messages = mutableListOf<Triple<Int, Int, Int>>()
        // Note on C3, then running-status note on D3, then note off.
        val bytes = intArrayOf(0x99, 36, 100, 38, 90, 0x89, 36, 0)
        bytes.forEach { b ->
            parser.feed(b, 1_000L) { status, d1, d2, _ -> messages += Triple(status, d1, d2) }
        }
        assertEquals(3, messages.size)
        assertEquals(Triple(0x99, 36, 100), messages[0])
        assertEquals(Triple(0x99, 38, 90), messages[1])
        assertEquals(Triple(0x89, 36, 0), messages[2])
    }

    @Test
    fun `realtime clock is emitted inside a channel message`() {
        val parser = MidiStreamParser()
        val messages = mutableListOf<Int>()
        // Note on, clock arrives between the status and its data bytes.
        intArrayOf(0x99, 36, 0xF8, 100).forEach { b ->
            parser.feed(b, 1L) { status, _, _, _ -> messages += status }
        }
        assertEquals(listOf(0xF8, 0x99), messages)
    }

    @Test
    fun `sysex payload is discarded and following message still parses`() {
        val parser = MidiStreamParser()
        val messages = mutableListOf<Triple<Int, Int, Int>>()
        intArrayOf(0xF0, 0x7E, 0x00, 0x06, 0x01, 0xF7, 0x90, 38, 77).forEach { b ->
            parser.feed(b, 1L) { status, d1, d2, _ -> messages += Triple(status, d1, d2) }
        }
        assertEquals(1, messages.size)
        assertEquals(Triple(0x90, 38, 77), messages[0])
    }

    @Test
    fun `program change has a single data byte`() {
        val parser = MidiStreamParser()
        var program = -1
        intArrayOf(0xC0, 42).forEach { b ->
            parser.feed(b, 1L) { status, d1, _, _ ->
                if (status and 0xF0 == 0xC0) program = d1
            }
        }
        assertEquals(42, program)
    }
}
