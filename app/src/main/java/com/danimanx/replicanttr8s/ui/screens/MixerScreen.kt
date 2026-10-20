package com.danimanx.replicanttr8s.ui.screens

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.danimanx.replicanttr8s.audio.EngineController
import com.danimanx.replicanttr8s.model.ChannelParam
import com.danimanx.replicanttr8s.model.EngineUiState
import com.danimanx.replicanttr8s.model.MasterParam
import com.danimanx.replicanttr8s.ui.components.LevelMeter
import com.danimanx.replicanttr8s.ui.components.MiniKnob
import com.danimanx.replicanttr8s.ui.components.RotaryKnob
import com.danimanx.replicanttr8s.ui.components.Rt8Panel
import com.danimanx.replicanttr8s.ui.components.SectionLabel
import com.danimanx.replicanttr8s.ui.components.VerticalFader
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
import com.danimanx.replicanttr8s.ui.theme.channelColor
import java.util.Locale
import kotlin.math.roundToInt

@Composable
fun MixerScreen(
    state: EngineUiState,
    meters: List<Float>,
    engine: EngineController
) {
    BoxWithConstraints(
        modifier = Modifier
            .fillMaxSize()
            .padding(horizontal = 12.dp, vertical = 10.dp)
    ) {
        val fitAll = maxWidth >= 1180.dp
        Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(9.dp)) {
            SectionLabel(
                "MIXER // ELEVEN CHANNELS + MASTER BUS",
                accent = TextSecondary
            )
            if (fitAll) {
                Row(
                    modifier = Modifier.fillMaxSize(),
                    horizontalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    repeat(11) { ch ->
                        ChannelStrip(
                            state = state,
                            meters = meters,
                            channel = ch,
                            engine = engine,
                            modifier = Modifier.weight(1f).fillMaxHeight()
                        )
                    }
                    MasterStrip(
                        state = state,
                        meters = meters,
                        engine = engine,
                        modifier = Modifier.width(190.dp).fillMaxHeight()
                    )
                }
            } else {
                Row(
                    modifier = Modifier.fillMaxSize().horizontalScroll(rememberScrollState()),
                    horizontalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    repeat(11) { ch ->
                        ChannelStrip(
                            state = state,
                            meters = meters,
                            channel = ch,
                            engine = engine,
                            modifier = Modifier.width(86.dp).fillMaxHeight()
                        )
                    }
                    MasterStrip(
                        state = state,
                        meters = meters,
                        engine = engine,
                        modifier = Modifier.width(190.dp).fillMaxHeight()
                    )
                }
            }
        }
    }
}

@Composable
private fun ChannelStrip(
    state: EngineUiState,
    meters: List<Float>,
    channel: Int,
    engine: EngineController,
    modifier: Modifier = Modifier
) {
    val data = state.channels.getOrNull(channel)
    val color = channelColor(channel)
    val selected = state.selectedChannel == channel
    Column(
        modifier = modifier
            .clip(RoundedCornerShape(9.dp))
            .background(
                Brush.verticalGradient(
                    listOf(color.copy(alpha = if (selected) 0.15f else 0.06f), PanelHigh)
                )
            )
            .border(
                if (selected) 1.5.dp else 1.dp,
                if (selected) color else Line,
                RoundedCornerShape(9.dp)
            )
            .clickable { engine.selectChannel(channel) }
            .padding(5.dp),
        verticalArrangement = Arrangement.spacedBy(4.dp)
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(
                data?.name ?: "—",
                style = MaterialTheme.typography.titleMedium.copy(fontSize = 14.sp),
                color = color
            )
            Spacer(Modifier.weight(1f))
            Text(
                "${((data?.level ?: 1f) * 150).roundToInt()}",
                style = MaterialTheme.typography.labelSmall.copy(fontSize = 8.sp),
                color = TextSecondary
            )
        }
        Text(
            data?.toneName ?: "TONE",
            style = MaterialTheme.typography.labelSmall.copy(fontSize = 7.sp),
            color = TextMuted,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis
        )
        Row(horizontalArrangement = Arrangement.spacedBy(3.dp)) {
            MiniToggle(
                "M",
                data?.muted == true,
                Amber,
                Modifier.weight(1f)
            ) { engine.setMute(channel, data?.muted != true) }
            MiniToggle(
                "S",
                data?.soloed == true,
                Cyan,
                Modifier.weight(1f)
            ) { engine.setSolo(channel, data?.soloed != true) }
        }
        Row(
            modifier = Modifier.weight(1f).fillMaxWidth(),
            horizontalArrangement = Arrangement.Center
        ) {
            VerticalFader(
                value = data?.level ?: 1f,
                onValueChange = { engine.setChannelParam(channel, ChannelParam.LEVEL, it) },
                accent = color,
                modifier = Modifier.width(34.dp).fillMaxHeight()
            )
            Spacer(Modifier.width(5.dp))
            LevelMeter(
                value = meters.getOrNull(channel) ?: 0f,
                modifier = Modifier.width(7.dp).fillMaxHeight().padding(vertical = 8.dp)
            )
        }
        SendBar(
            label = "REV",
            value = data?.reverb ?: 0f,
            color = color
        ) { engine.setChannelParam(channel, ChannelParam.REVERB, it) }
        SendBar(
            label = "DLY",
            value = data?.delay ?: 0f,
            color = color
        ) { engine.setChannelParam(channel, ChannelParam.DELAY, it) }
    }
}

@Composable
private fun MasterStrip(
    state: EngineUiState,
    meters: List<Float>,
    engine: EngineController,
    modifier: Modifier = Modifier
) {
    Rt8Panel(
        modifier = modifier,
        accent = Cyan,
        contentPadding = androidx.compose.foundation.layout.PaddingValues(8.dp)
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text("MASTER", style = MaterialTheme.typography.titleMedium.copy(fontSize = 14.sp), color = Cyan)
                Text(
                    "${state.audio.apiLabel} · ${String.format(Locale.US, "%.1f", state.audio.latencyMs)} MS",
                    style = MaterialTheme.typography.labelSmall.copy(fontSize = 7.sp),
                    color = TextMuted
                )
            }
            Text(
                "${(state.audio.cpuLoad * 100).roundToInt()}%",
                style = MaterialTheme.typography.labelMedium.copy(fontSize = 10.sp),
                color = if (state.audio.cpuLoad < 0.55f) Green else Coral
            )
        }
        Spacer(Modifier.height(6.dp))
        Row(
            modifier = Modifier.weight(1f).fillMaxWidth(),
            horizontalArrangement = Arrangement.Center
        ) {
            VerticalFader(
                value = state.master.volume,
                onValueChange = { engine.setMasterParam(MasterParam.VOLUME, it) },
                accent = Cyan,
                modifier = Modifier.width(40.dp).fillMaxHeight()
            )
            Spacer(Modifier.width(8.dp))
            LevelMeter(
                value = meters.getOrNull(11) ?: 0f,
                modifier = Modifier.width(8.dp).fillMaxHeight().padding(vertical = 8.dp)
            )
            LevelMeter(
                value = meters.getOrNull(12) ?: 0f,
                modifier = Modifier.width(8.dp).fillMaxHeight().padding(vertical = 8.dp)
            )
        }
        Spacer(Modifier.height(5.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
            MiniKnob(
                value = state.master.drive,
                onValueChange = { engine.setMasterParam(MasterParam.DRIVE, it) },
                label = "DRIVE",
                valueText = "${(state.master.drive * 100).roundToInt()}",
                accent = Amber,
                modifier = Modifier.weight(1f)
            )
            MiniKnob(
                value = state.master.reverb,
                onValueChange = { engine.setMasterParam(MasterParam.REVERB, it) },
                label = "REV",
                valueText = "${(state.master.reverb * 100).roundToInt()}",
                accent = Cyan,
                modifier = Modifier.weight(1f)
            )
            MiniKnob(
                value = state.master.delay,
                onValueChange = { engine.setMasterParam(MasterParam.DELAY, it) },
                label = "DLY",
                valueText = "${(state.master.delay * 100).roundToInt()}",
                accent = Cyan,
                modifier = Modifier.weight(1f)
            )
        }
        Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
            MiniKnob(
                value = state.master.filter,
                onValueChange = { engine.setMasterParam(MasterParam.FILTER, it) },
                label = "FILTER",
                valueText = filterText(state.master.filter),
                accent = TextSecondary,
                modifier = Modifier.weight(1f)
            )
            MiniKnob(
                value = state.master.scatterDepth,
                onValueChange = { engine.setMasterParam(MasterParam.SCATTER_DEPTH, it) },
                label = "SCAT",
                valueText = "${(state.master.scatterDepth * 100).roundToInt()}",
                accent = Color(0xFFA78BFA),
                modifier = Modifier.weight(1f)
            )
            Box(
                modifier = Modifier
                    .weight(1f)
                    .height(40.dp)
                    .clip(RoundedCornerShape(6.dp))
                    .background(if (state.master.scatterActive) Color(0xFFA78BFA) else PanelHigh)
                    .border(1.dp, if (state.master.scatterActive) Color(0xFFA78BFA) else Line, RoundedCornerShape(6.dp))
                    .clickable { engine.setScatterActive(!state.master.scatterActive) },
                contentAlignment = Alignment.Center
            ) {
                Text(
                    "SCATTER",
                    style = MaterialTheme.typography.labelSmall.copy(fontSize = 7.sp),
                    color = if (state.master.scatterActive) Ink else Color(0xFFA78BFA)
                )
            }
        }
        Spacer(Modifier.height(4.dp))
        Text(
            "SCATTER TYPE",
            style = MaterialTheme.typography.labelSmall.copy(fontSize = 7.sp),
            color = TextMuted
        )
        Row(horizontalArrangement = Arrangement.spacedBy(2.dp)) {
            repeat(8) { i ->
                val type = i + 1
                Box(
                    modifier = Modifier
                        .weight(1f)
                        .height(18.dp)
                        .clip(RoundedCornerShape(3.dp))
                        .background(if (state.master.scatterType == type) Amber else PanelHigh)
                        .clickable { engine.setScatterType(type) },
                    contentAlignment = Alignment.Center
                ) {
                    Text(
                        "$type",
                        style = MaterialTheme.typography.labelSmall.copy(fontSize = 7.sp),
                        color = if (state.master.scatterType == type) Ink else TextMuted
                    )
                }
            }
        }
        Spacer(Modifier.height(4.dp))
        Text(
            "${state.muteMask.countOneBits()} MUTED · ${state.soloMask.countOneBits()} SOLOED",
            style = MaterialTheme.typography.labelSmall.copy(fontSize = 7.sp),
            color = TextMuted
        )
    }
}

@Composable
private fun MiniToggle(
    label: String,
    selected: Boolean,
    accent: Color,
    modifier: Modifier = Modifier,
    onClick: () -> Unit
) {
    Box(
        modifier = modifier
            .height(22.dp)
            .clip(RoundedCornerShape(4.dp))
            .background(if (selected) accent else PanelHigh)
            .border(1.dp, if (selected) accent else Line, RoundedCornerShape(4.dp))
            .clickable(onClick = onClick),
        contentAlignment = Alignment.Center
    ) {
        Text(
            label,
            style = MaterialTheme.typography.labelSmall.copy(fontSize = 8.sp),
            color = if (selected) Ink else TextMuted
        )
    }
}

@Composable
private fun SendBar(
    label: String,
    value: Float,
    color: Color,
    onChange: (Float) -> Unit
) {
    var widthPx by remember { mutableFloatStateOf(1f) }
    Row(
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(3.dp)
    ) {
        Text(
            label,
            style = MaterialTheme.typography.labelSmall.copy(fontSize = 6.sp),
            color = TextMuted
        )
        Box(
            modifier = Modifier
                .weight(1f)
                .height(12.dp)
                .onSizeChanged { widthPx = it.width.toFloat().coerceAtLeast(1f) }
                .pointerInput(Unit) {
                    detectDragGestures(
                        onDragStart = { offset -> onChange((offset.x / widthPx).coerceIn(0f, 1f)) },
                        onDrag = { change, _ ->
                            change.consume()
                            onChange((change.position.x / widthPx).coerceIn(0f, 1f))
                        }
                    )
                }
        ) {
            Canvas(Modifier.fillMaxSize()) {
                val h = size.height
                drawRoundRect(
                    color = Line.copy(alpha = 0.8f),
                    size = size,
                    cornerRadius = CornerRadius(h / 2f, h / 2f)
                )
                drawRoundRect(
                    color = color.copy(alpha = 0.8f),
                    size = Size(size.width * value.coerceIn(0f, 1f), h),
                    cornerRadius = CornerRadius(h / 2f, h / 2f)
                )
                drawCircle(
                    color = TextPrimary,
                    radius = 2.dp.toPx(),
                    center = Offset(size.width * value.coerceIn(0f, 1f), h / 2f)
                )
            }
        }
        Text(
            "${(value * 100).roundToInt()}",
            style = MaterialTheme.typography.labelSmall.copy(fontSize = 6.sp),
            color = TextSecondary,
            modifier = Modifier.width(16.dp)
        )
    }
}

private fun filterText(value: Float): String = when {
    value < 0.48f -> "LP"
    value > 0.52f -> "HP"
    else -> "OFF"
}
