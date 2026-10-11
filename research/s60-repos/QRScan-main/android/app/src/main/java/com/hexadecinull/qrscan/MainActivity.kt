package com.hexadecinull.qrscan

import android.content.Intent
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.lifecycle.viewmodel.compose.viewModel
import com.hexadecinull.qrscan.theme.QRScanTheme
import com.hexadecinull.qrscan.ui.QRScanNavHost
import com.hexadecinull.qrscan.util.PreferencesManager

class MainActivity : ComponentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        val sharedImageUri = if (intent?.action == Intent.ACTION_SEND) {
            intent.getParcelableExtra<android.net.Uri>(Intent.EXTRA_STREAM)
        } else null

        setContent {
            val prefs = PreferencesManager(this)
            val themeMode by prefs.themeMode.collectAsState(initial = ThemeMode.SYSTEM)
            val dynamicColor by prefs.dynamicColor.collectAsState(initial = true)
            val systemDark = isSystemInDarkTheme()
            val darkTheme = when (themeMode) {
                ThemeMode.LIGHT  -> false
                ThemeMode.DARK   -> true
                ThemeMode.SYSTEM -> systemDark
            }
            QRScanTheme(darkTheme = darkTheme, dynamicColor = dynamicColor) {
                QRScanNavHost(sharedImageUri = sharedImageUri)
            }
        }
    }
}

enum class ThemeMode { LIGHT, DARK, SYSTEM }
