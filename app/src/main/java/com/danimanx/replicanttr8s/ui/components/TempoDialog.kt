package com.danimanx.replicanttr8s.ui.components

import android.os.SystemClock
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
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
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import com.danimanx.replicanttr8s.ui.theme.Amber
import com.danimanx.replicanttr8s.ui.theme.Cyan
import com.danimanx.replicanttr8s.ui.theme.Ink
import com.danimanx.replicanttr8s.ui.theme.Line
import com.danimanx.replicanttr8s.ui.theme.PanelHigh
import com.danimanx.replicanttr8s.ui.theme.TextMuted
import com.danimanx.replicanttr8s.ui.theme.TextPrimary
import kotlin.math.roundToInt

@Composable
fun TempoDialog(
    tempo: Float,
    onChange: (Float) -> Unit,
    onDismiss: () -> Unit
) {
    var value by remember { mutableStateOf(tempo.coerceIn(40f, 300f)) }
    var taps by remember { mutableStateOf(listOf<Long>()) }

    fun apply(newValue: Float) {
        value = newValue.coerceIn(40f, 300f)
        onChange(value)
    }

    fun tapTempo() {
        val now = SystemClock.elapsedRealtime()
        val next = (taps + now).takeLast(5)
        taps = next
        if (next.size >= 2) {
            val intervals = next.zipWithNext { a, b -> (b - a).toDouble() }.filter { it in 160.0..1600.0 }
            if (intervals.isNotEmpty()) {
                val bpm = (60_000.0 / intervals.average()).toFloat()
                apply(bpm)
            }
        }
    }

    Dialog(
        onDismissRequest = onDismiss,
        properties = DialogProperties(usePlatformDefaultWidth = false)
    ) {
        Column(
            modifier = Modifier
                .widthIn(min = 300.dp, max = 440.dp)
                .fillMaxWidth(0.92f)
                .clip(RoundedCornerShape(16.dp))
                .background(PanelHigh)
                .padding(18.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            Text(
                "TEMPO // BPM",
                style = MaterialTheme.typography.titleSmall,
                color = Cyan
            )
            Text(
                value.roundToInt().toString(),
                style = MaterialTheme.typography.displayLarge.copy(fontSize = 58.sp),
                color = Amber,
                modifier = Modifier.fillMaxWidth(),
                textAlign = TextAlign.Center
            )
            Slider(
                value = value,
                onValueChange = { apply(it) },
                valueRange = 40f..300f,
                colors = SliderDefaults.colors(
                    thumbColor = Amber,
                    activeTrackColor = Amber,
                    inactiveTrackColor = Line
                )
            )
            Row(horizontalArrangement = Arrangement.spacedBy(7.dp)) {
                TempoStep("-5", Modifier.weight(1f)) { apply(value - 5f) }
                TempoStep("-1", Modifier.weight(1f)) { apply(value - 1f) }
                TempoStep("+1", Modifier.weight(1f)) { apply(value + 1f) }
                TempoStep("+5", Modifier.weight(1f)) { apply(value + 5f) }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(7.dp)) {
                listOf(90, 120, 128, 140, 160, 174).forEach { preset ->
                    PresetChip(preset.toString(), Modifier.weight(1f)) { apply(preset.toFloat()) }
                }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                Box(
                    modifier = Modifier
                        .weight(1f)
                        .height(44.dp)
                        .clip(RoundedCornerShape(8.dp))
                        .background(Amber.copy(alpha = 0.14f))
                        .padding(1.dp)
                        .clip(RoundedCornerShape(8.dp))
                        .background(Color.Transparent)
                        .clickableNoRipple { tapTempo() },
                    contentAlignment = Alignment.Center
                ) {
                    Text(
                        "TAP TEMPO",
                        style = MaterialTheme.typography.labelMedium,
                        color = Amber
                    )
                }
                Box(
                    modifier = Modifier
                        .weight(1f)
                        .height(44.dp)
                        .clip(RoundedCornerShape(8.dp))
                        .background(Cyan)
                        .clickableNoRipple { onDismiss() },
                    contentAlignment = Alignment.Center
                ) {
                    Text(
                        "SET",
                        style = MaterialTheme.typography.labelMedium,
                        color = Ink
                    )
                }
            }
            Text(
                "Tap four times on the beat for tap tempo. Swing and accent stay independent.",
                style = MaterialTheme.typography.labelSmall,
                color = TextMuted,
                modifier = Modifier.fillMaxWidth(),
                textAlign = TextAlign.Center
            )
        }
    }
}

@Composable
private fun TempoStep(label: String, modifier: Modifier = Modifier, onClick: () -> Unit) {
    Box(
        modifier = modifier
            .height(38.dp)
            .clip(RoundedCornerShape(7.dp))
            .background(PanelHigh)
            .clickableNoRipple(onClick),
        contentAlignment = Alignment.Center
    ) {
        Text(label, style = MaterialTheme.typography.labelMedium, color = TextPrimary)
    }
}

@Composable
private fun PresetChip(label: String, modifier: Modifier = Modifier, onClick: () -> Unit) {
    Box(
        modifier = modifier
            .height(32.dp)
            .clip(RoundedCornerShape(6.dp))
            .background(PanelHigh)
            .clickableNoRipple(onClick),
        contentAlignment = Alignment.Center
    ) {
        Text(label, style = MaterialTheme.typography.labelSmall, color = Amber.copy(alpha = 0.9f))
    }
}

private fun Modifier.clickableNoRipple(onClick: () -> Unit): Modifier =
    this.then(
        Modifier.clickable(
            interactionSource = androidx.compose.foundation.interaction.MutableInteractionSource(),
            indication = null,
            onClick = onClick
        )
    )
