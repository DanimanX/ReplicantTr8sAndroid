package com.danimanx.replicanttr8s.ui.components

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import com.danimanx.replicanttr8s.ui.theme.Line

@Composable
fun ScopeCanvas(
    samples: List<Float>,
    accent: Color,
    modifier: Modifier = Modifier
) {
    Canvas(modifier.fillMaxSize()) {
        val w = size.width
        val h = size.height
        val mid = h * 0.52f

        for (i in 1..3) {
            val y = h * i / 4f
            drawLine(
                color = Line.copy(alpha = 0.35f),
                start = Offset(0f, y),
                end = Offset(w, y),
                strokeWidth = 1f
            )
        }
        for (i in 1..7) {
            val x = w * i / 8f
            drawLine(
                color = Line.copy(alpha = 0.18f),
                start = Offset(x, 0f),
                end = Offset(x, h),
                strokeWidth = 1f
            )
        }

        if (samples.size < 2) return@Canvas
        val path = Path()
        val fill = Path()
        val n = samples.size
        for (i in 0 until n) {
            val x = w * i / (n - 1f)
            val v = samples[i].coerceIn(-1f, 1f)
            val y = mid - v * h * 0.42f
            if (i == 0) {
                path.moveTo(x, y)
                fill.moveTo(x, mid)
                fill.lineTo(x, y)
            } else {
                path.lineTo(x, y)
                fill.lineTo(x, y)
            }
        }
        fill.lineTo(w, mid)
        fill.close()

        drawPath(
            fill,
            brush = Brush.verticalGradient(
                colors = listOf(accent.copy(alpha = 0.22f), Color.Transparent),
                startY = 0f,
                endY = h
            )
        )
        drawLine(
            color = accent.copy(alpha = 0.20f),
            start = Offset(0f, mid),
            end = Offset(w, mid),
            strokeWidth = 3f
        )
        drawPath(
            path,
            color = accent.copy(alpha = 0.20f),
            style = Stroke(width = 7f, cap = StrokeCap.Round)
        )
        drawPath(
            path,
            color = accent,
            style = Stroke(width = 2f, cap = StrokeCap.Round)
        )
    }
}
