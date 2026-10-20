package com.danimanx.replicanttr8s.ui.components

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.danimanx.replicanttr8s.ui.theme.Line
import com.danimanx.replicanttr8s.ui.theme.LineBright
import com.danimanx.replicanttr8s.ui.theme.PanelHigh
import com.danimanx.replicanttr8s.ui.theme.TextPrimary
import com.danimanx.replicanttr8s.ui.theme.TextSecondary
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.sin

/**
 * Hardware-style rotary control. Drag vertically to change, long-press to
 * return to the centre/default position. The arc and ticks are drawn directly
 * so the control keeps its look at any screen density.
 */
@Composable
fun RotaryKnob(
    value: Float,
    onValueChange: (Float) -> Unit,
    label: String,
    valueText: String,
    accent: Color,
    modifier: Modifier = Modifier,
    size: Dp = 62.dp,
    defaultValue: Float = 0.5f,
    enabled: Boolean = true,
    showLabel: Boolean = true
) {
    var local by remember { mutableFloatStateOf(value) }
    var dragging by remember { mutableStateOf(false) }
    LaunchedEffect(value) {
        if (!dragging) local = value
    }

    Column(
        modifier = modifier,
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Canvas(
            modifier = Modifier
                .size(size)
                .semantics {
                    contentDescription = "$label $valueText"
                }
                .pointerInput(enabled) {
                    if (!enabled) return@pointerInput
                    awaitEachGesture {
                        val down = awaitFirstDown(requireUnconsumed = false)
                        dragging = true
                        var lastY = down.position.y
                        var total = 0f
                        var longPressFired = false
                        var pressed = true
                        while (pressed) {
                            val event = awaitPointerEvent()
                            val change = event.changes.firstOrNull { it.id == down.id } ?: break
                            pressed = change.pressed
                            if (!pressed) break
                            val dy = change.position.y - lastY
                            lastY = change.position.y
                            total += abs(dy)
                            val heldMs = change.uptimeMillis - down.uptimeMillis
                            if (!longPressFired && heldMs > 700L && total < 10f) {
                                longPressFired = true
                                local = defaultValue
                                onValueChange(defaultValue)
                                change.consume()
                            } else if (!longPressFired && abs(dy) > 0.05f) {
                                local = (local - dy / 190f).coerceIn(0f, 1f)
                                onValueChange(local)
                                change.consume()
                            }
                        }
                        dragging = false
                    }
                }
        ) {
            val stroke = size.toPx() * 0.075f
            val radius = (size.toPx() - stroke * 2f) / 2f
            val center = Offset(this.size.width / 2f, this.size.height / 2f)
            val topLeft = Offset(center.x - radius, center.y - radius)
            val arcSize = Size(radius * 2f, radius * 2f)
            val start = 135f
            val sweep = 270f

            drawArc(
                color = Line,
                startAngle = start,
                sweepAngle = sweep,
                useCenter = false,
                topLeft = topLeft,
                size = arcSize,
                style = Stroke(width = stroke, cap = StrokeCap.Round)
            )
            drawArc(
                color = accent.copy(alpha = 0.18f),
                startAngle = start,
                sweepAngle = sweep * local,
                useCenter = false,
                topLeft = topLeft,
                size = arcSize,
                style = Stroke(width = stroke * 2.4f, cap = StrokeCap.Round)
            )
            drawArc(
                color = if (enabled) accent else accent.copy(alpha = 0.4f),
                startAngle = start,
                sweepAngle = sweep * local,
                useCenter = false,
                topLeft = topLeft,
                size = arcSize,
                style = Stroke(width = stroke, cap = StrokeCap.Round)
            )

            for (i in 0..10) {
                val angle = ((start + sweep * i / 10f) * PI / 180.0).toFloat()
                val inner = radius * if (i % 5 == 0) 0.74f else 0.80f
                val outer = radius * 0.90f
                drawLine(
                    color = if (i <= (local * 10f).toInt()) accent.copy(alpha = 0.58f) else LineBright.copy(alpha = 0.55f),
                    start = Offset(center.x + cos(angle) * inner, center.y + sin(angle) * inner),
                    end = Offset(center.x + cos(angle) * outer, center.y + sin(angle) * outer),
                    strokeWidth = if (i % 5 == 0) stroke * 0.55f else stroke * 0.35f,
                    cap = StrokeCap.Round
                )
            }

            drawCircle(color = PanelHigh, radius = radius * 0.56f, center = center)
            drawCircle(
                color = LineBright.copy(alpha = 0.7f),
                radius = radius * 0.56f,
                center = center,
                style = Stroke(width = 1.dp.toPx())
            )

            val angle = ((start + sweep * local) * PI / 180.0).toFloat()
            drawLine(
                color = if (enabled) TextPrimary else TextSecondary,
                start = center,
                end = Offset(
                    center.x + cos(angle) * radius * 0.68f,
                    center.y + sin(angle) * radius * 0.68f
                ),
                strokeWidth = stroke * 0.75f,
                cap = StrokeCap.Round
            )
            drawCircle(color = accent, radius = stroke * 0.42f, center = center)
        }

        if (showLabel) {
            Spacer(Modifier.height(3.dp))
            Text(
                label.uppercase(),
                style = MaterialTheme.typography.labelSmall.copy(fontSize = 8.sp),
                color = TextSecondary,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
            Text(
                valueText,
                style = MaterialTheme.typography.labelSmall.copy(fontSize = 9.sp),
                color = TextPrimary,
                maxLines = 1
            )
        }
    }
}

@Composable
fun MiniKnob(
    value: Float,
    onValueChange: (Float) -> Unit,
    label: String,
    valueText: String,
    accent: Color,
    modifier: Modifier = Modifier,
    defaultValue: Float = 0.5f
) {
    RotaryKnob(
        value = value,
        onValueChange = onValueChange,
        label = label,
        valueText = valueText,
        accent = accent,
        modifier = modifier.width(46.dp),
        size = 38.dp,
        defaultValue = defaultValue,
        showLabel = true
    )
}
