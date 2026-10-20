package com.danimanx.replicanttr8s.ui.screens

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
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
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.GridItemSpan
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.FolderOpen
import androidx.compose.material.icons.filled.Memory
import androidx.compose.material.icons.filled.Search
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.danimanx.replicanttr8s.MainViewModel
import com.danimanx.replicanttr8s.audio.EngineController
import com.danimanx.replicanttr8s.model.EngineCatalog
import com.danimanx.replicanttr8s.model.EngineUiState
import com.danimanx.replicanttr8s.ui.components.Rt8Button
import com.danimanx.replicanttr8s.ui.components.Rt8Panel
import com.danimanx.replicanttr8s.ui.components.SectionLabel
import com.danimanx.replicanttr8s.ui.components.TinyTag
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

private enum class LibraryMode(val label: String) {
    KITS("KITS"),
    PATTERNS("PATTERNS"),
    TONES("TONES")
}

@Composable
fun KitScreen(
    state: EngineUiState,
    catalog: EngineCatalog,
    engine: EngineController,
    viewModel: MainViewModel
) {
    var mode by rememberSaveable { mutableStateOf(LibraryMode.KITS) }
    var query by rememberSaveable { mutableStateOf("") }
    var confirmClear by remember { mutableStateOf(false) }

    val importFirmware = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) viewModel.importFirmware(uri)
    }

    val filtered = remember(mode, query, catalog) {
        val needle = query.trim().lowercase()
        when (mode) {
            LibraryMode.KITS -> catalog.kitNames.indices.filter { index ->
                needle.isEmpty() || catalog.kitNames[index].lowercase().contains(needle) ||
                    "%03d".format(index + 1).contains(needle)
            }
            LibraryMode.PATTERNS -> catalog.patternNames.indices.filter { index ->
                needle.isEmpty() || catalog.patternNames[index].lowercase().contains(needle) ||
                    catalog.patternCodes.getOrElse(index) { "" }.contains(needle)
            }
            LibraryMode.TONES -> catalog.toneNames.indices.filter { index ->
                needle.isEmpty() || catalog.toneNames[index].lowercase().contains(needle) ||
                    index.toString() == needle
            }
        }
    }

    LazyVerticalGrid(
        columns = GridCells.Adaptive(152.dp),
        modifier = Modifier.fillMaxSize(),
        contentPadding = PaddingValues(horizontal = 12.dp, vertical = 10.dp),
        horizontalArrangement = Arrangement.spacedBy(6.dp),
        verticalArrangement = Arrangement.spacedBy(6.dp)
    ) {
        item(span = { GridItemSpan(maxLineSpan) }) {
            CurrentKitCard(state, engine, catalog)
        }
        item(span = { GridItemSpan(maxLineSpan) }) {
            FirmwareCard(
                state = state,
                onImport = { importFirmware.launch(arrayOf("application/zip", "application/octet-stream", "*/*")) },
                onClear = { confirmClear = true }
            )
        }
        item(span = { GridItemSpan(maxLineSpan) }) {
            LibraryControls(
                mode = mode,
                onMode = { mode = it },
                query = query,
                onQuery = { query = it },
                count = filtered.size
            )
        }
        items(filtered) { index ->
            when (mode) {
                LibraryMode.KITS -> KitCard(
                    index = index,
                    name = catalog.kitNames.getOrElse(index) { "INIT KIT ${index + 1}" },
                    selected = state.kitIndex == index,
                    tonePreview = catalog.kitToneNames(index).take(4)
                ) { engine.loadKit(index) }
                LibraryMode.PATTERNS -> PatternCard(
                    index = index,
                    code = catalog.patternCodes.getOrElse(index) { "" },
                    name = catalog.patternNames.getOrElse(index) { "INIT PTN" },
                    bpm = if (index == state.patternIndex) state.tempo.toInt() else null,
                    selected = state.patternIndex == index
                ) { engine.loadPattern(index, true) }
                LibraryMode.TONES -> ToneCard(
                    index = index,
                    name = catalog.toneNames.getOrElse(index) { "Tone $index" },
                    channelName = state.channels.getOrNull(state.selectedChannel)?.name ?: "BD",
                    selected = state.channels.getOrNull(state.selectedChannel)?.toneId == index,
                    color = channelColor(state.selectedChannel)
                ) { engine.setTone(state.selectedChannel, index) }
            }
        }
        if (filtered.isEmpty()) {
            item(span = { GridItemSpan(maxLineSpan) }) {
                Text(
                    if (!catalog.ready) "Loading 128 kits, 128 patterns and 522 tones…"
                    else "No matches. Try a different search.",
                    style = MaterialTheme.typography.bodyMedium,
                    color = TextMuted,
                    modifier = Modifier.padding(20.dp)
                )
            }
        }
        item(span = { GridItemSpan(maxLineSpan) }) {
            Spacer(Modifier.height(16.dp))
        }
    }

    if (confirmClear) {
        androidx.compose.material3.AlertDialog(
            onDismissRequest = { confirmClear = false },
            title = { Text("Clear imported firmware?", color = TextPrimary) },
            text = {
                Text(
                    "The engine will lock and stay silent until you import firmware again. Your firmware file will be deleted from app storage.",
                    color = TextSecondary
                )
            },
            confirmButton = {
                Text(
                    "CLEAR",
                    color = Coral,
                    style = MaterialTheme.typography.labelLarge,
                    modifier = Modifier
                        .clickable {
                            confirmClear = false
                            viewModel.clearFirmware()
                        }
                        .padding(10.dp)
                )
            },
            dismissButton = {
                Text(
                    "CANCEL",
                    color = TextMuted,
                    style = MaterialTheme.typography.labelLarge,
                    modifier = Modifier.clickable { confirmClear = false }.padding(10.dp)
                )
            },
            containerColor = PanelHigh
        )
    }
}

@Composable
private fun CurrentKitCard(state: EngineUiState, engine: EngineController, catalog: EngineCatalog) {
    Rt8Panel(accent = Cyan) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text(
                    "CURRENT KIT",
                    style = MaterialTheme.typography.labelSmall,
                    color = Cyan
                )
                Text(
                    "${(state.kitIndex + 1).toString().padStart(3, '0')}  ${state.kitName.uppercase()}",
                    style = MaterialTheme.typography.headlineMedium,
                    color = TextPrimary,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
                Text(
                    "PTN ${state.patternCode} ${state.patternName.uppercase()} · ${state.tempo.toInt()} BPM · VAR ${state.variationLetter}",
                    style = MaterialTheme.typography.labelMedium,
                    color = TextSecondary,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
            }
            TinyTag(if (state.firmwareLoaded) "FW V3.00" else "NO FW", if (state.firmwareLoaded) Green else Amber)
        }
        Spacer(Modifier.height(10.dp))
        Row(
            modifier = Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
            horizontalArrangement = Arrangement.spacedBy(5.dp)
        ) {
            state.channels.forEachIndexed { index, channel ->
                val color = channelColor(index)
                Column(
                    modifier = Modifier
                        .width(92.dp)
                        .clip(RoundedCornerShape(6.dp))
                        .background(color.copy(alpha = 0.10f))
                        .border(1.dp, color.copy(alpha = 0.28f), RoundedCornerShape(6.dp))
                        .clickable { engine.selectChannel(index) }
                        .padding(6.dp)
                ) {
                    Text(channel.name, style = MaterialTheme.typography.labelMedium, color = color)
                    Text(
                        channel.toneName,
                        style = MaterialTheme.typography.labelSmall.copy(fontSize = 7.sp),
                        color = TextMuted,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis
                    )
                }
            }
        }
    }
}

@Composable
private fun FirmwareCard(
    state: EngineUiState,
    onImport: () -> Unit,
    onClear: () -> Unit
) {
    Rt8Panel(accent = if (state.firmwareLoaded) Green else Amber) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Icon(
                Icons.Filled.Memory,
                contentDescription = null,
                tint = if (state.firmwareLoaded) Green else Amber,
                modifier = Modifier.size(20.dp)
            )
            Spacer(Modifier.width(9.dp))
            Column(Modifier.weight(1f)) {
                Text(
                    if (state.firmwareLoaded) "TR-8S v3.00 FIRMWARE LOADED" else "FIRMWARE REQUIRED // ENGINE LOCKED",
                    style = MaterialTheme.typography.titleSmall,
                    color = if (state.firmwareLoaded) Green else Amber
                )
                Text(
                    if (state.firmwareLoaded) {
                        "${state.firmwareName} · 128 kits · 128 patterns · 522 tones"
                    } else {
                        "Import tr8s_sys_v300.zip or TR8S_UP.bin to unlock the factory kit, pattern and tone tables. No proprietary data ships with this app."
                    },
                    style = MaterialTheme.typography.bodySmall,
                    color = TextMuted
                )
            }
        }
        Spacer(Modifier.height(9.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(7.dp)) {
            Rt8Button(
                text = if (state.firmwareLoaded) "REPLACE FIRMWARE" else "IMPORT FIRMWARE",
                icon = Icons.Filled.FolderOpen,
                accent = if (state.firmwareLoaded) Green else Amber,
                modifier = Modifier.weight(1f),
                onClick = onImport
            )
            if (state.firmwareLoaded) {
                Rt8Button(
                    text = "CLEAR",
                    icon = Icons.Filled.Delete,
                    accent = Coral,
                    modifier = Modifier.width(110.dp),
                    onClick = onClear
                )
            }
        }
        Spacer(Modifier.height(6.dp))
        Text(
            "Legal: the app parses a user-supplied firmware image at runtime. It does not distribute, embed or download any Roland firmware.",
            style = MaterialTheme.typography.labelSmall.copy(fontSize = 7.sp),
            color = TextMuted
        )
    }
}

@Composable
private fun LibraryControls(
    mode: LibraryMode,
    onMode: (LibraryMode) -> Unit,
    query: String,
    onQuery: (String) -> Unit,
    count: Int
) {
    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        SectionLabel("LIBRARY // ${count} RESULTS", accent = TextSecondary) {
            Text(
                "TAP TO LOAD",
                style = MaterialTheme.typography.labelSmall.copy(fontSize = 8.sp),
                color = TextMuted
            )
        }
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            LibraryMode.entries.forEach { item ->
                Rt8Button(
                    text = item.label,
                    accent = Cyan,
                    selected = mode == item,
                    modifier = Modifier.weight(1f),
                    onClick = { onMode(item) }
                )
            }
        }
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .height(42.dp)
                .clip(RoundedCornerShape(8.dp))
                .background(PanelHigh)
                .border(1.dp, Line, RoundedCornerShape(8.dp))
                .padding(horizontal = 10.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Icon(Icons.Filled.Search, contentDescription = null, tint = TextMuted, modifier = Modifier.size(16.dp))
            Spacer(Modifier.width(8.dp))
            BasicTextField(
                value = query,
                onValueChange = onQuery,
                singleLine = true,
                textStyle = TextStyle(
                    color = TextPrimary,
                    fontSize = 13.sp,
                    fontFamily = MaterialTheme.typography.bodyMedium.fontFamily
                ),
                cursorBrush = SolidColor(Cyan),
                modifier = Modifier.weight(1f),
                decorationBox = { inner ->
                    if (query.isEmpty()) {
                        Text(
                            "Search kits, patterns or tones",
                            style = MaterialTheme.typography.bodySmall,
                            color = TextMuted
                        )
                    }
                    inner()
                }
            )
            if (query.isNotEmpty()) {
                Text(
                    "CLEAR",
                    style = MaterialTheme.typography.labelSmall,
                    color = Cyan,
                    modifier = Modifier.clickable { onQuery("") }.padding(4.dp)
                )
            }
        }
    }
}

@Composable
private fun KitCard(
    index: Int,
    name: String,
    selected: Boolean,
    tonePreview: List<String>,
    onClick: () -> Unit
) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .height(86.dp)
            .clip(RoundedCornerShape(8.dp))
            .background(if (selected) Cyan.copy(alpha = 0.12f) else PanelHigh.copy(alpha = 0.8f))
            .border(if (selected) 1.5.dp else 1.dp, if (selected) Cyan else Line, RoundedCornerShape(8.dp))
            .clickable(onClick = onClick)
            .padding(9.dp),
        verticalArrangement = Arrangement.spacedBy(2.dp)
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(
                (index + 1).toString().padStart(3, '0'),
                style = MaterialTheme.typography.labelSmall,
                color = if (selected) Cyan else TextMuted
            )
            Spacer(Modifier.weight(1f))
            if (selected) TinyTag("LOADED", Cyan)
        }
        Text(
            name.uppercase(),
            style = MaterialTheme.typography.titleMedium.copy(fontSize = 13.sp),
            color = TextPrimary,
            maxLines = 2,
            overflow = TextOverflow.Ellipsis
        )
        Text(
            tonePreview.joinToString(" · "),
            style = MaterialTheme.typography.labelSmall.copy(fontSize = 7.sp),
            color = TextMuted,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis
        )
    }
}

@Composable
private fun PatternCard(
    index: Int,
    code: String,
    name: String,
    bpm: Int?,
    selected: Boolean,
    onClick: () -> Unit
) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .height(86.dp)
            .clip(RoundedCornerShape(8.dp))
            .background(if (selected) Amber.copy(alpha = 0.12f) else PanelHigh.copy(alpha = 0.8f))
            .border(if (selected) 1.5.dp else 1.dp, if (selected) Amber else Line, RoundedCornerShape(8.dp))
            .clickable(onClick = onClick)
            .padding(9.dp),
        verticalArrangement = Arrangement.spacedBy(3.dp)
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(
                code.ifEmpty { "%d-%02d".format(index / 16 + 1, index % 16 + 1) },
                style = MaterialTheme.typography.labelMedium,
                color = if (selected) Amber else Cyan
            )
            Spacer(Modifier.weight(1f))
            if (bpm != null) TinyTag("$bpm BPM", Amber)
        }
        Text(
            name.uppercase(),
            style = MaterialTheme.typography.titleMedium.copy(fontSize = 13.sp),
            color = TextPrimary,
            maxLines = 2,
            overflow = TextOverflow.Ellipsis
        )
        Text(
            "PATTERN ${index + 1}",
            style = MaterialTheme.typography.labelSmall.copy(fontSize = 7.sp),
            color = TextMuted
        )
    }
}

@Composable
private fun ToneCard(
    index: Int,
    name: String,
    channelName: String,
    selected: Boolean,
    color: Color,
    onClick: () -> Unit
) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .height(72.dp)
            .clip(RoundedCornerShape(8.dp))
            .background(if (selected) color.copy(alpha = 0.14f) else PanelHigh.copy(alpha = 0.8f))
            .border(if (selected) 1.5.dp else 1.dp, if (selected) color else Line, RoundedCornerShape(8.dp))
            .clickable(onClick = onClick)
            .padding(8.dp),
        verticalArrangement = Arrangement.spacedBy(2.dp)
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(
                index.toString().padStart(3, '0'),
                style = MaterialTheme.typography.labelSmall,
                color = if (selected) color else TextMuted
            )
            Spacer(Modifier.weight(1f))
            TinyTag("→ $channelName", color)
        }
        Text(
            name.uppercase(),
            style = MaterialTheme.typography.titleMedium.copy(fontSize = 12.sp),
            color = TextPrimary,
            maxLines = 2,
            overflow = TextOverflow.Ellipsis
        )
    }
}
