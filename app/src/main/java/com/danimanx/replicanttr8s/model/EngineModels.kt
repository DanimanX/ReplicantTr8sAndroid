package com.danimanx.replicanttr8s.model

import androidx.compose.runtime.Immutable

object ChannelParam {
    const val TUNE = 0
    const val DECAY = 1
    const val CTRL = 2
    const val LEVEL = 3
    const val PAN = 4
    const val REVERB = 5
    const val DELAY = 6
}

object MasterParam {
    const val VOLUME = 0
    const val DRIVE = 1
    const val REVERB = 2
    const val DELAY = 3
    const val FILTER = 4
    const val SCATTER_DEPTH = 5
}

val ChannelShortNames = listOf("BD", "SD", "LT", "MT", "HT", "RS", "HC", "CH", "OH", "CC", "RC")
val ChannelLongNames = listOf(
    "BASS DRUM", "SNARE DRUM", "LOW TOM", "MID TOM", "HIGH TOM", "RIM SHOT",
    "HAND CLAP", "CLOSED HI-HAT", "OPEN HI-HAT", "CRASH CYMBAL", "RIDE CYMBAL"
)
val ChannelNotes = listOf(36, 38, 43, 47, 50, 37, 39, 42, 46, 49, 51)

@Immutable
data class ChannelState(
    val index: Int,
    val name: String,
    val fullName: String,
    val toneId: Int,
    val toneName: String,
    val tune: Float,
    val decay: Float,
    val ctrl: Float,
    val level: Float,
    val pan: Float,
    val reverb: Float,
    val delay: Float,
    val muted: Boolean,
    val soloed: Boolean
)

@Immutable
data class MasterState(
    val volume: Float = 0.85f,
    val drive: Float = 0.10f,
    val reverb: Float = 0.50f,
    val delay: Float = 0.45f,
    val filter: Float = 0.50f,
    val scatterDepth: Float = 0.50f,
    val scatterType: Int = 1,
    val scatterActive: Boolean = false
)

@Immutable
data class AudioStats(
    val latencyMs: Double = 0.0,
    val cpuLoad: Float = 0.0f,
    val xRuns: Int = 0,
    val sampleRate: Int = 0,
    val framesPerBurst: Int = 0,
    val bufferFrames: Int = 0,
    val api: Int = 0,
    val sharing: Int = 0
) {
    val apiLabel: String
        get() = when (api) {
            1 -> "AAUDIO"
            2 -> "OPENSL ES"
            else -> "STARTING"
        }

    val sharingLabel: String
        get() = when (sharing) {
            1 -> "EXCLUSIVE"
            2 -> "SHARED"
            else -> "—"
        }

    val bufferMs: Double
        get() = if (sampleRate > 0) bufferFrames * 1000.0 / sampleRate else 0.0

    val isLowLatency: Boolean
        get() = api == 1 && sampleRate > 0
}

@Immutable
data class EngineUiState(
    val ready: Boolean = false,
    val audioRunning: Boolean = false,
    val silentFallback: Boolean = false,
    val engineError: String? = null,
    val kitIndex: Int = 1,
    val kitName: String = "TR-909 Studio",
    val patternIndex: Int = 103,
    val patternName: String = "Straight 909",
    val patternCode: String = "7-08",
    val variation: Int = 0,
    val selectedChannel: Int = 0,
    val playing: Boolean = false,
    val currentStep: Int = 0,
    val fill: Boolean = false,
    val tempo: Float = 128f,
    val shuffle: Float = 0f,
    val accent: Float = 0.65f,
    val channels: List<ChannelState> = emptyList(),
    val master: MasterState = MasterState(),
    val muteMask: Int = 0,
    val soloMask: Int = 0,
    val firmwareLoaded: Boolean = false,
    val firmwareName: String = "Built-in preview",
    val clockReceive: Boolean = false,
    val clockSend: Boolean = true,
    val transportSend: Boolean = true,
    val midiOut: Boolean = true,
    val externalClock: Boolean = false,
    val midiChannel: Int = -1,
    val patternRows: List<List<Int>> = List(12) { List(16) { 0 } },
    val audio: AudioStats = AudioStats()
) {
    fun channel(index: Int): ChannelState? = channels.getOrNull(index)

    fun velocity(track: Int, step: Int): Int =
        (patternRows.getOrNull(track)?.getOrNull(step) ?: 0) and 0xFF

    fun flags(track: Int, step: Int): Int =
        ((patternRows.getOrNull(track)?.getOrNull(step) ?: 0) shr 8) and 0xFF

    val variationLetter: String
        get() = ('A' + variation.coerceIn(0, 7)).toString()

    val channelCount: Int get() = channels.size
}

@Immutable
data class EngineCatalog(
    val kitNames: List<String> = emptyList(),
    val patternNames: List<String> = emptyList(),
    val patternCodes: List<String> = emptyList(),
    val toneNames: List<String> = emptyList(),
    val kitToneIds: List<List<Int>> = emptyList()
) {
    val ready: Boolean get() = kitNames.size >= 128 && patternNames.size >= 128

    fun kitToneNames(index: Int): List<String> {
        val ids = kitToneIds.getOrNull(index) ?: return emptyList()
        return ids.map { id -> toneNames.getOrElse(id) { "Tone $id" } }
    }
}

enum class MidiDirection { IN, OUT, SYSTEM }

@Immutable
data class MidiLogEntry(
    val id: Long,
    val direction: MidiDirection,
    val bytes: String,
    val description: String,
    val timestampMs: Long
)

@Immutable
data class MidiDeviceEntry(
    val id: Int,
    val name: String,
    val manufacturer: String,
    val typeLabel: String,
    val inputPorts: Int,
    val outputPorts: Int,
    val isUmp: Boolean,
    val isInputConnected: Boolean,
    val isOutputConnected: Boolean
)

@Immutable
data class MidiUiState(
    val available: Boolean = true,
    val umpSupported: Boolean = false,
    val scanning: Boolean = false,
    val devices: List<MidiDeviceEntry> = emptyList(),
    val inputDeviceId: Int? = null,
    val outputDeviceId: Int? = null,
    val inputDeviceName: String = "",
    val outputDeviceName: String = "",
    val inputConnected: Boolean = false,
    val outputConnected: Boolean = false,
    val midiInEnabled: Boolean = true,
    val midiOutEnabled: Boolean = true,
    val clockSend: Boolean = true,
    val clockReceive: Boolean = false,
    val transportSend: Boolean = true,
    val channelFilter: Int = -1,
    val useUmp: Boolean = false,
    val error: String? = null,
    val log: List<MidiLogEntry> = emptyList(),
    val lastRxMs: Long = 0L
)
