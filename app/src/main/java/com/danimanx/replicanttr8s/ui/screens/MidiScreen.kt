package com.danimanx.replicanttr8s.ui.screens

import android.os.Build
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.KeyboardArrowDown
import androidx.compose.material.icons.filled.Link
import androidx.compose.material.icons.filled.LinkOff
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Usb
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.danimanx.replicanttr8s.audio.EngineController
import com.danimanx.replicanttr8s.midi.MidiController
import com.danimanx.replicanttr8s.model.ChannelNotes
import com.danimanx.replicanttr8s.model.ChannelShortNames
import com.danimanx.replicanttr8s.model.EngineUiState
import com.danimanx.replicanttr8s.model.MidiDeviceEntry
import com.danimanx.replicanttr8s.model.MidiDirection
import com.danimanx.replicanttr8s.model.MidiLogEntry
import com.danimanx.replicanttr8s.model.MidiUiState
import com.danimanx.replicanttr8s.ui.components.Rt8Button
import com.danimanx.replicanttr8s.ui.components.Rt8Panel
import com.danimanx.replicanttr8s.ui.components.SectionLabel
import com.danimanx.replicanttr8s.ui.components.StatusChip
import com.danimanx.replicanttr8s.ui.components.TinyTag
import com.danimanx.replicanttr8s.ui.components.ToggleChip
import com.danimanx.replicanttr8s.ui.theme.Amber
import com.danimanx.replicanttr8s.ui.theme.Coral
import com.danimanx.replicanttr8s.ui.theme.Cyan
import com.danimanx.replicanttr8s.ui.theme.Green
import com.danimanx.replicanttr8s.ui.theme.Ink
import com.danimanx.replicanttr8s.ui.theme.Line
import com.danimanx.replicanttr8s.ui.theme.PanelHigh
import com.danimanx.replicanttr8s.ui.theme.TextMuted
import com.danimanx.replicanttr8s.ui.theme.TextPrimary
import com.danimanx.replicanttr8s.ui.theme.TextSecondary
import com.danimanx.replicanttr8s.ui.theme.Violet
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

@Composable
fun MidiScreen(
    state: EngineUiState,
    midi: MidiUiState,
    engine: EngineController,
    controller: MidiController
) {
    var showNoteMap by rememberSaveable { mutableStateOf(false) }
    var showChannelMenu by remember { mutableStateOf(false) }

    LaunchedEffect(Unit) { controller.refreshDevices() }

    LazyColumn(
        modifier = Modifier.fillMaxSize(),
        contentPadding = PaddingValues(horizontal = 12.dp, vertical = 10.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        item {
            MidiHero(state, midi)
        }
        if (!midi.error.isNullOrBlank()) {
            item {
                Rt8Panel(accent = Coral) {
                    Text(
                        midi.error,
                        style = MaterialTheme.typography.bodySmall,
                        color = Coral
                    )
                }
            }
        }
        item {
            Rt8Panel(accent = Cyan) {
                SectionLabel("MIDI IN // FROM CONTROLLER", accent = Cyan) {
                    Icon(
                        Icons.Filled.Refresh,
                        contentDescription = "Refresh MIDI devices",
                        tint = Cyan,
                        modifier = Modifier
                            .size(20.dp)
                            .clickable { controller.refreshDevices() }
                    )
                }
                Spacer(Modifier.height(7.dp))
                if (midi.inputConnected) {
                    ConnectedRow(
                        name = midi.inputDeviceName.ifBlank { "MIDI input" },
                        direction = "INPUT",
                        accent = Green,
                        onDisconnect = { controller.disconnectInput() }
                    )
                }
                val inputs = midi.devices.filter { it.inputPorts > 0 }
                if (inputs.isEmpty()) {
                    EmptyMidiHint("Connect a class-compliant USB MIDI controller or interface.")
                } else {
                    inputs.forEach { device ->
                        DeviceRow(
                            device = device,
                            accent = Cyan,
                            connected = device.isInputConnected,
                            portLabel = "${device.inputPorts} OUT PORT${if (device.inputPorts == 1) "" else "S"}",
                            onConnect = { controller.connectInput(device.id) },
                            onDisconnect = { controller.disconnectInput() }
                        )
                    }
                }
            }
        }
        item {
            Rt8Panel(accent = Violet) {
                SectionLabel("MIDI OUT // TO DEVICE", accent = Violet) {
                    TinyTag(if (midi.outputConnected) "ROUTED" else "IDLE", if (midi.outputConnected) Green else TextMuted)
                }
                Spacer(Modifier.height(7.dp))
                if (midi.outputConnected) {
                    ConnectedRow(
                        name = midi.outputDeviceName.ifBlank { "MIDI output" },
                        direction = "OUTPUT",
                        accent = Green,
                        onDisconnect = { controller.disconnectOutput() }
                    )
                }
                val outputs = midi.devices.filter { it.outputPorts > 0 }
                if (outputs.isEmpty()) {
                    EmptyMidiHint("Connect a USB MIDI synth, drum machine or interface to send notes and clock.")
                } else {
                    outputs.forEach { device ->
                        DeviceRow(
                            device = device,
                            accent = Violet,
                            connected = device.isOutputConnected,
                            portLabel = "${device.outputPorts} IN PORT${if (device.outputPorts == 1) "" else "S"}",
                            onConnect = { controller.connectOutput(device.id) },
                            onDisconnect = { controller.disconnectOutput() }
                        )
                    }
                }
            }
        }
        item {
            Rt8Panel(accent = Amber) {
                SectionLabel("SYNC & ROUTING", accent = Amber)
                Spacer(Modifier.height(6.dp))
                SettingSwitch(
                    title = "MIDI IN",
                    detail = "Receive notes, CCs, clock and transport",
                    checked = midi.midiInEnabled
                ) { controller.setMidiInEnabled(it) }
                SettingSwitch(
                    title = "MIDI OUT",
                    detail = "Send sequencer notes and performance data",
                    checked = midi.midiOutEnabled
                ) { controller.setMidiOutEnabled(it) }
                SettingSwitch(
                    title = "CLOCK OUT // 24 PPQN",
                    detail = "Send MIDI clock and start/stop to downstream gear",
                    checked = midi.clockSend
                ) { controller.setClockSend(it) }
                SettingSwitch(
                    title = "CLOCK IN // EXTERNAL SYNC",
                    detail = "Follow incoming MIDI clock; Play waits for the first pulse",
                    checked = midi.clockReceive
                ) { controller.setClockReceive(it) }
                SettingSwitch(
                    title = "TRANSPORT OUT",
                    detail = "Send start/stop when the performance transport changes",
                    checked = midi.transportSend
                ) { controller.setTransportSend(it) }

                Spacer(Modifier.height(8.dp))
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Column(Modifier.weight(1f)) {
                        Text("MIDI CHANNEL FILTER", style = MaterialTheme.typography.titleSmall, color = TextPrimary)
                        Text(
                            if (midi.channelFilter < 0) "OMNI // all channels" else "CH ${midi.channelFilter + 1} only",
                            style = MaterialTheme.typography.labelSmall,
                            color = TextMuted
                        )
                    }
                    Box {
                        Row(
                            modifier = Modifier
                                .clip(RoundedCornerShape(7.dp))
                                .background(PanelHigh)
                                .border(1.dp, Line, RoundedCornerShape(7.dp))
                                .clickable { showChannelMenu = true }
                                .padding(horizontal = 12.dp, vertical = 7.dp),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Text(
                                if (midi.channelFilter < 0) "OMNI" else "${midi.channelFilter + 1}",
                                style = MaterialTheme.typography.labelMedium,
                                color = Cyan
                            )
                            Spacer(Modifier.width(5.dp))
                            Icon(Icons.Filled.KeyboardArrowDown, contentDescription = null, tint = Cyan, modifier = Modifier.size(16.dp))
                        }
                        androidx.compose.material3.DropdownMenu(
                            expanded = showChannelMenu,
                            onDismissRequest = { showChannelMenu = false },
                            containerColor = PanelHigh
                        ) {
                            androidx.compose.material3.DropdownMenuItem(
                                text = { Text("OMNI", color = TextPrimary) },
                                onClick = {
                                    controller.setChannelFilter(-1)
                                    showChannelMenu = false
                                }
                            )
                            (1..16).forEach { ch ->
                                androidx.compose.material3.DropdownMenuItem(
                                    text = { Text("CH $ch", color = TextPrimary) },
                                    onClick = {
                                        controller.setChannelFilter(ch - 1)
                                        showChannelMenu = false
                                    }
                                )
                            }
                        }
                    }
                }

                Spacer(Modifier.height(8.dp))
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Column(Modifier.weight(1f)) {
                        Text("MIDI 2.0 // UMP", style = MaterialTheme.typography.titleSmall, color = TextPrimary)
                        Text(
                            when {
                                !midi.umpSupported -> "Requires Android 13 (API 33) or newer"
                                midi.useUmp -> "UMP translation is active for UMP-capable devices"
                                else -> "Opt in to Universal MIDI Packet translation"
                            },
                            style = MaterialTheme.typography.labelSmall,
                            color = TextMuted
                        )
                    }
                    ToggleChip(
                        text = if (midi.useUmp) "UMP ON" else "UMP OFF",
                        selected = midi.useUmp,
                        accent = Violet,
                        enabled = midi.umpSupported
                    ) { controller.setUseUmp(!midi.useUmp) }
                }

                Spacer(Modifier.height(8.dp))
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    StatusChip(
                        "CLOCK",
                        when {
                            state.clockReceive && state.externalClock -> "LOCKED"
                            state.clockReceive -> "WAITING"
                            else -> "INTERNAL"
                        },
                        if (state.externalClock) Green else Amber,
                        active = state.playing
                    )
                    StatusChip("MIDI", if (midi.inputConnected) "IN" else "—", Cyan, active = midi.inputConnected)
                    StatusChip("MIDI", if (midi.outputConnected) "OUT" else "—", Violet, active = midi.outputConnected)
                    if (Build.VERSION.SDK_INT >= 33) {
                        StatusChip("UMP", if (midi.useUmp) "ON" else "OFF", Violet, active = midi.useUmp)
                    }
                }
            }
        }
        item {
            Rt8Panel(accent = TextSecondary) {
                SectionLabel("MIDI MONITOR // LAST MESSAGES", accent = TextSecondary) {
                    Text(
                        "CLEAR",
                        style = MaterialTheme.typography.labelSmall,
                        color = Cyan,
                        modifier = Modifier.clickable { controller.clearLog() }.padding(2.dp)
                    )
                }
                Spacer(Modifier.height(6.dp))
                val entries = midi.log.takeLast(10).reversed()
                if (entries.isEmpty()) {
                    Text(
                        "No traffic yet. Notes, CCs and clock land here in real time.",
                        style = MaterialTheme.typography.bodySmall,
                        color = TextMuted
                    )
                } else {
                    entries.forEach { entry -> MonitorRow(entry) }
                }
            }
        }
        item {
            Rt8Panel(accent = Amber) {
                Row(
                    modifier = Modifier.fillMaxWidth().clickable { showNoteMap = !showNoteMap },
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Column(Modifier.weight(1f)) {
                        Text("NOTE MAP // GM DRUM STANDARD", style = MaterialTheme.typography.titleSmall, color = Amber)
                        Text(
                            "Notes 36–51 plus chromatic C3–A3",
                            style = MaterialTheme.typography.labelSmall,
                            color = TextMuted
                        )
                    }
                    Icon(
                        Icons.Filled.KeyboardArrowDown,
                        contentDescription = if (showNoteMap) "Collapse note map" else "Expand note map",
                        tint = Amber,
                        modifier = Modifier.size(20.dp)
                    )
                }
                if (showNoteMap) {
                    Spacer(Modifier.height(8.dp))
                    11.let {
                        ChannelShortNames.forEachIndexed { index, name ->
                            Row(
                                modifier = Modifier.fillMaxWidth().padding(vertical = 2.dp),
                                verticalAlignment = Alignment.CenterVertically
                            ) {
                                Text(name, style = MaterialTheme.typography.labelMedium, color = Cyan, modifier = Modifier.width(30.dp))
                                Text("NOTE ${ChannelNotes[index]}", style = MaterialTheme.typography.labelSmall, color = TextPrimary, modifier = Modifier.width(74.dp))
                                Text(noteName(ChannelNotes[index]), style = MaterialTheme.typography.labelSmall, color = TextMuted)
                            }
                        }
                    }
                }
            }
        }
        item { Spacer(Modifier.height(18.dp)) }
    }
}

@Composable
private fun MidiHero(state: EngineUiState, midi: MidiUiState) {
    Rt8Panel(accent = Violet) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Icon(Icons.Filled.Usb, contentDescription = null, tint = Violet, modifier = Modifier.size(26.dp))
            Spacer(Modifier.width(10.dp))
            Column(Modifier.weight(1f)) {
                Text("USB MIDI // ROUTING & SYNC", style = MaterialTheme.typography.headlineMedium, color = TextPrimary)
                Text(
                    "Class-compliant USB MIDI hosts, MIDI 2.0 UMP where supported, clock and transport sync.",
                    style = MaterialTheme.typography.bodySmall,
                    color = TextMuted
                )
            }
        }
        Spacer(Modifier.height(9.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(5.dp)) {
            StatusChip("USB HOST", if (midi.available) "READY" else "N/A", Green, active = midi.available)
            StatusChip("UMP", if (midi.umpSupported) "API 33+" else "N/A", Violet, active = midi.umpSupported)
            StatusChip(
                state.audio.apiLabel,
                if (state.audio.sampleRate > 0) "${state.audio.sampleRate / 1000}K" else "—",
                Cyan,
                active = state.audioRunning
            )
        }
    }
}

@Composable
private fun ConnectedRow(
    name: String,
    direction: String,
    accent: Color,
    onDisconnect: () -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(8.dp))
            .background(accent.copy(alpha = 0.10f))
            .border(1.dp, accent.copy(alpha = 0.45f), RoundedCornerShape(8.dp))
            .padding(10.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Box(Modifier.size(7.dp).clip(RoundedCornerShape(50)).background(accent))
        Spacer(Modifier.width(8.dp))
        Column(Modifier.weight(1f)) {
            Text(name, style = MaterialTheme.typography.titleMedium, color = TextPrimary, maxLines = 1, overflow = TextOverflow.Ellipsis)
            Text("$direction · CONNECTED", style = MaterialTheme.typography.labelSmall, color = accent)
        }
        Rt8Button(
            text = "DISCONNECT",
            icon = Icons.Filled.LinkOff,
            accent = Coral,
            modifier = Modifier.width(132.dp),
            onClick = onDisconnect
        )
    }
}

@Composable
private fun DeviceRow(
    device: MidiDeviceEntry,
    accent: Color,
    connected: Boolean,
    portLabel: String,
    onConnect: () -> Unit,
    onDisconnect: () -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(top = 6.dp)
            .clip(RoundedCornerShape(8.dp))
            .background(PanelHigh.copy(alpha = 0.75f))
            .border(1.dp, Line, RoundedCornerShape(8.dp))
            .padding(10.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Column(Modifier.weight(1f)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(
                    device.name,
                    style = MaterialTheme.typography.titleMedium,
                    color = TextPrimary,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
                Spacer(Modifier.width(6.dp))
                if (device.isUmp) TinyTag("UMP", Violet)
            }
            Text(
                listOf(device.typeLabel, portLabel, device.manufacturer)
                    .filter { it.isNotBlank() }
                    .joinToString(" · "),
                style = MaterialTheme.typography.labelSmall,
                color = TextMuted,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
        }
        Spacer(Modifier.width(8.dp))
        Rt8Button(
            text = if (connected) "CONNECTED" else "CONNECT",
            icon = if (connected) Icons.Filled.LinkOff else Icons.Filled.Link,
            accent = if (connected) Green else accent,
            selected = connected,
            modifier = Modifier.width(124.dp),
            onClick = if (connected) onDisconnect else onConnect
        )
    }
}

@Composable
private fun EmptyMidiHint(text: String) {
    Text(
        text,
        style = MaterialTheme.typography.bodySmall,
        color = TextMuted,
        modifier = Modifier.padding(vertical = 8.dp)
    )
}

@Composable
private fun SettingSwitch(
    title: String,
    detail: String,
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit
) {
    Row(
        modifier = Modifier.fillMaxWidth().padding(vertical = 3.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Column(Modifier.weight(1f).padding(end = 8.dp)) {
            Text(title, style = MaterialTheme.typography.titleSmall, color = TextPrimary)
            Text(detail, style = MaterialTheme.typography.labelSmall, color = TextMuted)
        }
        Switch(
            checked = checked,
            onCheckedChange = onCheckedChange,
            colors = SwitchDefaults.colors(
                checkedThumbColor = Ink,
                checkedTrackColor = Cyan,
                uncheckedThumbColor = TextMuted,
                uncheckedTrackColor = PanelHigh,
                uncheckedBorderColor = Line
            )
        )
    }
}

@Composable
private fun MonitorRow(entry: MidiLogEntry) {
    val color = when (entry.direction) {
        MidiDirection.IN -> Cyan
        MidiDirection.OUT -> Violet
        MidiDirection.SYSTEM -> Amber
    }
    Row(
        modifier = Modifier.fillMaxWidth().padding(vertical = 2.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(
            entry.direction.name,
            style = MaterialTheme.typography.labelSmall.copy(fontSize = 7.sp),
            color = color,
            modifier = Modifier.width(26.dp)
        )
        Text(
            entry.bytes,
            style = MaterialTheme.typography.labelSmall.copy(fontSize = 8.sp),
            color = TextPrimary,
            modifier = Modifier.width(76.dp)
        )
        Text(
            entry.description,
            style = MaterialTheme.typography.labelSmall,
            color = TextSecondary,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
            modifier = Modifier.weight(1f)
        )
        Text(
            timeFormat.format(Date(entry.timestampMs)),
            style = MaterialTheme.typography.labelSmall.copy(fontSize = 7.sp),
            color = TextMuted
        )
    }
}

private val timeFormat = SimpleDateFormat("HH:mm:ss.SSS", Locale.US)

private fun noteName(note: Int): String {
    val names = arrayOf("C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B")
    return "${names[note.mod(12)]}${note / 12 - 1}"
}
