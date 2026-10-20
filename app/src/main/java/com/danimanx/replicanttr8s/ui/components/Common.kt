package com.danimanx.replicanttr8s.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.danimanx.replicanttr8s.ui.theme.Cyan
import com.danimanx.replicanttr8s.ui.theme.Ink
import com.danimanx.replicanttr8s.ui.theme.Line
import com.danimanx.replicanttr8s.ui.theme.Panel
import com.danimanx.replicanttr8s.ui.theme.PanelHigh
import com.danimanx.replicanttr8s.ui.theme.TextMuted
import com.danimanx.replicanttr8s.ui.theme.TextPrimary
import com.danimanx.replicanttr8s.ui.theme.TextSecondary

@Composable
fun Rt8Panel(
    modifier: Modifier = Modifier,
    accent: Color? = null,
    contentPadding: PaddingValues = PaddingValues(12.dp),
    content: @Composable ColumnScope.() -> Unit
) {
    Column(
        modifier = modifier
            .clip(RoundedCornerShape(12.dp))
            .background(
                Brush.verticalGradient(
                    listOf(PanelHigh.copy(alpha = 0.92f), Panel.copy(alpha = 0.96f))
                )
            )
            .border(1.dp, Line.copy(alpha = 0.9f), RoundedCornerShape(12.dp))
            .drawBehind {
                if (accent != null) {
                    drawLine(
                        color = accent.copy(alpha = 0.9f),
                        start = Offset(14.dp.toPx(), 0f),
                        end = Offset(size.width - 14.dp.toPx(), 0f),
                        strokeWidth = 2.dp.toPx()
                    )
                }
            }
            .padding(contentPadding),
        content = content
    )
}

@Composable
fun SectionLabel(
    text: String,
    modifier: Modifier = Modifier,
    accent: Color = TextSecondary,
    trailing: (@Composable () -> Unit)? = null
) {
    Row(
        modifier = modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(
            text.uppercase(),
            style = MaterialTheme.typography.labelSmall,
            color = accent,
            maxLines = 1
        )
        Spacer(Modifier.width(10.dp))
        Box(
            Modifier
                .weight(1f)
                .height(1.dp)
                .background(Line)
        )
        if (trailing != null) {
            Spacer(Modifier.width(10.dp))
            trailing()
        }
    }
}

@Composable
fun StatusChip(
    label: String,
    value: String? = null,
    accent: Color = Cyan,
    active: Boolean = true,
    modifier: Modifier = Modifier
) {
    Row(
        modifier = modifier
            .clip(RoundedCornerShape(50))
            .background(if (active) accent.copy(alpha = 0.10f) else PanelHigh)
            .border(1.dp, if (active) accent.copy(alpha = 0.35f) else Line, RoundedCornerShape(50))
            .padding(horizontal = 9.dp, vertical = 5.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(6.dp)
    ) {
        Box(
            Modifier
                .size(5.dp)
                .clip(RoundedCornerShape(50))
                .background(if (active) accent else TextMuted.copy(alpha = 0.5f))
        )
        Text(
            label.uppercase(),
            style = MaterialTheme.typography.labelSmall,
            color = if (active) accent else TextMuted,
            maxLines = 1
        )
        if (!value.isNullOrBlank()) {
            Text(
                value.uppercase(),
                style = MaterialTheme.typography.labelSmall,
                color = TextPrimary,
                maxLines = 1
            )
        }
    }
}

@Composable
fun LedDot(active: Boolean, accent: Color, size: Dp = 6.dp, modifier: Modifier = Modifier) {
    Box(
        modifier
            .size(size)
            .clip(RoundedCornerShape(50))
            .background(if (active) accent else Line)
    )
}

@Composable
fun Rt8Button(
    text: String,
    modifier: Modifier = Modifier,
    accent: Color = Cyan,
    selected: Boolean = false,
    enabled: Boolean = true,
    icon: ImageVector? = null,
    onClick: () -> Unit
) {
    val bg = if (selected) accent else PanelHigh
    val fg = if (selected) Ink else if (enabled) TextPrimary else TextMuted
    Row(
        modifier = modifier
            .height(40.dp)
            .clip(RoundedCornerShape(8.dp))
            .background(if (enabled) bg else PanelHigh.copy(alpha = 0.5f))
            .border(
                1.dp,
                if (selected) accent else Line,
                RoundedCornerShape(8.dp)
            )
            .clickable(enabled = enabled, onClick = onClick)
            .padding(horizontal = 12.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.Center
    ) {
        if (icon != null) {
            Icon(icon, contentDescription = null, tint = fg, modifier = Modifier.size(16.dp))
            Spacer(Modifier.width(7.dp))
        }
        Text(
            text.uppercase(),
            style = MaterialTheme.typography.labelMedium,
            color = fg,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis
        )
    }
}

@Composable
fun TinyTag(text: String, color: Color = TextSecondary, modifier: Modifier = Modifier) {
    Text(
        text.uppercase(),
        modifier = modifier
            .clip(RoundedCornerShape(3.dp))
            .background(color.copy(alpha = 0.10f))
            .border(1.dp, color.copy(alpha = 0.25f), RoundedCornerShape(3.dp))
            .padding(horizontal = 5.dp, vertical = 2.dp),
        style = MaterialTheme.typography.labelSmall.copy(fontSize = 8.sp),
        color = color,
        maxLines = 1
    )
}
