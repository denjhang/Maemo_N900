package com.hexadecinull.qrscan.ui.screen

import android.os.Build
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.filled.Brightness6
import androidx.compose.material.icons.filled.ColorLens
import androidx.compose.material.icons.filled.History
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.Vibration
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.ListItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import com.hexadecinull.qrscan.ThemeMode
import com.hexadecinull.qrscan.util.PreferencesManager
import kotlinx.coroutines.launch

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SettingsScreen(onBack: () -> Unit) {
    val context = LocalContext.current
    val scope   = rememberCoroutineScope()
    val prefs   = PreferencesManager(context)

    val themeMode    by prefs.themeMode.collectAsState(initial = ThemeMode.SYSTEM)
    val dynamicColor by prefs.dynamicColor.collectAsState(initial = true)
    val haptics      by prefs.haptics.collectAsState(initial = true)
    val saveHistory  by prefs.saveHistory.collectAsState(initial = true)

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Settings") },
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(Icons.Default.ArrowBack, contentDescription = "Back")
                    }
                }
            )
        }
    ) { pad ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(pad)
                .verticalScroll(rememberScrollState())
        ) {
            Text(
                text = "Appearance",
                style = androidx.compose.material3.MaterialTheme.typography.labelSmall,
                color = androidx.compose.material3.MaterialTheme.colorScheme.primary,
                modifier = Modifier.padding(start = androidx.compose.ui.unit.dp.times(16f), top = androidx.compose.ui.unit.dp.times(16f), bottom = androidx.compose.ui.unit.dp.times(4f))
            )

            ListItem(
                headlineContent = { Text("Theme") },
                supportingContent = { Text(themeMode.name.lowercase().replaceFirstChar { it.uppercase() }) },
                leadingContent = { Icon(Icons.Default.Brightness6, contentDescription = null) },
                trailingContent = {
                    androidx.compose.material3.SegmentedButton @OptIn(ExperimentalMaterial3Api::class) {
                    }
                }
            )

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                ListItem(
                    headlineContent = { Text("Material You") },
                    supportingContent = { Text("Dynamic color from wallpaper") },
                    leadingContent = { Icon(Icons.Default.ColorLens, contentDescription = null) },
                    trailingContent = {
                        Switch(
                            checked = dynamicColor,
                            onCheckedChange = { scope.launch { prefs.setDynamicColor(it) } }
                        )
                    }
                )
            }

            HorizontalDivider()

            Text(
                text = "Scanning",
                style = androidx.compose.material3.MaterialTheme.typography.labelSmall,
                color = androidx.compose.material3.MaterialTheme.colorScheme.primary,
                modifier = Modifier.padding(start = androidx.compose.ui.unit.dp.times(16f), top = androidx.compose.ui.unit.dp.times(16f), bottom = androidx.compose.ui.unit.dp.times(4f))
            )

            ListItem(
                headlineContent = { Text("Haptic feedback") },
                supportingContent = { Text("Vibrate on successful scan") },
                leadingContent = { Icon(Icons.Default.Vibration, contentDescription = null) },
                trailingContent = {
                    Switch(
                        checked = haptics,
                        onCheckedChange = { scope.launch { prefs.setHaptics(it) } }
                    )
                }
            )

            ListItem(
                headlineContent = { Text("Save scan history") },
                supportingContent = { Text("Store scans locally on device") },
                leadingContent = { Icon(Icons.Default.History, contentDescription = null) },
                trailingContent = {
                    Switch(
                        checked = saveHistory,
                        onCheckedChange = { scope.launch { prefs.setSaveHistory(it) } }
                    )
                }
            )

            HorizontalDivider()

            ListItem(
                headlineContent = { Text("About QRScan") },
                supportingContent = { Text("Version 1.0.0  •  GPL-3.0  •  Hexadecinull") },
                leadingContent = { Icon(Icons.Default.Info, contentDescription = null) }
            )
        }
    }
}
