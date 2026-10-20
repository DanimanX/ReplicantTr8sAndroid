package com.danimanx.replicanttr8s.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.LocalCafe
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.danimanx.replicanttr8s.ReplicantLinks
import com.danimanx.replicanttr8s.ui.theme.Amber

/**
 * "Buy me a coffee" control, matching the donation button in the desktop
 * ReplicantTR8S header. Opens the project's PayPal page in the browser.
 */
@Composable
fun DonateButton(
    modifier: Modifier = Modifier,
    compact: Boolean = false
) {
    val uriHandler = LocalUriHandler.current
    val action = {
        runCatching { uriHandler.openUri(ReplicantLinks.PAYPAL_DONATE) }
        Unit
    }
    if (compact) {
        Row(
            modifier = modifier
                .size(46.dp)
                .clip(RoundedCornerShape(50))
                .background(Amber.copy(alpha = 0.10f))
                .border(1.dp, Amber.copy(alpha = 0.42f), RoundedCornerShape(50))
                .semantics { contentDescription = "Donate a coffee via PayPal" }
                .clickable(onClick = action),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.Center
        ) {
            Icon(
                Icons.Filled.LocalCafe,
                contentDescription = null,
                tint = Amber,
                modifier = Modifier.size(20.dp)
            )
        }
    } else {
        Row(
            modifier = modifier
                .height(36.dp)
                .clip(RoundedCornerShape(7.dp))
                .background(Amber.copy(alpha = 0.12f))
                .border(1.dp, Amber.copy(alpha = 0.45f), RoundedCornerShape(7.dp))
                .semantics { contentDescription = "Donate a coffee via PayPal" }
                .clickable(onClick = action)
                .padding(horizontal = 12.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.Center
        ) {
            Icon(
                Icons.Filled.LocalCafe,
                contentDescription = null,
                tint = Amber,
                modifier = Modifier.size(17.dp)
            )
            Spacer(Modifier.width(7.dp))
            Text(
                "DONATE A COFFEE",
                style = MaterialTheme.typography.labelMedium.copy(fontSize = 9.sp),
                color = Amber,
                maxLines = 1
            )
        }
    }
}
