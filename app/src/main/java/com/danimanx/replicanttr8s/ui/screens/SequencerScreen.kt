package com.danimanx.replicanttr8s.ui.screens

import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.AutoAwesome
import androidx.compose.material.icons.filled.Bolt
import androidx.compose.material.icons.filled.ContentCopy
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.GridOn
import androidx.compose.material.icons.filled.Tune
import androidx.compose.material3.MaterialTheme
import androidx.compose.animation.Crossfade
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.danimanx.replicanttr8s.audio.EngineController
import com.danimanx.replicanttr8s.model.ChannelLongNames
import com.danimanx.replicanttr8s.model.ChannelShortNames
import com.danimanx.replicanttr8s.model.ChannelParam
import com.danimanx.replicanttr8s.model.EngineUiState
import com.danimanx.replicanttr8s.model.MasterParam
import com.danimanx.replicanttr8s.ui.components.AccentPad
import com.danimanx.replicanttr8s.ui.components.BigPlayButton
import com.danimanx.replicanttr8s.ui.components.LedDot
import com.danimanx.replicanttr8s.ui.components.HorizontalMeter
import com.danimanx.replicanttr8s.ui.components.LevelBar
import com.danimanx.replicanttr8s.ui.components.LevelMeter
import com.danimanx.replicanttr8s.ui.components.MiniKnob
import com.danimanx.replicanttr8s.ui.components.RotaryKnob
import com.danimanx.replicanttr8s.ui.components.Rt8Button
import com.danimanx.replicanttr8s.ui.components.Rt8Panel
import com.danimanx.replicanttr8s.ui.components.SectionLabel
import com.danimanx.replicanttr8s.ui.components.StepPad
import com.danimanx.replicanttr8s.ui.components.ToggleChip
import com.danimanx.replicanttr8s.ui.components.TransportButton
import com.danimanx.replicanttr8s.ui.components.VariationChip
import com.danimanx.replicanttr8s.ui.theme.Amber
import com.danimanx.replicanttr8s.ui.theme.Cyan
import com.danimanx.replicanttr8s.ui.theme.Ink
import com.danimanx.replicanttr8s.ui.theme.Line
import com.danimanx.replicanttr8s.ui.theme.PanelHigh
import com.danimanx.replicanttr8s.ui.theme.TextMuted
import com.danimanx.replicanttr8s.ui.theme.TextPrimary
import com.danimanx.replicanttr8s.ui.theme.TextSecondary
import com.danimanx.replicanttr8s.ui.theme.channelColor
import kotlin.math.abs
import kotlin.math.roundToInt

@Composable
fun SequencerScreen(
    state: EngineUiState,
    meters: List<Float>,
    scope: List<Float>,
    engine: EngineController,
    onTempoClick: () -> Unit,
    onOpenTones: () -> Unit
) {
    var compactView by rememberSaveable { mutableIntStateOf(0) }
    BoxWithConstraints(Modifier.fillMaxSize()) {
        val wide = maxWidth >= 720.dp
        val tall = wide && maxHeight >= 900.dp
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(horizontal = 12.dp, vertical = 10.dp),
            verticalArrangement = Arrangement.spacedBy(9.dp)
        ) {
            ChannelSelector(state, meters, engine)
            if (wide) {
                Row(
                    modifier = Modifier.weight(1f),
                    horizontalArrangement = Arrangement.spacedBy(10.dp)
                ) {
                    Column(
                        modifier = Modifier.widthIn(min = 340.dp, max = 410.dp).fillMaxHeight(),
                        verticalArrangement = Arrangement.spacedBy(9.dp)
                    ) {
                        PerformancePanel(
                            state = state,
                            scope = scope,
                            engine = engine,
                            onOpenTones = onOpenTones,
                            modifier = Modifier.weight(1f).fillMaxWidth()
                        )
                        if (tall) {
                            MasterPanel(
                                state = state,
                                meters = meters,
                                engine = engine,
                                modifier = Modifier.height(280.dp).fillMaxWidth()
                            )
                        }
                    }
                    StepPanel(
                        state = state,
                        engine = engine,
                        compact = false,
                        modifier = Modifier.weight(1f).fillMaxHeight()
                    )
                }
            } else {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    Rt8Button(
                        text = "PERFORM",
                        accent = channelColor(state.selectedChannel),
                        selected = compactView == 0,
                        modifier = Modifier.weight(1f)
                    ) { compactView = 0 }
                    Rt8Button(
                        text = "STEPS",
                        accent = channelColor(state.selectedChannel),
                        selected = compactView == 1,
                        modifier = Modifier.weight(1f)
                    ) { compactView = 1 }
                }
                Crossfade(
                    targetState = compactView,
                    label = "compact-view",
                    modifier = Modifier.weight(1f).fillMaxWidth()
                ) { view ->
                    if (view == 0) {
                        PerformancePanel(
                            state = state,
                            scope = scope,
                            engine = engine,
                            onOpenTones = onOpenTones,
                            compact = false,
                            modifier = Modifier.fillMaxSize()
                        )
                    } else {
                        StepPanel(
                            state = state,
                            engine = engine,
                            compact = true,
                            modifier = Modifier.fillMaxSize()
                        )
                    }
                }
            }
            TransportBar(state, engine, onTempoClick, wide = wide)
        }
    }
}

@Composable
private fun ChannelSelector(
    state: EngineUiState,
    meters: List<Float>,
    engine: EngineController
) {
    val scroll = rememberScrollState()
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .horizontalScroll(scroll),
        horizontalArrangement = Arrangement.spacedBy(6.dp)
    ) {
        repeat(11) { ch ->
            val channel = state.channels.getOrNull(ch)
            val color = channelColor(ch)
            val selected = state.selectedChannel == ch
            val level = meters.getOrNull(ch) ?: 0f
            Column(
                modifier = Modifier
                    .width(66.dp)
                    .height(48.dp)
                    .clip(RoundedCornerShape(7.dp))
                    .background(
                        if (selected) {
                            Brush.verticalGradient(listOf(color.copy(alpha = 0.36f), color.copy(alpha = 0.12f)))
                        } else {
                            Brush.verticalGradient(listOf(PanelHigh, PanelHigh.copy(alpha = 0.76f)))
                        }
                    )
                    .border(
                        if (selected) 1.5.dp else 1.dp,
                        if (selected) color else Line,
                        RoundedCornerShape(7.dp)
                    )
                    .clickable { engine.selectChannel(ch) }
                    .padding(horizontal = 6.dp, vertical = 5.dp),
                verticalArrangement = Arrangement.SpaceBetween
            ) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Box(
                        Modifier
                            .size(6.dp)
                            .clip(RoundedCornerShape(50))
                            .background(if (channel?.muted == true) TextMuted else color)
                    )
                    Spacer(Modifier.width(5.dp))
                    Text(
                        channel?.name ?: "—",
                        style = MaterialTheme.typography.labelMedium.copy(fontSize = 11.sp),
                        color = if (selected) TextPrimary else color
                    )
                    Spacer(Modifier.weight(1f))
                    if (channel?.soloed == true) {
                        Text("S", style = MaterialTheme.typography.labelSmall.copy(fontSize = 8.sp), color = Amber)
                    }
                }
                LevelBar(
                    value = level,
                    color = color,
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(3.dp)
                )
            }
        }
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun PerformancePanel(
    state: EngineUiState,
    scope: List<Float>,
    engine: EngineController,
    onOpenTones: () -> Unit,
    modifier: Modifier = Modifier,
    compact: Boolean = false
) {
    val channel = state.channels.getOrNull(state.selectedChannel)
    val color = channelColor(state.selectedChannel)
    val name = channel?.name ?: "BD"
    val fullName = ChannelLongNames.getOrElse(state.selectedChannel) { "" }

    Rt8Panel(modifier = modifier, accent = color, contentPadding = PaddingValues(10.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Box(
                Modifier
                    .size(width = 4.dp, height = 34.dp)
                    .clip(RoundedCornerShape(2.dp))
                    .background(color)
            )
            Spacer(Modifier.width(8.dp))
            Column(Modifier.weight(1f)) {
                Text(
                    "$name // $fullName",
                    style = MaterialTheme.typography.titleMedium,
                    color = TextPrimary,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
                Text(
                    channel?.toneName ?: "TONE",
                    style = MaterialTheme.typography.labelSmall,
                    color = color,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                    modifier = Modifier.clickable(onClick = onOpenTones)
                )
            }
            ToggleChip(
                text = "M",
                selected = channel?.muted == true,
                accent = Amber,
                modifier = Modifier.width(34.dp).height(28.dp)
            ) { engine.setMute(state.selectedChannel, channel?.muted != true) }
            Spacer(Modifier.width(4.dp))
            ToggleChip(
                text = "S",
                selected = channel?.soloed == true,
                accent = Cyan,
                modifier = Modifier.width(34.dp).height(28.dp)
            ) { engine.setSolo(state.selectedChannel, channel?.soloed != true) }
        }

        Spacer(Modifier.height(8.dp))

        if (compact) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                KnobCell("TUNE", channel?.tune ?: 0.5f, color, tuneText(channel?.tune ?: 0.5f)) {
                    engine.setChannelParam(state.selectedChannel, ChannelParam.TUNE, it)
                }
                KnobCell("DECAY", channel?.decay ?: 0.5f, color, pct(channel?.decay ?: 0.5f)) {
                    engine.setChannelParam(state.selectedChannel, ChannelParam.DECAY, it)
                }
                KnobCell("REVERB", channel?.reverb ?: 0.2f, color, pct(channel?.reverb ?: 0.2f)) {
                    engine.setChannelParam(state.selectedChannel, ChannelParam.REVERB, it)
                }
                KnobCell("DELAY", channel?.delay ?: 0.1f, color, pct(channel?.delay ?: 0.1f)) {
                    engine.setChannelParam(state.selectedChannel, ChannelParam.DELAY, it)
                }
                KnobCell("CTRL", channel?.ctrl ?: 0.5f, color, pct(channel?.ctrl ?: 0.5f)) {
                    engine.setChannelParam(state.selectedChannel, ChannelParam.CTRL, it)
                }
                KnobCell("LEVEL", channel?.level ?: 1f, color, "${((channel?.level ?: 1f) * 150).roundToInt()}") {
                    engine.setChannelParam(state.selectedChannel, ChannelParam.LEVEL, it)
                }
            }
            Spacer(Modifier.height(8.dp))
            TriggerPad(
                name = name,
                color = color,
                modifier = Modifier.fillMaxWidth().height(42.dp),
                onTrigger = { engine.trigger(state.selectedChannel) },
                onFlam = { engine.triggerFlam(state.selectedChannel) }
            )
        } else {
            Row(
                modifier = Modifier.weight(1f).fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(8.dp),
                verticalAlignment = Alignment.Top
            ) {
                Column(
                    modifier = Modifier.width(120.dp).fillMaxHeight(),
                    verticalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    TriggerPad(
                        name = name,
                        color = color,
                        modifier = Modifier.fillMaxWidth().weight(0.66f),
                        onTrigger = { engine.trigger(state.selectedChannel) },
                        onFlam = { engine.triggerFlam(state.selectedChannel) }
                    )
                    ChannelQuickPads(
                        state = state,
                        engine = engine,
                        modifier = Modifier.fillMaxWidth().weight(0.34f)
                    )
                }
                Column(
                    modifier = Modifier.weight(1f).fillMaxHeight(),
                    verticalArrangement = Arrangement.spacedBy(9.dp)
                ) {
                    Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                        KnobCell("TUNE", channel?.tune ?: 0.5f, color, tuneText(channel?.tune ?: 0.5f)) {
                            engine.setChannelParam(state.selectedChannel, ChannelParam.TUNE, it)
                        }
                        KnobCell("DECAY", channel?.decay ?: 0.5f, color, pct(channel?.decay ?: 0.5f)) {
                            engine.setChannelParam(state.selectedChannel, ChannelParam.DECAY, it)
                        }
                        KnobCell("REVERB", channel?.reverb ?: 0.2f, color, pct(channel?.reverb ?: 0.2f)) {
                            engine.setChannelParam(state.selectedChannel, ChannelParam.REVERB, it)
                        }
                    }
                    Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                        KnobCell("DELAY", channel?.delay ?: 0.1f, color, pct(channel?.delay ?: 0.1f)) {
                            engine.setChannelParam(state.selectedChannel, ChannelParam.DELAY, it)
                        }
                        KnobCell("CTRL", channel?.ctrl ?: 0.5f, color, pct(channel?.ctrl ?: 0.5f)) {
                            engine.setChannelParam(state.selectedChannel, ChannelParam.CTRL, it)
                        }
                        KnobCell("LEVEL", channel?.level ?: 1f, color, "${((channel?.level ?: 1f) * 150).roundToInt()}") {
                            engine.setChannelParam(state.selectedChannel, ChannelParam.LEVEL, it)
                        }
                    }
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Text(
                            "LIVE BUS",
                            style = MaterialTheme.typography.labelSmall.copy(fontSize = 8.sp),
                            color = TextMuted
                        )
                        Spacer(Modifier.weight(1f))
                        Text(
                            channel?.toneName?.uppercase() ?: "",
                            style = MaterialTheme.typography.labelSmall.copy(fontSize = 8.sp),
                            color = color,
                            maxLines = 1
                        )
                    }
                    Box(
                        Modifier
                            .fillMaxWidth()
                            .weight(1f)
                            .heightIn(min = 58.dp)
                            .clip(RoundedCornerShape(6.dp))
                            .background(Color(0xFF040A0A))
                            .border(1.dp, color.copy(alpha = 0.22f), RoundedCornerShape(6.dp))
                    ) {
                        com.danimanx.replicanttr8s.ui.components.ScopeCanvas(
                            samples = scope,
                            accent = color,
                            modifier = Modifier.fillMaxSize().padding(3.dp)
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun KnobCell(
    label: String,
    value: Float,
    color: Color,
    valueText: String,
    onChange: (Float) -> Unit
) {
    RotaryKnob(
        value = value,
        onValueChange = onChange,
        label = label,
        valueText = valueText,
        accent = color,
        size = 50.dp,
        defaultValue = if (label == "LEVEL") 1f else if (label == "TUNE" || label == "CTRL") 0.5f else 0.2f
    )
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun TriggerPad(
    name: String,
    color: Color,
    modifier: Modifier = Modifier,
    onTrigger: () -> Unit,
    onFlam: () -> Unit
) {
    Box(
        modifier = modifier
            .clip(RoundedCornerShape(9.dp))
            .background(
                Brush.verticalGradient(
                    listOf(color.copy(alpha = 0.78f), color.copy(alpha = 0.28f))
                )
            )
            .border(1.5.dp, color, RoundedCornerShape(9.dp))
            .combinedClickable(onClick = onTrigger, onLongClick = onFlam),
        contentAlignment = Alignment.Center
    ) {
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            Text(
                name,
                style = MaterialTheme.typography.titleLarge.copy(fontSize = 22.sp),
                color = Ink
            )
            Text(
                "TRIGGER",
                style = MaterialTheme.typography.labelSmall.copy(fontSize = 7.sp),
                color = Ink.copy(alpha = 0.72f)
            )
        }
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun ChannelQuickPads(
    state: EngineUiState,
    engine: EngineController,
    modifier: Modifier = Modifier
) {
    Column(modifier = modifier, verticalArrangement = Arrangement.spacedBy(4.dp)) {
        repeat(3) { row ->
            Row(
                modifier = Modifier.weight(1f).fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(4.dp)
            ) {
                repeat(4) { col ->
                    val ch = row * 4 + col
                    if (ch < 11) {
                        val chColor = channelColor(ch)
                        val selected = state.selectedChannel == ch
                        Box(
                            modifier = Modifier
                                .weight(1f)
                                .fillMaxHeight()
                                .clip(RoundedCornerShape(5.dp))
                                .background(chColor.copy(alpha = if (selected) 0.32f else 0.12f))
                                .border(
                                    if (selected) 1.5.dp else 1.dp,
                                    if (selected) chColor else Line,
                                    RoundedCornerShape(5.dp)
                                )
                                .combinedClickable(
                                    onClick = { engine.trigger(ch) },
                                    onLongClick = { engine.selectChannel(ch) }
                                ),
                            contentAlignment = Alignment.Center
                        ) {
                            Text(
                                ChannelShortNames.getOrElse(ch) { "—" },
                                style = MaterialTheme.typography.labelSmall.copy(fontSize = 7.sp),
                                color = if (selected) TextPrimary else chColor,
                                maxLines = 1
                            )
                        }
                    } else {
                        Spacer(Modifier.weight(1f))
                    }
                }
            }
        }
    }
}

@Composable
private fun StepPanel(
    state: EngineUiState,
    engine: EngineController,
    compact: Boolean,
    modifier: Modifier = Modifier
) {
    val color = channelColor(state.selectedChannel)
    Rt8Panel(modifier = modifier, accent = color, contentPadding = PaddingValues(10.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(
                "STEPS // ${state.channels.getOrNull(state.selectedChannel)?.name ?: "—"}",
                style = MaterialTheme.typography.titleSmall,
                color = TextPrimary
            )
            Spacer(Modifier.weight(1f))
            Text(
                "VAR ${state.variationLetter}",
                style = MaterialTheme.typography.labelSmall,
                color = color
            )
        }
        Spacer(Modifier.height(7.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
            repeat(8) { v ->
                VariationChip(
                    letter = ('A' + v).toString(),
                    selected = state.variation == v,
                    accent = color,
                    modifier = Modifier.weight(1f)
                ) { engine.setVariation(v) }
            }
        }
        Spacer(Modifier.height(9.dp))

        if (compact) {
            Column(verticalArrangement = Arrangement.spacedBy(7.dp)) {
                repeat(4) { row ->
                    Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        repeat(4) { col ->
                            val step = row * 4 + col
                            StepPad(
                                step = step,
                                velocity = state.velocity(state.selectedChannel, step),
                                isPlaying = state.playing && state.currentStep == step,
                                accent = color,
                                modifier = Modifier.weight(1f),
                                height = 62.dp,
                                onClick = { engine.toggleStep(state.selectedChannel, step) },
                                onLongClick = { engine.cycleVelocity(state.selectedChannel, step) }
                            )
                        }
                    }
                }
            }
            Spacer(Modifier.height(8.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(3.dp)) {
                repeat(16) { step ->
                    AccentPad(
                        step = step,
                        velocity = state.velocity(11, step),
                        isPlaying = state.playing && state.currentStep == step,
                        modifier = Modifier.weight(1f),
                        height = 28.dp
                    ) { engine.toggleAccentStep(step) }
                }
            }
            Spacer(Modifier.weight(1f))
        } else {
            Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                repeat(16) { step ->
                    StepPad(
                        step = step,
                        velocity = state.velocity(state.selectedChannel, step),
                        isPlaying = state.playing && state.currentStep == step,
                        accent = color,
                        modifier = Modifier.weight(1f),
                        height = if (step % 4 == 0) 100.dp else 92.dp,
                        onClick = { engine.toggleStep(state.selectedChannel, step) },
                        onLongClick = { engine.cycleVelocity(state.selectedChannel, step) }
                    )
                }
            }
            Spacer(Modifier.height(4.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                repeat(16) { step ->
                    AccentPad(
                        step = step,
                        velocity = state.velocity(11, step),
                        isPlaying = state.playing && state.currentStep == step,
                        modifier = Modifier.weight(1f)
                    ) { engine.toggleAccentStep(step) }
                }
            }
            Spacer(Modifier.height(9.dp))
            SectionLabel("PATTERN MATRIX // ALL CHANNELS", accent = TextMuted)
            Spacer(Modifier.height(5.dp))
            PatternMatrix(state, engine, Modifier.weight(1f).fillMaxWidth())
        }

        Spacer(Modifier.height(9.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            TransportButton(
                text = "Fill",
                icon = Icons.Filled.Bolt,
                accent = Amber,
                selected = state.fill,
                modifier = Modifier.weight(1f)
            ) { engine.toggleFill() }
            TransportButton(
                text = "Clear",
                icon = Icons.Filled.Delete,
                accent = Color(0xFFFF5A6E),
                modifier = Modifier.weight(1f)
            ) { engine.clearVariation() }
            TransportButton(
                text = "Copy",
                icon = Icons.Filled.ContentCopy,
                accent = Cyan,
                modifier = Modifier.weight(1f)
            ) { engine.copyVariation() }
        }
    }
}

@Composable
private fun PatternMatrix(
    state: EngineUiState,
    engine: EngineController,
    modifier: Modifier = Modifier
) {
    Column(modifier = modifier, verticalArrangement = Arrangement.spacedBy(2.dp)) {
        repeat(11) { track ->
            val color = channelColor(track)
            Row(
                modifier = Modifier.weight(1f).fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(2.dp)
            ) {
                Text(
                    state.channels.getOrNull(track)?.name ?: "—",
                    style = MaterialTheme.typography.labelSmall.copy(fontSize = 7.sp),
                    color = if (state.selectedChannel == track) color else TextMuted,
                    modifier = Modifier.width(20.dp)
                )
                repeat(16) { step ->
                    val velocity = state.velocity(track, step)
                    val alpha = when {
                        velocity >= 100 -> 0.9f
                        velocity >= 70 -> 0.62f
                        velocity > 0 -> 0.34f
                        else -> 0.10f
                    }
                    val playhead = state.playing && state.currentStep == step
                    Box(
                        Modifier
                            .weight(1f)
                            .fillMaxHeight()
                            .clip(RoundedCornerShape(2.dp))
                            .background(
                                if (state.selectedChannel == track) color.copy(alpha = alpha)
                                else color.copy(alpha = alpha * 0.75f)
                            )
                            .drawBehind {
                                if (playhead) {
                                    drawRect(Color.White.copy(alpha = 0.16f))
                                    drawLine(
                                        color = Color.White.copy(alpha = 0.55f),
                                        start = Offset(1.dp.toPx(), 0f),
                                        end = Offset(1.dp.toPx(), size.height),
                                        strokeWidth = 1.5.dp.toPx()
                                    )
                                }
                            }
                            .clickable { engine.toggleStep(track, step) }
                    )
                }
            }
        }
        Spacer(Modifier.height(1.dp))
        Text(
            "TAP A CELL TO TOGGLE · SELECT A CHANNEL ABOVE TO EDIT WITH STEP PADS",
            style = MaterialTheme.typography.labelSmall.copy(fontSize = 7.sp),
            color = TextMuted
        )
    }
}

@Composable
private fun TransportBar(
    state: EngineUiState,
    engine: EngineController,
    onTempoClick: () -> Unit,
    wide: Boolean = false
) {
    val scroll = rememberScrollState()
    Rt8Panel(
        modifier = Modifier.fillMaxWidth(),
        contentPadding = PaddingValues(horizontal = 10.dp, vertical = 8.dp)
    ) {
        Row(
            modifier = if (wide) Modifier.fillMaxWidth()
            else Modifier.fillMaxWidth().horizontalScroll(scroll),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(10.dp)
        ) {
            BigPlayButton(
                playing = state.playing,
                onClick = { engine.togglePlaying() },
                accent = Cyan,
                size = 48
            )
            Column(
                modifier = Modifier
                    .clip(RoundedCornerShape(7.dp))
                    .background(PanelHigh)
                    .border(1.dp, Line, RoundedCornerShape(7.dp))
                    .clickable(onClick = onTempoClick)
                    .padding(horizontal = 14.dp, vertical = 4.dp),
                horizontalAlignment = Alignment.CenterHorizontally
            ) {
                Text(
                    "${state.tempo.roundToInt()}",
                    style = MaterialTheme.typography.headlineMedium,
                    color = Cyan,
                    maxLines = 1
                )
                Text(
                    "BPM // TAP",
                    style = MaterialTheme.typography.labelSmall.copy(fontSize = 7.sp),
                    color = TextMuted
                )
            }
            RotaryKnob(
                value = state.shuffle,
                onValueChange = { engine.setShuffle(it) },
                label = "SWING",
                valueText = pct(state.shuffle),
                accent = Amber,
                size = 38.dp,
                defaultValue = 0f
            )
            RotaryKnob(
                value = state.accent,
                onValueChange = { engine.setAccent(it) },
                label = "ACCENT",
                valueText = pct(state.accent),
                accent = Color(0xFFFF5A6E),
                size = 38.dp,
                defaultValue = 0.65f
            )
            ToggleChip(
                text = "FILL",
                selected = state.fill,
                accent = Amber
            ) { engine.toggleFill() }
            ToggleChip(
                text = "SCATTER ${state.master.scatterType}",
                selected = state.master.scatterActive,
                accent = Color(0xFFA78BFA)
            ) {
                engine.setScatterActive(!state.master.scatterActive)
                if (!state.master.scatterActive) {
                    engine.setScatterType((state.master.scatterType % 8) + 1)
                }
            }
            if (wide) {
                Spacer(Modifier.weight(1f))
                Text(
                    "AAUDIO ${state.audio.sampleRate / 1000}K · ${state.audio.bufferFrames} FR · CPU ${(state.audio.cpuLoad * 100).roundToInt()}%",
                    style = MaterialTheme.typography.labelSmall.copy(fontSize = 8.sp),
                    color = TextMuted
                )
                Text(
                    "${state.patternCode} // ${state.patternName.uppercase()}",
                    style = MaterialTheme.typography.labelSmall.copy(fontSize = 8.sp),
                    color = TextSecondary
                )
            }
        }
    }
}

@Composable
private fun MasterPanel(
    state: EngineUiState,
    meters: List<Float>,
    engine: EngineController,
    modifier: Modifier = Modifier
) {
    Rt8Panel(modifier = modifier, accent = Cyan, contentPadding = PaddingValues(10.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text("MASTER BUS // FX", style = MaterialTheme.typography.titleSmall, color = Cyan)
            Spacer(Modifier.weight(1f))
            Text(
                "${state.audio.apiLabel} · ${state.audio.sharingLabel} · ${state.audio.latencyMs.toInt()} MS",
                style = MaterialTheme.typography.labelSmall.copy(fontSize = 8.sp),
                color = TextMuted
            )
        }
        Spacer(Modifier.height(6.dp))
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            MiniKnob(
                value = state.master.volume,
                onValueChange = { engine.setMasterParam(MasterParam.VOLUME, it) },
                label = "VOLUME",
                valueText = pct(state.master.volume),
                accent = Cyan,
                modifier = Modifier.weight(1f)
            )
            MiniKnob(
                value = state.master.drive,
                onValueChange = { engine.setMasterParam(MasterParam.DRIVE, it) },
                label = "DRIVE",
                valueText = pct(state.master.drive),
                accent = Amber,
                modifier = Modifier.weight(1f)
            )
            MiniKnob(
                value = state.master.reverb,
                onValueChange = { engine.setMasterParam(MasterParam.REVERB, it) },
                label = "REVERB",
                valueText = pct(state.master.reverb),
                accent = Cyan,
                modifier = Modifier.weight(1f)
            )
            MiniKnob(
                value = state.master.delay,
                onValueChange = { engine.setMasterParam(MasterParam.DELAY, it) },
                label = "DELAY",
                valueText = pct(state.master.delay),
                accent = Cyan,
                modifier = Modifier.weight(1f)
            )
            MiniKnob(
                value = state.master.filter,
                onValueChange = { engine.setMasterParam(MasterParam.FILTER, it) },
                label = "FILTER",
                valueText = filterLabel(state.master.filter),
                accent = TextSecondary,
                modifier = Modifier.weight(1f)
            )
            MiniKnob(
                value = state.master.scatterDepth,
                onValueChange = { engine.setMasterParam(MasterParam.SCATTER_DEPTH, it) },
                label = "SCAT",
                valueText = pct(state.master.scatterDepth),
                accent = Color(0xFFA78BFA),
                modifier = Modifier.weight(1f)
            )
        }
        Spacer(Modifier.height(6.dp))
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text("SCATTER TYPE", style = MaterialTheme.typography.labelSmall.copy(fontSize = 8.sp), color = TextMuted)
            Spacer(Modifier.width(8.dp))
            repeat(8) { i ->
                val type = i + 1
                Box(
                    modifier = Modifier
                        .padding(horizontal = 2.dp)
                        .weight(1f)
                        .height(24.dp)
                        .clip(RoundedCornerShape(4.dp))
                        .background(if (state.master.scatterType == type) Amber else PanelHigh)
                        .border(1.dp, if (state.master.scatterType == type) Amber else Line, RoundedCornerShape(4.dp))
                        .clickable { engine.setScatterType(type) },
                    contentAlignment = Alignment.Center
                ) {
                    Text(
                        "$type",
                        style = MaterialTheme.typography.labelSmall.copy(fontSize = 8.sp),
                        color = if (state.master.scatterType == type) Ink else TextMuted
                    )
                }
            }
            Spacer(Modifier.width(6.dp))
            ToggleChip(
                text = if (state.master.scatterActive) "SCATTER ON" else "SCATTER OFF",
                selected = state.master.scatterActive,
                accent = Color(0xFFA78BFA),
                modifier = Modifier.width(110.dp)
            ) { engine.setScatterActive(!state.master.scatterActive) }
        }
        Spacer(Modifier.height(7.dp))
        Row(
            modifier = Modifier.fillMaxWidth().height(52.dp),
            horizontalArrangement = Arrangement.spacedBy(3.dp)
        ) {
            repeat(11) { ch ->
                Column(
                    modifier = Modifier.weight(1f).fillMaxHeight(),
                    horizontalAlignment = Alignment.CenterHorizontally
                ) {
                    LevelMeter(
                        value = meters.getOrNull(ch) ?: 0f,
                        segments = 8,
                        modifier = Modifier.weight(1f).width(6.dp)
                    )
                    Text(
                        ChannelShortNames.getOrElse(ch) { "—" },
                        style = MaterialTheme.typography.labelSmall.copy(fontSize = 6.sp),
                        color = TextMuted
                    )
                }
            }
        }
        Spacer(Modifier.height(5.dp))
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text("MASTER LEVEL", style = MaterialTheme.typography.labelSmall.copy(fontSize = 8.sp), color = TextMuted)
            Spacer(Modifier.width(8.dp))
            HorizontalMeter(
                value = maxOf(meters.getOrNull(11) ?: 0f, meters.getOrNull(12) ?: 0f),
                modifier = Modifier.weight(1f).height(6.dp)
            )
        }
    }
}

private fun filterLabel(value: Float): String = when {
    value < 0.48f -> "LP"
    value > 0.52f -> "HP"
    else -> "OFF"
}

private fun pct(value: Float): String = "${(value.coerceIn(0f, 1f) * 100).roundToInt()}"

private fun tuneText(value: Float): String {
    val semis = ((value - 0.5f) * 24f).roundToInt()
    return if (semis == 0) "0" else if (semis > 0) "+$semis" else "$semis"
}
