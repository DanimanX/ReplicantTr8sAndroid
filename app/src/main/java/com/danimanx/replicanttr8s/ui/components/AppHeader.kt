package com.danimanx.replicanttr8s.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.GraphicEq
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.danimanx.replicanttr8s.model.EngineUiState
import com.danimanx.replicanttr8s.model.MidiUiState
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
import java.util.Locale
import kotlin.math.roundToInt

@Composable
fun AppHeader(
    state: EngineUiState,
    midi: MidiUiState,
    scope: List<Float>,
    compact: Boolean,
    onPlayToggle: () -> Unit,
    onTempoClick: () -> Unit
) {
    if (compact) {
        Column(
            modifier = Modifier.fillMaxWidth().padding(horizontal = 12.dp, vertical = 8.dp),
            verticalArrangement = Arrangement.spacedBy(7.dp)
        ) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Brand(compact = true, modifier = Modifier.weight(1f))
                BigPlayButton(
                    playing = state.playing,
                    onClick = onPlayToggle,
                    accent = Cyan,
                    size = 46
                )
            }
            LcdStrip(state, scope, compact = true, onTempoClick = onTempoClick)
            StatusRow(state, midi)
        }
    } else {
        Row(
            modifier = Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 10.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(14.dp)
        ) {
            Brand(compact = false, modifier = Modifier.width(210.dp))
            LcdStrip(
                state = state,
                scope = scope,
                compact = false,
                onTempoClick = onTempoClick,
                modifier = Modifier.weight(1f)
            )
            Column(
                modifier = Modifier.width(300.dp),
                verticalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    StatusChip(
                        "AAUDIO",
                        if (state.audio.sampleRate > 0) "${state.audio.sampleRate / 1000}K" else "—",
                        Cyan,
                        active = state.audioRunning
                    )
                    StatusChip(
                        "LAT",
                        if (state.audio.latencyMs > 0.1) String.format(Locale.US, "%.1fMS", state.audio.latencyMs) else "—",
                        if (state.audio.latencyMs in 0.1..15.0) Green else Amber,
                        active = state.audioRunning
                    )
                    StatusChip(
                        "MODE",
                        if (state.audio.sharing == 1) "EXCL" else "SHR",
                        if (state.audio.sharing == 1) Green else Amber,
                        active = state.audioRunning
                    )
                }
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    StatusChip(
                        "USB",
                        "${if (midi.inputConnected) "IN" else "—"} / ${if (midi.outputConnected) "OUT" else "—"}",
                        Violet,
                        active = midi.inputConnected || midi.outputConnected
                    )
                    StatusChip(
                        "FW",
                        if (state.firmwareLoaded) "V3.00" else "PREVIEW",
                        Amber,
                        active = state.firmwareLoaded
                    )
                    StatusChip(
                        if (state.clockReceive) "EXT CLK" else "INT CLK",
                        if (state.clockReceive && !state.externalClock) "WAIT" else null,
                        if (state.externalClock) Green else TextSecondary,
                        active = state.clockReceive || state.playing
                    )
                }
            }
            BigPlayButton(
                playing = state.playing,
                onClick = onPlayToggle,
                accent = Cyan,
                size = 52
            )
        }
    }
}

@Composable
private fun Brand(compact: Boolean, modifier: Modifier = Modifier) {
    Row(modifier = modifier, verticalAlignment = Alignment.CenterVertically) {
        Box(
            modifier = Modifier
                .size(if (compact) 38.dp else 46.dp)
                .clip(RoundedCornerShape(9.dp))
                .background(Cyan.copy(alpha = 0.10f))
                .border(1.dp, Cyan.copy(alpha = 0.45f), RoundedCornerShape(9.dp)),
            contentAlignment = Alignment.Center
        ) {
            Icon(
                Icons.Filled.GraphicEq,
                contentDescription = null,
                tint = Cyan,
                modifier = Modifier.size(if (compact) 22.dp else 26.dp)
            )
        }
        Spacer(Modifier.width(9.dp))
        Column {
            Text(
                "REPLICANT",
                style = MaterialTheme.typography.headlineLarge.copy(fontSize = if (compact) 22.sp else 28.sp),
                color = TextPrimary,
                maxLines = 1
            )
            Text(
                "TR-8S // ANDROID",
                style = MaterialTheme.typography.labelSmall.copy(fontSize = 8.sp),
                color = Cyan,
                maxLines = 1
            )
            if (!compact) {
                Text(
                    "ACB · FM · PCM // LOW-LATENCY RHYTHM PERFORMER",
                    style = MaterialTheme.typography.labelSmall.copy(fontSize = 7.sp),
                    color = TextMuted,
                    maxLines = 1
                )
            }
        }
    }
}

@Composable
private fun LcdStrip(
    state: EngineUiState,
    scope: List<Float>,
    compact: Boolean,
    onTempoClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    Box(
        modifier = modifier
            .clip(RoundedCornerShape(10.dp))
            .background(
                Brush.verticalGradient(
                    listOf(Color(0xFF071110), Color(0xFF040A0A))
                )
            )
            .border(1.dp, Cyan.copy(alpha = 0.28f), RoundedCornerShape(10.dp))
            .padding(horizontal = 10.dp, vertical = if (compact) 6.dp else 8.dp)
    ) {
        Column(Modifier.fillMaxWidth()) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(
                    "PTN ${state.patternCode}",
                    style = MaterialTheme.typography.labelMedium.copy(fontSize = 9.sp),
                    color = Cyan
                )
                Spacer(Modifier.width(8.dp))
                Text(
                    state.patternName.uppercase(),
                    style = MaterialTheme.typography.titleMedium.copy(fontSize = if (compact) 13.sp else 16.sp),
                    color = TextPrimary,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                    modifier = Modifier.weight(1f)
                )
                Text(
                    "VAR ${state.variationLetter} · ${(state.currentStep + 1).toString().padStart(2, '0')}/16",
                    style = MaterialTheme.typography.labelSmall.copy(fontSize = 8.sp),
                    color = if (state.playing) Cyan else TextMuted
                )
            }
            Box(
                Modifier
                    .fillMaxWidth()
                    .height(if (compact) 26.dp else 38.dp)
            ) {
                ScopeCanvas(samples = scope, accent = Cyan, modifier = Modifier.fillMaxWidth())
            }
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(
                    "KIT ${(state.kitIndex + 1).toString().padStart(3, '0')} ${state.kitName.uppercase()}",
                    style = MaterialTheme.typography.labelSmall.copy(fontSize = 8.sp),
                    color = TextSecondary,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                    modifier = Modifier.weight(1f)
                )
                Text(
                    "${state.tempo.roundToInt()}",
                    style = MaterialTheme.typography.headlineMedium.copy(fontSize = if (compact) 18.sp else 22.sp),
                    color = Amber,
                    modifier = Modifier
                        .clip(RoundedCornerShape(4.dp))
                        .background(Amber.copy(alpha = 0.06f))
                        .padding(horizontal = 6.dp, vertical = 1.dp)
                )
                Text(
                    " BPM",
                    style = MaterialTheme.typography.labelSmall.copy(fontSize = 8.sp),
                    color = Amber.copy(alpha = 0.8f)
                )
                Spacer(Modifier.width(8.dp))
                LedDot(state.playing, if (state.playing) Green else TextMuted, 5.dp)
                Spacer(Modifier.width(4.dp))
                Text(
                    if (state.playing) "RUN" else "STOP",
                    style = MaterialTheme.typography.labelSmall.copy(fontSize = 7.sp),
                    color = if (state.playing) Green else TextMuted
                )
            }
        }
    }
}

@Composable
private fun StatusRow(state: EngineUiState, midi: MidiUiState) {
    Row(
        modifier = Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
        horizontalArrangement = Arrangement.spacedBy(5.dp)
    ) {
        StatusChip(
            "AAUDIO",
            buildString {
                if (state.audio.sampleRate > 0) append("${state.audio.sampleRate / 1000}K")
                else append("—")
                if (state.audio.latencyMs > 0.1) append(" · ${String.format(Locale.US, "%.0f", state.audio.latencyMs)}MS")
            },
            if (state.audio.latencyMs in 0.1..15.0) Green else Amber,
            active = state.audioRunning
        )
        StatusChip(
            "USB",
            "${if (midi.inputConnected) "IN" else "—"}/${if (midi.outputConnected) "OUT" else "—"}",
            Violet,
            active = midi.inputConnected || midi.outputConnected
        )
        StatusChip("FW", if (state.firmwareLoaded) "V3.00" else "PREVIEW", Amber, active = state.firmwareLoaded)
        if (state.clockReceive) {
            StatusChip(
                "EXT",
                if (state.externalClock) "LOCK" else "WAIT",
                if (state.externalClock) Green else Amber,
                active = true
            )
        }
    }
}
