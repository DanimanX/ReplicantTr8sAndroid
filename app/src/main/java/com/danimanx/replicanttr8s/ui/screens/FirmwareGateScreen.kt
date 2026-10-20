package com.danimanx.replicanttr8s.ui.screens

import android.net.Uri
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.FolderOpen
import androidx.compose.material.icons.filled.Memory
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Icon
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.danimanx.replicanttr8s.BuildConfig
import com.danimanx.replicanttr8s.ui.components.Rt8Button
import com.danimanx.replicanttr8s.ui.components.Rt8Panel
import com.danimanx.replicanttr8s.ui.components.TinyTag
import com.danimanx.replicanttr8s.ui.theme.Amber
import com.danimanx.replicanttr8s.ui.theme.BackgroundGradient
import com.danimanx.replicanttr8s.ui.theme.Coral
import com.danimanx.replicanttr8s.ui.theme.Cyan
import com.danimanx.replicanttr8s.ui.theme.Panel
import com.danimanx.replicanttr8s.ui.theme.TextMuted
import com.danimanx.replicanttr8s.ui.theme.TextPrimary
import com.danimanx.replicanttr8s.ui.theme.TextSecondary

/**
 * Shown until the user imports their own firmware image. The native engine is
 * silent and the transport is locked while this screen is visible, so no
 * built-in or preview content can ever be heard.
 */
@Composable
fun FirmwareGateScreen(
    importing: Boolean,
    message: String?,
    onImport: (Uri) -> Unit
) {
    val launcher = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) onImport(uri)
    }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Brush.verticalGradient(BackgroundGradient)),
        contentAlignment = Alignment.Center
    ) {
        Column(
            modifier = Modifier
                .widthIn(max = 560.dp)
                .padding(horizontal = 24.dp, vertical = 28.dp)
                .verticalScroll(rememberScrollState()),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            TinyTag("REPLICANT TR-8S // ANDROID", Cyan)
            Spacer(Modifier.height(18.dp))
            Text(
                "FIRMWARE REQUIRED",
                style = MaterialTheme.typography.headlineMedium,
                color = TextPrimary,
                textAlign = TextAlign.Center
            )
            Spacer(Modifier.height(4.dp))
            Text(
                "THE ENGINE IS LOCKED UNTIL YOU IMPORT YOUR OWN IMAGE",
                style = MaterialTheme.typography.labelMedium.copy(fontSize = 10.sp),
                color = Amber,
                textAlign = TextAlign.Center
            )
            Spacer(Modifier.height(20.dp))

            Rt8Panel(accent = Amber) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Icon(
                        Icons.Filled.Memory,
                        contentDescription = null,
                        tint = Amber,
                        modifier = Modifier.size(24.dp)
                    )
                    Spacer(Modifier.width(10.dp))
                    Column {
                        Text(
                            "NO CONTENT IS BUNDLED",
                            style = MaterialTheme.typography.titleSmall,
                            color = TextPrimary
                        )
                        Text(
                            "BRING-YOUR-OWN FIRMWARE",
                            style = MaterialTheme.typography.labelSmall.copy(fontSize = 8.sp),
                            color = TextMuted
                        )
                    }
                }
                Spacer(Modifier.height(12.dp))
                Text(
                    "ReplicantTR8S does not include any Roland firmware, samples, kits or patterns. " +
                        "The sound engine, transport and sequencer stay disabled until you import a " +
                        "TR-8S v3.00 firmware image that you own.",
                    style = MaterialTheme.typography.bodyMedium,
                    color = TextSecondary
                )
                Spacer(Modifier.height(10.dp))
                Text(
                    "Supported: tr8s_sys_v300.zip · TR8S_UP.bin · rpg42_init_param.bin",
                    style = MaterialTheme.typography.labelSmall,
                    color = TextMuted
                )
                Spacer(Modifier.height(16.dp))
                Rt8Button(
                    text = if (importing) "PARSING FIRMWARE…" else "IMPORT FIRMWARE",
                    icon = Icons.Filled.FolderOpen,
                    accent = Cyan,
                    enabled = !importing,
                    modifier = Modifier.fillMaxWidth(),
                    onClick = {
                        launcher.launch(
                            arrayOf("application/zip", "application/octet-stream", "*/*")
                        )
                    }
                )
                if (importing) {
                    Spacer(Modifier.height(12.dp))
                    LinearProgressIndicator(
                        modifier = Modifier.fillMaxWidth(),
                        color = Cyan,
                        trackColor = Panel
                    )
                }
                if (message != null) {
                    Spacer(Modifier.height(12.dp))
                    Text(
                        message,
                        style = MaterialTheme.typography.bodySmall,
                        color = Coral
                    )
                }
                Spacer(Modifier.height(14.dp))
                Text(
                    "The file is parsed on this device and never uploaded. It is stored in " +
                        "app-private storage and excluded from Android backups. Nothing is " +
                        "downloaded or redistributed.",
                    style = MaterialTheme.typography.labelSmall.copy(fontSize = 8.sp),
                    color = TextMuted
                )
            }

            Spacer(Modifier.height(18.dp))
            Text(
                "v${BuildConfig.VERSION_NAME} · BRING-YOUR-OWN-FIRMWARE",
                style = MaterialTheme.typography.labelSmall.copy(fontSize = 8.sp),
                color = TextMuted,
                textAlign = TextAlign.Center
            )
        }
    }
}

@Composable
fun EngineBootScreen() {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(Brush.verticalGradient(BackgroundGradient)),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center
    ) {
        CircularProgressIndicator(color = Cyan, strokeWidth = 2.dp)
        Spacer(Modifier.height(14.dp))
        Text(
            "INITIALIZING ENGINE",
            style = MaterialTheme.typography.labelMedium,
            color = TextMuted
        )
    }
}
