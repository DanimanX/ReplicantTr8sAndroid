package com.danimanx.replicanttr8s.ui.theme

import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Shapes
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.unit.dp

private val Rt8Colors = darkColorScheme(
    primary = Cyan,
    onPrimary = Ink,
    secondary = Amber,
    onSecondary = Ink,
    tertiary = Coral,
    onTertiary = Ink,
    background = Ink,
    onBackground = TextPrimary,
    surface = Panel,
    onSurface = TextPrimary,
    surfaceVariant = PanelHigh,
    onSurfaceVariant = TextSecondary,
    outline = Line,
    outlineVariant = LineBright,
    error = Coral,
    onError = Ink
)

private val Rt8Shapes = Shapes(
    extraSmall = RoundedCornerShape(4.dp),
    small = RoundedCornerShape(7.dp),
    medium = RoundedCornerShape(12.dp),
    large = RoundedCornerShape(18.dp),
    extraLarge = RoundedCornerShape(24.dp)
)

@Composable
fun ReplicantTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = Rt8Colors,
        typography = Rt8Typography,
        shapes = Rt8Shapes,
        content = content
    )
}
