package com.danimanx.replicanttr8s.ui

import androidx.compose.animation.Crossfade
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.GridOn
import androidx.compose.material.icons.filled.LibraryMusic
import androidx.compose.material.icons.filled.Tune
import androidx.compose.material.icons.filled.Usb
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Snackbar
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text
import androidx.compose.material3.windowsizeclass.WindowSizeClass
import androidx.compose.material3.windowsizeclass.WindowWidthSizeClass
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
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.danimanx.replicanttr8s.BuildConfig
import com.danimanx.replicanttr8s.MainViewModel
import com.danimanx.replicanttr8s.ui.components.AppHeader
import com.danimanx.replicanttr8s.ui.components.TempoDialog
import com.danimanx.replicanttr8s.ui.screens.EngineBootScreen
import com.danimanx.replicanttr8s.ui.screens.FirmwareGateScreen
import com.danimanx.replicanttr8s.ui.screens.KitScreen
import com.danimanx.replicanttr8s.ui.screens.MidiScreen
import com.danimanx.replicanttr8s.ui.screens.MixerScreen
import com.danimanx.replicanttr8s.ui.screens.SequencerScreen
import com.danimanx.replicanttr8s.ui.theme.BackgroundGradient
import com.danimanx.replicanttr8s.ui.theme.Cyan
import com.danimanx.replicanttr8s.ui.theme.Ink
import com.danimanx.replicanttr8s.ui.theme.Line
import com.danimanx.replicanttr8s.ui.theme.Panel
import com.danimanx.replicanttr8s.ui.theme.TextMuted
import com.danimanx.replicanttr8s.ui.theme.TextPrimary
import com.danimanx.replicanttr8s.ui.theme.Violet

private enum class Rt8Tab(val label: String, val icon: ImageVector) {
    SEQUENCER("SEQUENCE", Icons.Filled.GridOn),
    MIXER("MIXER", Icons.Filled.Tune),
    KITS("KITS", Icons.Filled.LibraryMusic),
    MIDI("MIDI", Icons.Filled.Usb)
}

@Composable
fun ReplicantApp(windowSizeClass: WindowSizeClass, viewModel: MainViewModel) {
    val state by viewModel.engineState.collectAsStateWithLifecycle()
    val meters by viewModel.meters.collectAsStateWithLifecycle()
    val scope by viewModel.scope.collectAsStateWithLifecycle()
    val catalog by viewModel.catalog.collectAsStateWithLifecycle()
    val midiState by viewModel.midiState.collectAsStateWithLifecycle()
    val importing by viewModel.importing.collectAsStateWithLifecycle()
    val firmwareMessage by viewModel.firmwareMessage.collectAsStateWithLifecycle()

    val expanded = windowSizeClass.widthSizeClass == WindowWidthSizeClass.Expanded
    val compactHeader = windowSizeClass.widthSizeClass != WindowWidthSizeClass.Expanded
    var tab by rememberSaveable { mutableStateOf(Rt8Tab.SEQUENCER) }
    var tempoDialog by remember { mutableStateOf(false) }
    val snackbar = remember { SnackbarHostState() }

    LaunchedEffect(Unit) {
        viewModel.messages.collect { snackbar.showSnackbar(it) }
    }
    var lastEngineError by remember { mutableStateOf<String?>(null) }
    LaunchedEffect(state.engineError) {
        val error = state.engineError
        if (error == null) {
            lastEngineError = null
        } else if (error != lastEngineError) {
            lastEngineError = error
            snackbar.showSnackbar(error)
        }
    }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Brush.verticalGradient(BackgroundGradient))
            .drawBehind {
                val grid = Line.copy(alpha = 0.14f)
                val step = 52.dp.toPx()
                var x = 0f
                while (x < size.width) {
                    drawLine(grid, Offset(x, 0f), Offset(x, size.height), 1f)
                    x += step
                }
                var y = 0f
                while (y < size.height) {
                    drawLine(grid.copy(alpha = 0.08f), Offset(0f, y), Offset(size.width, y), 1f)
                    y += step
                }
                drawRect(
                    brush = Brush.radialGradient(
                        colors = listOf(Cyan.copy(alpha = 0.055f), Color.Transparent),
                        center = Offset(size.width * 0.52f, 0f),
                        radius = size.height * 0.55f
                    )
                )
            }
    ) {
        when {
            !state.ready -> EngineBootScreen()
            !state.firmwareLoaded -> FirmwareGateScreen(
                importing = importing,
                message = firmwareMessage ?: state.engineError,
                onImport = { viewModel.importFirmware(it) }
            )
            else -> Scaffold(
            containerColor = Color.Transparent,
            contentWindowInsets = WindowInsets(0, 0, 0, 0),
            snackbarHost = {
                SnackbarHost(snackbar) { data ->
                    Snackbar(
                        snackbarData = data,
                        containerColor = Panel,
                        contentColor = TextPrimary,
                        shape = RoundedCornerShape(8.dp)
                    )
                }
            },
            bottomBar = {
                if (!expanded) {
                    Rt8BottomBar(tab) {
                        tab = it
                        if (it == Rt8Tab.MIDI) viewModel.midi.refreshDevices()
                    }
                }
            }
        ) { padding ->
            Row(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(padding)
            ) {
                if (expanded) {
                    Rt8Rail(
                        tab = tab,
                        latencyMs = state.audio.latencyMs,
                        onSelect = {
                            tab = it
                            if (it == Rt8Tab.MIDI) viewModel.midi.refreshDevices()
                        }
                    )
                }
                Column(Modifier.weight(1f).fillMaxHeight()) {
                    AppHeader(
                        state = state,
                        midi = midiState,
                        scope = scope,
                        compact = compactHeader,
                        onPlayToggle = { viewModel.engine.togglePlaying() },
                        onTempoClick = { tempoDialog = true }
                    )
                    Box(
                        modifier = Modifier
                            .weight(1f)
                            .fillMaxWidth()
                    ) {
                        Crossfade(
                            targetState = tab,
                            animationSpec = tween(180),
                            label = "tab"
                        ) { target ->
                            when (target) {
                                Rt8Tab.SEQUENCER -> SequencerScreen(
                                    state = state,
                                    meters = meters,
                                    scope = scope,
                                    engine = viewModel.engine,
                                    onTempoClick = { tempoDialog = true },
                                    onOpenTones = { tab = Rt8Tab.KITS }
                                )
                                Rt8Tab.MIXER -> MixerScreen(
                                    state = state,
                                    meters = meters,
                                    engine = viewModel.engine
                                )
                                Rt8Tab.KITS -> KitScreen(
                                    state = state,
                                    catalog = catalog,
                                    engine = viewModel.engine,
                                    viewModel = viewModel
                                )
                                Rt8Tab.MIDI -> MidiScreen(
                                    state = state,
                                    midi = midiState,
                                    engine = viewModel.engine,
                                    controller = viewModel.midi
                                )
                            }
                        }
                    }
                }
            }
        }
        }
    }

    if (tempoDialog) {
        TempoDialog(
            tempo = state.tempo,
            onChange = { viewModel.engine.setTempo(it) },
            onDismiss = { tempoDialog = false }
        )
    }
}

@Composable
private fun Rt8Rail(
    tab: Rt8Tab,
    latencyMs: Double,
    onSelect: (Rt8Tab) -> Unit
) {
    Column(
        modifier = Modifier
            .width(88.dp)
            .fillMaxHeight()
            .background(Panel.copy(alpha = 0.92f))
            .border(width = 1.dp, color = Line, shape = RoundedCornerShape(0.dp))
            .padding(vertical = 12.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(6.dp)
    ) {
        Box(
            modifier = Modifier
                .size(42.dp)
                .clip(RoundedCornerShape(9.dp))
                .background(Cyan.copy(alpha = 0.10f))
                .border(1.dp, Cyan.copy(alpha = 0.45f), RoundedCornerShape(9.dp)),
            contentAlignment = Alignment.Center
        ) {
            Text("R8", style = MaterialTheme.typography.titleMedium, color = Cyan)
        }
        Spacer(Modifier.height(10.dp))
        Rt8Tab.entries.forEach { item ->
            val selected = tab == item
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(62.dp)
                    .padding(horizontal = 6.dp)
                    .clip(RoundedCornerShape(8.dp))
                    .background(if (selected) Cyan.copy(alpha = 0.12f) else Color.Transparent)
                    .clickable { onSelect(item) },
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.Center
            ) {
                Icon(
                    item.icon,
                    contentDescription = item.label,
                    tint = if (selected) Cyan else TextMuted,
                    modifier = Modifier.size(21.dp)
                )
                Spacer(Modifier.height(3.dp))
                Text(
                    item.label,
                    style = MaterialTheme.typography.labelSmall.copy(fontSize = 7.sp),
                    color = if (selected) Cyan else TextMuted
                )
            }
        }
        Spacer(Modifier.weight(1f))
        Text(
            if (latencyMs > 0.1) "${latencyMs.toInt()}ms" else "—",
            style = MaterialTheme.typography.labelSmall.copy(fontSize = 7.sp),
            color = TextMuted
        )
        Text(
            "v${BuildConfig.VERSION_NAME}",
            style = MaterialTheme.typography.labelSmall.copy(fontSize = 7.sp),
            color = TextMuted
        )
    }
}

@Composable
private fun Rt8BottomBar(tab: Rt8Tab, onSelect: (Rt8Tab) -> Unit) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(64.dp)
            .background(Panel.copy(alpha = 0.96f))
            .border(1.dp, Line, RoundedCornerShape(0.dp))
            .padding(horizontal = 4.dp, vertical = 4.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Rt8Tab.entries.forEach { item ->
            val selected = tab == item
            Column(
                modifier = Modifier
                    .weight(1f)
                    .fillMaxHeight()
                    .clip(RoundedCornerShape(8.dp))
                    .background(if (selected) Cyan.copy(alpha = 0.12f) else Color.Transparent)
                    .clickable { onSelect(item) },
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.Center
            ) {
                Icon(
                    item.icon,
                    contentDescription = item.label,
                    tint = if (selected) Cyan else TextMuted,
                    modifier = Modifier.size(20.dp)
                )
                Text(
                    item.label,
                    style = MaterialTheme.typography.labelSmall.copy(fontSize = 7.sp),
                    color = if (selected) Cyan else TextMuted
                )
            }
        }
    }
}
