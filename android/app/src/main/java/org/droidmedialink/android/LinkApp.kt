package org.droidmedialink.android

import android.app.Application
import android.content.Context
import androidx.datastore.preferences.core.*
import androidx.datastore.preferences.preferencesDataStore
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.*
import org.json.JSONObject

val Context.settings by preferencesDataStore("settings", corruptionHandler = androidx.datastore.core.handlers.ReplaceFileCorruptionHandler { emptyPreferences() })
class LinkApp : Application() {
 override fun onCreate() { super.onCreate(); Hub.app = this; Hub.scope.launch { settings.data.catch { emit(emptyPreferences()) }.collect { Hub.preferences.value = it } } }
}
data class PendingPair(val clientId: String, val name: String, val sas: String, val approve: (Boolean) -> Unit)
object Hub {
 lateinit var app: LinkApp
 val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
 val preferences = MutableStateFlow<Preferences>(emptyPreferences())
 val running = MutableStateFlow(false)
 val mediaAccess = MutableStateFlow(false)
 val error = MutableStateFlow("")
 val media = MutableStateFlow(JSONObject())
 val sources = MutableStateFlow<List<Pair<String,String>>>(emptyList())
 val lastSeen = MutableStateFlow<Map<String,Long>>(emptyMap())
 val peers = MutableStateFlow<List<Pair<String,String>>>(emptyList())
 val connected = MutableStateFlow<Set<String>>(emptySet())
 val pending = MutableStateFlow<PendingPair?>(null)
 var broadcast: ((JSONObject) -> Unit)? = null
 var broadcastVolume: ((JSONObject) -> Unit)? = null
 var control: ((String, JSONObject) -> String?)? = null
 var refreshMedia: (() -> Unit)? = null
 var revoke: ((String) -> Unit)? = null
 var select: ((String?) -> Unit)? = null
 fun set(key: String, value: Boolean) { scope.launch { app.settings.edit { it[booleanPreferencesKey(key)] = value } } }
 fun set(key: String, value: String) { scope.launch { app.settings.edit { it[stringPreferencesKey(key)] = value } } }
 fun bool(key: String, default: Boolean = false) = preferences.value[booleanPreferencesKey(key)] ?: default
 fun string(key: String, default: String = "") = preferences.value[stringPreferencesKey(key)] ?: default
}
