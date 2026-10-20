package com.danimanx.replicanttr8s.ui.components

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import com.danimanx.replicanttr8s.ui.theme.Amber
import com.danimanx.replicanttr8s.ui.theme.Coral
import com.danimanx.replicanttr8s.ui.theme.Green
import com.danimanx.replicanttr8s.ui.theme.Line
import com.danimanx.replicanttr8s.ui.theme.LineBright
import kotlin.math.max

@Composable
fun VerticalFader(
    value: Float,
    onValueChange: (Float) -> Unit,
    accent: Color,
    modifier: Modifier = Modifier
) {
    var heightPx by remember { mutableFloatStateOf(1f) }
    Box(
        modifier = modifier
            .onSizeChanged { heightPx = it.height.toFloat().coerceAtLeast(1f) }
            .pointerInput(Unit) {
                fun update(y: Float) {
                    onValueChange((1f - y / heightPx).coerceIn(0f, 1f))
                }
                detectDragGestures(
                    onDragStart = { offset -> update(offset.y) },
                    onDrag = { change, _ ->
                        change.consume()
                        update(change.position.y)
                    }
                )
            }
    ) {
        Canvas(Modifier.fillMaxSize()) {
            val cx = size.width / 2f
            val top = 8.dp.toPx()
            val bottom = size.height - 8.dp.toPx()
            val trackW = 7.dp.toPx()
            val thumbY = bottom - (bottom - top) * value.coerceIn(0f, 1f)
            val radius = CornerRadius(trackW / 2f, trackW / 2f)

            drawRoundRect(
                color = Line,
                topLeft = Offset(cx - trackW / 2f, top),
                size = Size(trackW, bottom - top),
                cornerRadius = radius
            )
            drawRoundRect(
                color = accent.copy(alpha = 0.36f),
                topLeft = Offset(cx - trackW / 2f, thumbY),
                size = Size(trackW, bottom - thumbY),
                cornerRadius = radius
            )

            for (i in 0..8) {
                val y = top + (bottom - top) * i / 8f
                val tickW = if (i % 2 == 0) 10.dp.toPx() else 6.dp.toPx()
                drawLine(
                    color = LineBright.copy(alpha = 0.55f),
                    start = Offset(cx + 12.dp.toPx(), y),
                    end = Offset(cx + 12.dp.toPx() + tickW, y),
                    strokeWidth = 1.dp.toPx()
                )
            }

            drawLine(
                color = accent,
                start = Offset(cx - 14.dp.toPx(), thumbY),
                end = Offset(cx + 14.dp.toPx(), thumbY),
                strokeWidth = 3.dp.toPx(),
                cap = StrokeCap.Round
            )
            drawCircle(color = accent, radius = 3.dp.toPx(), center = Offset(cx, thumbY))
        }
    }
}

@Composable
fun LevelMeter(
    value: Float,
    modifier: Modifier = Modifier,
    segments: Int = 18
) {
    Canvas(modifier.fillMaxSize()) {
        val gap = 2.dp.toPx()
        val n = max(4, segments)
        val segH = (size.height - gap * (n - 1)) / n
        val lit = (value.coerceIn(0f, 1f) * n).toInt()
        for (i in 0 until n) {
            val y = size.height - (i + 1) * segH - i * gap
            val color = when {
                i >= n * 0.86f -> Coral
                i >= n * 0.66f -> Amber
                else -> Green
            }
            drawRoundRect(
                color = if (i < lit) color else Line.copy(alpha = 0.55f),
                topLeft = Offset(0f, y),
                size = Size(size.width, segH.coerceAtLeast(1f)),
                cornerRadius = CornerRadius(1.dp.toPx(), 1.dp.toPx())
            )
        }
    }
}

@Composable
fun HorizontalMeter(value: Float, modifier: Modifier = Modifier) {
    Canvas(modifier) {
        val h = size.height
        drawRoundRect(
            color = Line.copy(alpha = 0.8f),
            size = size,
            cornerRadius = CornerRadius(h / 2f, h / 2f)
        )
        val fill = size.width * value.coerceIn(0f, 1f)
        if (fill > 0f) {
            drawRoundRect(
                brush = Brush.horizontalGradient(listOf(Green, Amber, Coral)),
                size = Size(fill, h),
                cornerRadius = CornerRadius(h / 2f, h / 2f)
            )
        }
    }
}

@Composable
fun LevelBar(value: Float, color: Color, modifier: Modifier = Modifier) {
    Canvas(modifier) {
        drawRoundRect(
            color = Line.copy(alpha = 0.7f),
            size = size,
            cornerRadius = CornerRadius(size.height / 2f, size.height / 2f)
        )
        drawRoundRect(
            color = color,
            size = Size(size.width * value.coerceIn(0f, 1f), size.height),
            cornerRadius = CornerRadius(size.height / 2f, size.height / 2f)
        )
    }
}
