package com.danimanx.replicanttr8s.ui.components

import androidx.compose.animation.animateColorAsState
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.danimanx.replicanttr8s.ui.theme.Ink
import com.danimanx.replicanttr8s.ui.theme.Line
import com.danimanx.replicanttr8s.ui.theme.PanelHigh
import com.danimanx.replicanttr8s.ui.theme.TextMuted
import com.danimanx.replicanttr8s.ui.theme.TextPrimary

@OptIn(ExperimentalFoundationApi::class)
@Composable
fun StepPad(
    step: Int,
    velocity: Int,
    isPlaying: Boolean,
    accent: Color,
    modifier: Modifier = Modifier,
    label: String = (step + 1).toString().padStart(2, '0'),
    height: Dp = 56.dp,
    onClick: () -> Unit,
    onLongClick: () -> Unit
) {
    val active = velocity > 0
    val intensity = (velocity.coerceIn(0, 127) / 127f)
    val background = when {
        !active -> PanelHigh.copy(alpha = 0.76f)
        velocity >= 100 -> accent.copy(alpha = 0.82f)
        velocity >= 70 -> accent.copy(alpha = 0.56f)
        else -> accent.copy(alpha = 0.30f)
    }
    val animatedBackground by animateColorAsState(background, label = "pad-bg")
    val borderColor by animateColorAsState(
        when {
            isPlaying -> Color.White
            active -> accent.copy(alpha = 0.9f)
            else -> Line
        },
        label = "pad-border"
    )

    Box(
        modifier = modifier
            .height(height)
            .clip(RoundedCornerShape(8.dp))
            .background(
                Brush.verticalGradient(
                    listOf(animatedBackground.copy(alpha = if (active) 0.96f else 0.76f), animatedBackground.copy(alpha = 0.62f))
                )
            )
            .border(if (isPlaying) 2.dp else 1.dp, borderColor, RoundedCornerShape(8.dp))
            .semantics {
                contentDescription = "Step ${step + 1} ${if (active) "on velocity $velocity" else "off"}"
            }
            .combinedClickable(onClick = onClick, onLongClick = onLongClick)
            .padding(5.dp),
        contentAlignment = Alignment.Center
    ) {
        Text(
            label,
            style = MaterialTheme.typography.labelMedium.copy(fontSize = 11.sp),
            color = if (active && velocity >= 100) Ink else if (active) TextPrimary else TextMuted,
            textAlign = TextAlign.Center
        )
        if (active) {
            Box(
                Modifier
                    .align(Alignment.BottomCenter)
                    .fillMaxWidth(0.55f)
                    .height(3.dp)
                    .clip(RoundedCornerShape(2.dp))
                    .background(if (velocity >= 100) Ink.copy(alpha = 0.55f) else accent.copy(alpha = 0.8f))
            )
        }
        if (step % 4 == 0) {
            Box(
                Modifier
                    .align(Alignment.TopStart)
                    .padding(4.dp)
                    .width(8.dp)
                    .height(3.dp)
                    .clip(RoundedCornerShape(2.dp))
                    .background(accent.copy(alpha = if (active) 0.9f else 0.5f))
            )
        }
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
fun AccentPad(
    step: Int,
    velocity: Int,
    isPlaying: Boolean,
    modifier: Modifier = Modifier,
    height: Dp = 32.dp,
    onClick: () -> Unit
) {
    val active = velocity > 85
    Box(
        modifier = modifier
            .height(height)
            .clip(RoundedCornerShape(6.dp))
            .background(if (active) Color(0xFFFFB45C).copy(alpha = 0.75f) else PanelHigh.copy(alpha = 0.7f))
            .border(
                if (isPlaying) 1.6.dp else 1.dp,
                if (isPlaying) Color.White else if (active) Color(0xFFFFB45C) else Line,
                RoundedCornerShape(6.dp)
            )
            .combinedClickable(onClick = onClick, onLongClick = onClick),
        contentAlignment = Alignment.Center
    ) {
        Text(
            if (active) "▲" else "·",
            style = MaterialTheme.typography.labelSmall.copy(fontSize = 9.sp),
            color = if (active) Ink else TextMuted
        )
    }
}
