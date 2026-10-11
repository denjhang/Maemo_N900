package com.hexadecinull.qrscan.util

import android.content.Context
import androidx.datastore.core.DataStore
import androidx.datastore.preferences.core.Preferences
import androidx.datastore.preferences.core.booleanPreferencesKey
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.datastore.preferences.preferencesDataStore
import com.hexadecinull.qrscan.ThemeMode
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map

private val Context.dataStore: DataStore<Preferences> by preferencesDataStore(name = "qrscan_prefs")

class PreferencesManager(private val context: Context) {

    private object Keys {
        val THEME_MODE    = stringPreferencesKey("theme_mode")
        val DYNAMIC_COLOR = booleanPreferencesKey("dynamic_color")
        val HAPTICS       = booleanPreferencesKey("haptics")
        val SAVE_HISTORY  = booleanPreferencesKey("save_history")
        val BEEP_ON_SCAN  = booleanPreferencesKey("beep_on_scan")
    }

    val themeMode: Flow<ThemeMode> = context.dataStore.data.map { prefs ->
        runCatching {
            ThemeMode.valueOf(prefs[Keys.THEME_MODE] ?: ThemeMode.SYSTEM.name)
        }.getOrDefault(ThemeMode.SYSTEM)
    }

    val dynamicColor: Flow<Boolean> = context.dataStore.data.map { prefs ->
        prefs[Keys.DYNAMIC_COLOR] != false
    }

    val haptics: Flow<Boolean> = context.dataStore.data.map { prefs ->
        prefs[Keys.HAPTICS] != false
    }

    val saveHistory: Flow<Boolean> = context.dataStore.data.map { prefs ->
        prefs[Keys.SAVE_HISTORY] != false
    }

    val beepOnScan: Flow<Boolean> = context.dataStore.data.map { prefs ->
        prefs[Keys.BEEP_ON_SCAN] ?: false
    }

    suspend fun setThemeMode(mode: ThemeMode) {
        context.dataStore.edit { it[Keys.THEME_MODE] = mode.name }
    }

    suspend fun setDynamicColor(enabled: Boolean) {
        context.dataStore.edit { it[Keys.DYNAMIC_COLOR] = enabled }
    }

    suspend fun setHaptics(enabled: Boolean) {
        context.dataStore.edit { it[Keys.HAPTICS] = enabled }
    }

    suspend fun setSaveHistory(enabled: Boolean) {
        context.dataStore.edit { it[Keys.SAVE_HISTORY] = enabled }
    }

    suspend fun setBeepOnScan(enabled: Boolean) {
        context.dataStore.edit { it[Keys.BEEP_ON_SCAN] = enabled }
    }
}
