package com.danimanx.replicanttr8s

import android.app.Application
import android.net.Uri
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.danimanx.replicanttr8s.audio.EngineHolder
import com.danimanx.replicanttr8s.midi.MidiController
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch

class MainViewModel(application: Application) : AndroidViewModel(application) {

    val engine = EngineHolder.get(application)
    val midi: MidiController = EngineHolder.getMidi(application)

    val engineState = engine.state
    val meters = engine.meters
    val scope = engine.scope
    val catalog = engine.catalog
    val midiState = midi.state

    private val _messages = MutableSharedFlow<String>(extraBufferCapacity = 4)
    val messages: SharedFlow<String> = _messages

    private val _importing = MutableStateFlow(false)
    val importing: StateFlow<Boolean> = _importing

    private val _firmwareMessage = MutableStateFlow<String?>(null)
    val firmwareMessage: StateFlow<String?> = _firmwareMessage

    fun importFirmware(uri: Uri) {
        viewModelScope.launch {
            _importing.value = true
            _firmwareMessage.value = null
            val error = engine.importFirmware(uri)
            _firmwareMessage.value = error.ifEmpty { null }
            _messages.emit(if (error.isEmpty()) "TR-8S firmware loaded — engine unlocked" else error)
            _importing.value = false
        }
    }

    fun clearFirmware() {
        engine.clearFirmware()
        _firmwareMessage.value = null
        viewModelScope.launch { _messages.emit("Firmware removed — engine locked until you import it again") }
    }

    fun engineToggle() = engine.togglePlaying()
}
