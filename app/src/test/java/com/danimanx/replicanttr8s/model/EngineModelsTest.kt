package com.danimanx.replicanttr8s.model

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class EngineModelsTest {

    @Test
    fun `velocity and flags are unpacked from the pattern cell`() {
        val rows = List(12) { List(16) { 0 } }.toMutableList()
        rows[0] = List(16) { 0 }.toMutableList().also { it[4] = 85 or (2 shl 8) }
        val state = EngineUiState(patternRows = rows)
        assertEquals(85, state.velocity(0, 4))
        assertEquals(2, state.flags(0, 4))
        assertEquals(0, state.velocity(0, 0))
    }

    @Test
    fun `variation letter follows the selected variation`() {
        assertEquals("A", EngineUiState(variation = 0).variationLetter)
        assertEquals("H", EngineUiState(variation = 7).variationLetter)
    }

    @Test
    fun `audio stats expose a readable low latency description`() {
        val stats = AudioStats(
            latencyMs = 6.4,
            sampleRate = 48000,
            bufferFrames = 192,
            framesPerBurst = 192,
            api = 1,
            sharing = 1
        )
        assertEquals("AAUDIO", stats.apiLabel)
        assertEquals("EXCLUSIVE", stats.sharingLabel)
        assertEquals(4.0, stats.bufferMs, 0.01)
        assertTrue(stats.isLowLatency)
        assertFalse(AudioStats().isLowLatency)
    }

    @Test
    fun `channel table matches the GM drum map`() {
        assertEquals(11, ChannelShortNames.size)
        assertEquals(36, ChannelNotes[0])
        assertEquals(51, ChannelNotes[10])
        assertEquals("CH", ChannelShortNames[7])
    }
}
