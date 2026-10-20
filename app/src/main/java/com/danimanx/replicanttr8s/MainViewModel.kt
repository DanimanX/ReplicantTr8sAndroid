package com.danimanx.replicanttr8s

import android.app.Application
import android.net.Uri
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.danimanx.replicanttr8s.audio.EngineHolder
import com.danimanx.replicanttr8s.midi.MidiController
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.SharedFlow
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

    fun importFirmware(uri: Uri) {
        viewModelScope.launch {
            val error = engine.importFirmware(uri)
            _messages.emit(if (error.isEmpty()) "TR-8S v3.00 firmware loaded" else error)
        }
    }

    fun clearFirmware() {
        engine.clearFirmware()
        viewModelScope.launch { _messages.emit("Firmware cleared — built-in preview restored") }
    }

    fun engineToggle() = engine.togglePlaying()
}
