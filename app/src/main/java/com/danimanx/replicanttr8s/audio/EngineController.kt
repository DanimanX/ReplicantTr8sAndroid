package com.danimanx.replicanttr8s.audio

import android.content.Context
import android.net.Uri
import com.danimanx.replicanttr8s.model.AudioStats
import com.danimanx.replicanttr8s.model.ChannelParam
import com.danimanx.replicanttr8s.model.ChannelShortNames
import com.danimanx.replicanttr8s.model.ChannelState
import com.danimanx.replicanttr8s.model.EngineCatalog
import com.danimanx.replicanttr8s.model.EngineUiState
import com.danimanx.replicanttr8s.model.MasterParam
import com.danimanx.replicanttr8s.model.MasterState
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.File
import java.util.concurrent.ConcurrentHashMap
import java.util.zip.ZipInputStream
import kotlin.math.roundToInt

/**
 * Owns the native engine handle, the low-latency audio stream and the polling
 * loop that turns native state into immutable UI state.
 */
class EngineController(context: Context) {

    private val appContext = context.applicationContext
    private val prefs = appContext.getSharedPreferences("engine", Context.MODE_PRIVATE)
    private val workScope = CoroutineScope(SupervisorJob() + Dispatchers.Default)
    private val handle: Long = NativeEngine.nativeCreate()

    private val _state = kotlinx.coroutines.flow.MutableStateFlow(EngineUiState())
    val state: kotlinx.coroutines.flow.StateFlow<EngineUiState> = _state

    private val _meters = kotlinx.coroutines.flow.MutableStateFlow(List(13) { 0f })
    val meters: kotlinx.coroutines.flow.StateFlow<List<Float>> = _meters

    private val _scope = kotlinx.coroutines.flow.MutableStateFlow(List(128) { 0f })
    val scope: kotlinx.coroutines.flow.StateFlow<List<Float>> = _scope

    private val _catalog = kotlinx.coroutines.flow.MutableStateFlow(EngineCatalog())
    val catalog: kotlinx.coroutines.flow.StateFlow<EngineCatalog> = _catalog

    private val params = FloatArray(96)
    private val status = IntArray(64)
    private val metersBuffer = FloatArray(13)
    private val scopeBuffer = FloatArray(128)
    private val statsFloats = FloatArray(2)
    private val statsInts = IntArray(6)
    private val rowBuffer = ByteArray(32)

    private val kitNameCache = ConcurrentHashMap<Int, String>()
    private val patternNameCache = ConcurrentHashMap<Int, String>()
    private val patternCodeCache = ConcurrentHashMap<Int, String>()
    private val toneNameCache = ConcurrentHashMap<Int, String>()

    private val firmwareFile = File(appContext.filesDir, "firmware/TR8S_UP.bin")
    private var pollJob: Job? = null
    private var silentJob: Job? = null
    private var catalogJob: Job? = null
    private var started = false
    private var loadedRowsVariation = -1
    private var loadedRowsPattern = -1

    fun start() {
        synchronized(this) {
            if (started) return
            started = true
        }

        val savedFirmware = firmwareFile.takeIf { it.isFile && it.length() > 64 }
        if (savedFirmware != null) {
            runCatching {
                val bytes = savedFirmware.readBytes()
                val error = NativeEngine.nativeLoadFirmware(handle, bytes)
                if (error.isNotEmpty()) {
                    _state.value = _state.value.copy(engineError = "Saved firmware could not be loaded: $error")
                }
            }
        }

        val audioStarted = NativeEngine.nativeStartAudio(handle)
        if (!audioStarted) {
            _state.value = _state.value.copy(
                engineError = "No low-latency audio device available. The sequencer is running in silent clock mode."
            )
        }

        startSilentTicker()

        pollJob = workScope.launch { pollLoop() }
        reloadCatalog()
        reloadPatternRows(_state.value.variation)
    }

    fun ensureAudio(): Boolean {
        val running = NativeEngine.nativeStartAudio(handle)
        if (!running) startSilentTicker()
        return running
    }

    private fun startSilentTicker() {
        if (silentJob?.isActive == true) return
        // If the device cannot open an audio stream (or a stream dies), this
        // ticker keeps the sequencer, MIDI clock and UI alive. It is a no-op
        // while the Oboe callback owns the engine.
        silentJob = workScope.launch {
            while (isActive) {
                NativeEngine.nativeRenderSilent(handle, 512)
                delay(10)
            }
        }
    }

    fun stopAudio() {
        silentJob?.cancel()
        silentJob = null
        NativeEngine.nativeStopAudio(handle)
        _state.value = _state.value.copy(audioRunning = false, silentFallback = true)
    }

    fun destroy() {
        pollJob?.cancel()
        silentJob?.cancel()
        catalogJob?.cancel()
        NativeEngine.nativeStopAudio(handle)
        NativeEngine.nativeDestroy(handle)
        workScope.cancel()
    }

    private suspend fun pollLoop() {
        while (workScope.isActive) {
            try {
                val newState = readState()
                if (newState != _state.value) _state.value = newState
                _meters.value = metersBuffer.toList()
                _scope.value = scopeBuffer.toList()
            } catch (_: Throwable) {
                // Never let a transient JNI issue kill the UI loop.
            }
            delay(30)
        }
    }

    private fun readState(): EngineUiState {
        NativeEngine.nativeGetParams(handle, params)
        NativeEngine.nativeGetStatus(handle, status)
        NativeEngine.nativeGetMeters(handle, metersBuffer)
        NativeEngine.nativeGetScope(handle, scopeBuffer)
        NativeEngine.nativeGetAudioStats(handle, statsFloats, statsInts)

        val variation = status[2].coerceIn(0, 7)
        val pattern = status[1].coerceIn(0, 127)
        if (variation != loadedRowsVariation || pattern != loadedRowsPattern) {
            reloadPatternRows(variation, pattern)
        }

        val muteMask = status[27]
        val soloMask = status[28]
        val channels = (0 until 11).map { ch ->
            val base = ch * 7
            val toneId = status[16 + ch]
            ChannelState(
                index = ch,
                name = ChannelShortNames[ch],
                fullName = com.danimanx.replicanttr8s.model.ChannelLongNames[ch],
                toneId = toneId,
                toneName = toneName(toneId),
                tune = params[base + ChannelParam.TUNE],
                decay = params[base + ChannelParam.DECAY],
                ctrl = params[base + ChannelParam.CTRL],
                level = params[base + ChannelParam.LEVEL],
                pan = params[base + ChannelParam.PAN],
                reverb = params[base + ChannelParam.REVERB],
                delay = params[base + ChannelParam.DELAY],
                muted = (muteMask and (1 shl ch)) != 0,
                soloed = (soloMask and (1 shl ch)) != 0
            )
        }

        val audio = AudioStats(
            latencyMs = statsFloats[0].toDouble(),
            cpuLoad = statsFloats[1],
            xRuns = statsInts[0],
            sampleRate = statsInts[1],
            framesPerBurst = statsInts[2],
            bufferFrames = statsInts[3],
            api = statsInts[4],
            sharing = statsInts[5]
        )

        val kit = status[0].coerceIn(0, 127)
        val audioRunning = NativeEngine.nativeIsAudioRunning(handle)
        val staleError = _state.value.engineError
        return EngineUiState(
            ready = true,
            audioRunning = audioRunning,
            silentFallback = !audioRunning,
            engineError = if (audioRunning && staleError?.startsWith("No low-latency") == true) null else staleError,
            kitIndex = kit,
            kitName = kitName(kit),
            patternIndex = pattern,
            patternName = patternName(pattern),
            patternCode = patternCode(pattern),
            variation = variation,
            selectedChannel = status[3].coerceIn(0, 10),
            playing = status[4] != 0,
            currentStep = status[5].coerceIn(0, 15),
            fill = status[6] != 0,
            tempo = params[77],
            shuffle = params[78],
            accent = params[79],
            channels = channels,
            master = MasterState(
                volume = params[80 + MasterParam.VOLUME],
                drive = params[80 + MasterParam.DRIVE],
                reverb = params[80 + MasterParam.REVERB],
                delay = params[80 + MasterParam.DELAY],
                filter = params[80 + MasterParam.FILTER],
                scatterDepth = params[80 + MasterParam.SCATTER_DEPTH],
                scatterType = status[8],
                scatterActive = status[7] != 0
            ),
            muteMask = muteMask,
            soloMask = soloMask,
            firmwareLoaded = status[9] != 0,
            firmwareName = NativeEngine.nativeFirmwareName(handle),
            clockReceive = status[10] != 0,
            clockSend = status[11] != 0,
            transportSend = status[12] != 0,
            midiOut = status[13] != 0,
            externalClock = status[14] != 0,
            midiChannel = status[15],
            patternRows = _state.value.patternRows,
            audio = audio
        )
    }

    private fun reloadPatternRows(variation: Int, pattern: Int = status[1].coerceIn(0, 127)) {
        val rows = List(12) { track ->
            NativeEngine.nativeGetPatternRow(handle, variation, track, rowBuffer)
            List(16) { step ->
                (rowBuffer[step * 2].toInt() and 0xFF) or ((rowBuffer[step * 2 + 1].toInt() and 0xFF) shl 8)
            }
        }
        loadedRowsVariation = variation
        loadedRowsPattern = pattern
        _state.value = _state.value.copy(patternRows = rows)
    }

    fun reloadCatalog() {
        catalogJob?.cancel()
        catalogJob = workScope.launch {
            val kits = withContext(Dispatchers.IO) { (0 until 128).map { kitName(it) } }
            val patterns = withContext(Dispatchers.IO) { (0 until 128).map { patternName(it) } }
            val codes = withContext(Dispatchers.IO) { (0 until 128).map { patternCode(it) } }
            val tones = withContext(Dispatchers.IO) { (0 until 522).map { toneName(it) } }
            val kitTones = withContext(Dispatchers.IO) {
                (0 until 128).map { kit ->
                    (0 until 11).map { ch -> NativeEngine.nativeKitToneId(handle, kit, ch) }
                }
            }
            _catalog.value = EngineCatalog(kits, patterns, codes, tones, kitTones)
        }
    }

    private fun kitName(index: Int): String =
        kitNameCache.getOrPut(index) { NativeEngine.nativeKitName(handle, index) }

    private fun patternName(index: Int): String =
        patternNameCache.getOrPut(index) { NativeEngine.nativePatternName(handle, index) }

    private fun patternCode(index: Int): String =
        patternCodeCache.getOrPut(index) { NativeEngine.nativePatternCode(handle, index) }

    private fun toneName(index: Int): String =
        toneNameCache.getOrPut(index) { NativeEngine.nativeToneName(handle, index) }

    // ---------------------------------------------------------------------
    // Transport and performance
    // ---------------------------------------------------------------------

    fun setPlaying(playing: Boolean) = NativeEngine.nativeSetPlaying(handle, playing)
    fun togglePlaying() = setPlaying(!_state.value.playing)
    fun setTempo(bpm: Float) = NativeEngine.nativeSetTempo(handle, bpm.coerceIn(40f, 300f).toDouble())
    fun setShuffle(value: Float) = NativeEngine.nativeSetShuffle(handle, value)
    fun setAccent(value: Float) = NativeEngine.nativeSetAccent(handle, value)
    fun setVariation(variation: Int) {
        val clamped = variation.coerceIn(0, 7)
        NativeEngine.nativeSetVariation(handle, clamped)
        _state.value = _state.value.copy(variation = clamped)
        reloadPatternRows(clamped)
    }
    fun setFill(fill: Boolean) = NativeEngine.nativeSetFill(handle, fill)
    fun toggleFill() = NativeEngine.nativeToggleFill(handle)
    fun selectChannel(channel: Int) {
        NativeEngine.nativeSetSelectedChannel(handle, channel.coerceIn(0, 10))
        _state.value = _state.value.copy(selectedChannel = channel.coerceIn(0, 10))
    }
    fun trigger(channel: Int, velocity: Float = 0.95f) =
        NativeEngine.nativeTrigger(handle, channel, velocity, false)
    fun triggerFlam(channel: Int, velocity: Float = 0.95f) =
        NativeEngine.nativeTrigger(handle, channel, velocity, true)

    fun setPlayingWithFill() {
        if (!_state.value.playing) {
            NativeEngine.nativeSetFill(handle, true)
        }
        setPlaying(true)
    }

    // ---------------------------------------------------------------------
    // Channel, kit and pattern parameters
    // ---------------------------------------------------------------------

    fun setChannelParam(channel: Int, param: Int, value: Float) =
        NativeEngine.nativeSetChannelParam(handle, channel, param, value.coerceIn(0f, 1f))

    fun setMasterParam(param: Int, value: Float) =
        NativeEngine.nativeSetMasterParam(handle, param, value)

    fun setTone(channel: Int, toneId: Int) {
        NativeEngine.nativeSetTone(handle, channel, toneId)
        toneNameCache.clear()
    }

    fun setMute(channel: Int, muted: Boolean) = NativeEngine.nativeSetMute(handle, channel, muted)
    fun setSolo(channel: Int, soloed: Boolean) = NativeEngine.nativeSetSolo(handle, channel, soloed)
    fun loadKit(index: Int) {
        NativeEngine.nativeLoadKit(handle, index)
        _state.value = _state.value.copy(kitIndex = index, kitName = kitName(index))
    }

    fun loadPattern(index: Int, loadKit: Boolean = true) {
        NativeEngine.nativeLoadPattern(handle, index, loadKit)
        _state.value = _state.value.copy(
            patternIndex = index,
            patternName = patternName(index),
            patternCode = patternCode(index),
            fill = false
        )
        reloadPatternRows(_state.value.variation, index)
    }

    fun clearVariation() {
        NativeEngine.nativeClearVariation(handle, _state.value.variation)
        reloadPatternRows(_state.value.variation)
    }

    fun copyVariation() {
        val src = _state.value.variation
        val dst = (src + 1) % 8
        NativeEngine.nativeCopyVariation(handle, src, dst)
        NativeEngine.nativeSetVariation(handle, dst)
        _state.value = _state.value.copy(variation = dst)
        reloadPatternRows(dst)
    }

    fun toggleStep(track: Int, step: Int, accentMode: Boolean = false) {
        val current = _state.value.velocity(track, step)
        val velocity = when {
            !accentMode -> if (current > 0) 0 else 85
            current == 0 -> 100
            current >= 100 -> 50
            else -> 100
        }
        val flags = _state.value.flags(track, step)
        setStep(track, step, velocity, flags)
    }

    fun cycleVelocity(track: Int, step: Int) {
        val current = _state.value.velocity(track, step)
        val velocity = when {
            current == 0 -> 85
            current < 70 -> 50
            current < 100 -> 115
            else -> 0
        }
        setStep(track, step, velocity, _state.value.flags(track, step))
    }

    fun setStep(track: Int, step: Int, velocity: Int, flags: Int = 0) {
        val variation = _state.value.variation
        NativeEngine.nativeSetStep(handle, variation, track, step, velocity.coerceIn(0, 127), flags)
        val rows = _state.value.patternRows.toMutableList()
        val row = rows.getOrNull(track)?.toMutableList() ?: MutableList(16) { 0 }
        row[step] = (velocity.coerceIn(0, 127)) or ((flags and 0xFF) shl 8)
        rows[track] = row
        _state.value = _state.value.copy(patternRows = rows)
    }

    fun toggleAccentStep(step: Int) {
        val current = _state.value.velocity(11, step)
        setStep(11, step, if (current > 0) 0 else 100, 0)
    }

    fun setScatterActive(active: Boolean) = NativeEngine.nativeSetScatterActive(handle, active)
    fun setScatterType(type: Int) = NativeEngine.nativeSetScatterType(handle, type.coerceIn(1, 8))

    // ---------------------------------------------------------------------
    // MIDI plumbing
    // ---------------------------------------------------------------------

    fun pushMidi(status: Int, data1: Int, data2: Int, timestampNanos: Long) =
        NativeEngine.nativePushMidi(handle, status, data1, data2, timestampNanos)

    fun pollMidiOut(maxRecords: Int = 64): List<MidiOutRecord> {
        if (maxRecords <= 0) return emptyList()
        val bytes = ByteArray(maxRecords * 16)
        val count = NativeEngine.nativePollMidiOut(handle, bytes)
        if (count <= 0) return emptyList()
        return (0 until count).map { i ->
            val base = i * 16
            var timestamp = 0L
            for (b in 0 until 8) {
                timestamp = timestamp or ((bytes[base + 8 + b].toLong() and 0xFF) shl (8 * b))
            }
            MidiOutRecord(
                status = bytes[base].toInt() and 0xFF,
                data1 = bytes[base + 1].toInt() and 0xFF,
                data2 = bytes[base + 2].toInt() and 0xFF,
                timestampNanos = timestamp
            )
        }
    }

    fun setMidiOutEnabled(enabled: Boolean) = NativeEngine.nativeSetMidiOutEnabled(handle, enabled)
    fun setClockSend(enabled: Boolean) = NativeEngine.nativeSetClockSend(handle, enabled)
    fun setClockReceive(enabled: Boolean) = NativeEngine.nativeSetClockReceive(handle, enabled)
    fun setTransportSend(enabled: Boolean) = NativeEngine.nativeSetTransportSend(handle, enabled)
    fun setMidiChannel(channel: Int) = NativeEngine.nativeSetMidiChannel(handle, channel)

    // ---------------------------------------------------------------------
    // Firmware
    // ---------------------------------------------------------------------

    suspend fun importFirmware(uri: Uri): String = withContext(Dispatchers.IO) {
        val bytes = try {
            appContext.contentResolver.openInputStream(uri)?.use { it.readBytes() }
        } catch (t: Throwable) {
            null
        } ?: return@withContext "Could not read the selected file."

        if (bytes.size > 96 * 1024 * 1024) return@withContext "File is too large for a TR-8S firmware image."
        val binary = if (bytes.size > 2 && bytes[0] == 'P'.code.toByte() && bytes[1] == 'K'.code.toByte()) {
            unzipFirmware(bytes) ?: return@withContext "No TR8S_UP.bin or rpg42_init_param.bin found inside the .zip."
        } else {
            bytes
        }

        val error = NativeEngine.nativeLoadFirmware(handle, binary)
        if (error.isNotEmpty()) return@withContext error

        firmwareFile.parentFile?.mkdirs()
        firmwareFile.writeBytes(binary)
        kitNameCache.clear()
        patternNameCache.clear()
        patternCodeCache.clear()
        toneNameCache.clear()
        reloadCatalog()
        reloadPatternRows(_state.value.variation)
        _state.value = _state.value.copy(
            firmwareLoaded = true,
            firmwareName = NativeEngine.nativeFirmwareName(handle),
            engineError = null
        )
        ""
    }

    fun clearFirmware() {
        runCatching { firmwareFile.delete() }
        NativeEngine.nativeClearFirmware(handle)
        kitNameCache.clear()
        patternNameCache.clear()
        patternCodeCache.clear()
        toneNameCache.clear()
        reloadCatalog()
        reloadPatternRows(_state.value.variation)
        _state.value = _state.value.copy(
            firmwareLoaded = false,
            firmwareName = NativeEngine.nativeFirmwareName(handle),
            engineError = null
        )
    }

    private fun unzipFirmware(zipBytes: ByteArray): ByteArray? {
        ZipInputStream(ByteArrayInputStream(zipBytes)).use { zip ->
            var entry = zip.nextEntry
            while (entry != null) {
                val name = entry.name.lowercase()
                if (!entry.isDirectory && (name.endsWith(".bin") || name.contains("tr8s_up"))) {
                    val out = ByteArrayOutputStream()
                    val buffer = ByteArray(64 * 1024)
                    var total = 0
                    while (true) {
                        val read = zip.read(buffer)
                        if (read < 0) break
                        total += read
                        if (total > 16 * 1024 * 1024) return null
                        out.write(buffer, 0, read)
                    }
                    return out.toByteArray()
                }
                entry = zip.nextEntry
            }
        }
        return null
    }

    fun bpmRounded(): Int = _state.value.tempo.roundToInt()
}

data class MidiOutRecord(
    val status: Int,
    val data1: Int,
    val data2: Int,
    val timestampNanos: Long
)

object EngineHolder {
    @Volatile
    private var controller: EngineController? = null

    @Volatile
    private var midi: com.danimanx.replicanttr8s.midi.MidiController? = null

    fun get(context: Context): EngineController {
        val existing = controller
        if (existing != null) return existing
        return synchronized(this) {
            controller ?: EngineController(context.applicationContext).also {
                controller = it
                it.start()
            }
        }
    }

    fun getMidi(context: Context): com.danimanx.replicanttr8s.midi.MidiController {
        val existing = midi
        if (existing != null) return existing
        return synchronized(this) {
            midi ?: com.danimanx.replicanttr8s.midi.MidiController(
                context.applicationContext,
                get(context)
            ).also {
                midi = it
                it.start()
            }
        }
    }

    fun stopEngine() {
        // Keep the controller object alive so a service restart can reuse it,
        // but release USB ports and the audio stream while not in use.
        midi?.disconnectAll()
        controller?.stopAudio()
    }

    fun peek(): EngineController? = controller
}
