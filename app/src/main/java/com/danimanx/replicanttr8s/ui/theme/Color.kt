package com.danimanx.replicanttr8s.ui.theme

import androidx.compose.ui.graphics.Color

val Ink = Color(0xFF05080A)
val InkDeep = Color(0xFF030506)
val Panel = Color(0xFF0B1115)
val PanelHigh = Color(0xFF101A1F)
val PanelSoft = Color(0xFF0E171C)
val Line = Color(0xFF1E2C33)
val LineBright = Color(0xFF2A3D46)
val TextPrimary = Color(0xFFE7F4F1)
val TextSecondary = Color(0xFF9AB0B4)
val TextMuted = Color(0xFF60777C)

val Cyan = Color(0xFF2FE6D0)
val Amber = Color(0xFFFFB45C)
val Coral = Color(0xFFFF5A6E)
val Green = Color(0xFF43E08A)
val Violet = Color(0xFFA78BFA)
val Magenta = Color(0xFFFF4FA3)
val Blue = Color(0xFF4EA8FF)
val Yellow = Color(0xFFF4D03F)

val ChannelPalette = listOf(
    Color(0xFFFF4D5E), // BD
    Color(0xFFFF9F43), // SD
    Color(0xFFF4D03F), // LT
    Color(0xFFB6E64B), // MT
    Color(0xFF43E08A), // HT
    Color(0xFFA78BFA), // RS
    Color(0xFFFF4FA3), // HC
    Color(0xFF2FE6D0), // CH
    Color(0xFF4EA8FF), // OH
    Color(0xFF35D0A5), // CC
    Color(0xFF8FE3E0)  // RC
)

fun channelColor(index: Int): Color = ChannelPalette[index.coerceIn(0, ChannelPalette.lastIndex)]

val BackgroundGradient = listOf(
    Color(0xFF0A1013),
    Color(0xFF060A0C),
    Color(0xFF030506)
)
