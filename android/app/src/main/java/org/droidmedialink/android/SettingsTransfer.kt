package org.droidmedialink.android
import android.content.Context
import androidx.datastore.preferences.core.*
import kotlinx.coroutines.flow.first
import org.json.JSONObject
object SettingsTransfer {
 private val strings=setOf("theme","language","preferred","excluded")
 private val booleans=setOf("autoStart","liquidGlass")
 suspend fun export(context:Context):String {
  val prefs=context.settings.data.first();val json=JSONObject().put("schemaVersion",1)
  strings.forEach { key -> json.put(key,prefs[stringPreferencesKey(key)] ?: "") }
  booleans.forEach { key -> json.put(key,prefs[booleanPreferencesKey(key)] ?: false) }
  return json.toString(2)
 }
 suspend fun import(context:Context, text:String) {
  require(text.length<=65536)
  val json=JSONObject(text);require(json.getInt("schemaVersion")==1)
  strings.forEach { key -> if(json.has(key))require(json.get(key) is String && json.getString(key).length<=2048) }
  require(json.optString("theme") in setOf("","light","dark"));require(json.optString("language") in setOf("","en","zh-CN"))
  booleans.forEach { key -> if(json.has(key))require(json.get(key) is Boolean) }
  context.settings.edit { prefs ->
   strings.forEach { key -> prefs[stringPreferencesKey(key)]=json.optString(key,"") }
   booleans.forEach { key -> prefs[booleanPreferencesKey(key)]=json.optBoolean(key,false) }
   prefs[intPreferencesKey("schemaVersion")]=1
  }
  context.getSharedPreferences("locale",0).edit().putString("language",json.optString("language")).apply()
 }
 suspend fun reset(context:Context) = import(context,"{\"schemaVersion\":1}")
}
