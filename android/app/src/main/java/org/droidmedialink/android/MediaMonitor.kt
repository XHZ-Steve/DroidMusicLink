package org.droidmedialink.android

import android.content.ComponentName
import android.content.Context
import android.app.NotificationManager
import android.media.AudioManager
import android.media.MediaMetadata
import android.media.session.*
import android.os.*
import android.service.notification.NotificationListenerService
import android.util.Base64
import android.view.KeyEvent
import org.json.*
import java.io.ByteArrayOutputStream
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.map

// Authorization belongs to the notification listener, but session tracking belongs
// to the foreground link service: OEMs can leave an approved listener unbound.
class MediaMonitor : NotificationListenerService() {
 override fun onListenerConnected() { Hub.refreshMedia?.invoke() }
 override fun onListenerDisconnected() { Hub.refreshMedia?.invoke();if(Hub.running.value)reconnect(this) }
 companion object {
  fun hasAccess(context:Context):Boolean {
   val component=ComponentName(context,MediaMonitor::class.java)
   return if(Build.VERSION.SDK_INT>=27)context.getSystemService(NotificationManager::class.java).isNotificationListenerAccessGranted(component)
   else android.provider.Settings.Secure.getString(context.contentResolver,"enabled_notification_listeners").orEmpty().split(':').any { ComponentName.unflattenFromString(it)==component }
  }
  fun reconnect(context:Context) { Hub.mediaAccess.value=hasAccess(context);if(Hub.mediaAccess.value)runCatching { requestRebind(ComponentName(context,MediaMonitor::class.java)) } }
 }
}

class MediaSessionTracker(private val context:Context) {
 private val handler = Handler(Looper.getMainLooper())
 private val manager=context.getSystemService(MediaSessionManager::class.java)
 private val component=ComponentName(context,MediaMonitor::class.java)
 private var listening=false
 private val controllers = linkedMapOf<MediaController,MediaController.Callback>()
 private var settingsJob:Job?=null
 private var manual: String? = null
 private var current: MediaController? = null
 private val listener = MediaSessionManager.OnActiveSessionsChangedListener { attach(it.orEmpty()) }
 fun start() {
  manual=Hub.string("manual").ifEmpty { null }
  Hub.select = { manual = it; publish() }
  Hub.control = { command,payload -> execute(command,payload) }
  Hub.refreshMedia = { refresh() }
  refresh()
  settingsJob=Hub.scope.launch { Hub.preferences.map { Triple(Hub.string("manual"),Hub.string("preferred"),Hub.string("excluded")) }.distinctUntilChanged().collect { manual=it.first.ifEmpty { null };publish() } }
 }
 fun refresh() {
  Hub.mediaAccess.value=MediaMonitor.hasAccess(context)
  try {
   if(!Hub.mediaAccess.value) { if(listening)manager.removeOnActiveSessionsChangedListener(listener);listening=false;attach(emptyList());return }
   if(!listening) { manager.addOnActiveSessionsChangedListener(listener,component,handler);listening=true }
   attach(manager.getActiveSessions(component))
  } catch(_:SecurityException) { if(listening)runCatching { manager.removeOnActiveSessionsChangedListener(listener) };listening=false;attach(emptyList()) }
 }
 private fun attach(sessions: List<MediaController>) {
  controllers.forEach { (c,callback) -> c.unregisterCallback(callback) }; controllers.clear()
  sessions.forEach { c ->
   val cb = object: MediaController.Callback() {
    override fun onMetadataChanged(metadata: MediaMetadata?) = publish()
    override fun onPlaybackStateChanged(state: PlaybackState?) = publish()
    override fun onAudioInfoChanged(info: MediaController.PlaybackInfo) = publish()
    override fun onSessionDestroyed() { controllers.remove(c); publish() }
   }; controllers[c]=cb; c.registerCallback(cb,handler)
  }; publish()
 }
 private fun appName(c: MediaController) = runCatching { context.packageManager.getApplicationLabel(context.packageManager.getApplicationInfo(c.packageName,0)).toString() }.getOrDefault(c.packageName)
 private fun canUseSystemKeys()=controllers.isEmpty() && manual==null && Hub.string("excluded").isBlank()
 private fun publish() {
  val excluded = Hub.string("excluded").split(',').map { it.trim() }
  val eligible = controllers.keys.filter { it.packageName !in excluded }
  current = eligible.firstOrNull { it.packageName == manual } ?: eligible.sortedByDescending { if(it.playbackState?.state == PlaybackState.STATE_PLAYING) 100 else if(it.packageName == Hub.string("preferred")) 10 else 0 }.firstOrNull()
  Hub.sources.value = controllers.keys.map { it.packageName to appName(it) }
  val c=current
  if(c==null) {
   val data=JSONObject().put("state","stopped").put("commands",JSONArray(if(canUseSystemKeys())MediaCommands.basic else emptyList<String>()))
   for(key in listOf("title","artist","album","appName","packageName","sessionId","artwork"))data.put(key,"")
   for(key in listOf("durationMs","positionMs","speed","volume","maxVolume"))data.put(key,0)
   Hub.media.value=data;Hub.broadcast?.invoke(data);return
  }
  val m=c.metadata;val s=c.playbackState
  val actions=s?.actions ?: 0L
  val commands=MediaCommands.basic.toMutableList()
  fun supports(flag:Long,name:String) { if(actions and flag != 0L) commands.add(name) }
  supports(PlaybackState.ACTION_SEEK_TO,"seek")
  if(c.playbackInfo?.volumeControl == android.media.VolumeProvider.VOLUME_CONTROL_ABSOLUTE) commands.add("volume")
  val duration=(m?.getLong(MediaMetadata.METADATA_KEY_DURATION) ?: 0L).coerceAtLeast(0)
  val speed=(s?.playbackSpeed ?: 1f).takeIf { it.isFinite() } ?: 1f
  val position = (s?.position ?: 0L) + if(s?.state == PlaybackState.STATE_PLAYING) ((SystemClock.elapsedRealtime()-s.lastPositionUpdateTime)*speed).toLong() else 0L
  val data=JSONObject().put("title",m?.getString(MediaMetadata.METADATA_KEY_TITLE).orEmpty()).put("artist",m?.getString(MediaMetadata.METADATA_KEY_ARTIST).orEmpty()).put("album",m?.getString(MediaMetadata.METADATA_KEY_ALBUM).orEmpty())
   .put("appName",appName(c)).put("packageName",c.packageName).put("sessionId",c.packageName+":"+c.sessionToken.hashCode())
   .put("durationMs",duration).put("positionMs",position.coerceAtLeast(0)).put("speed",speed)
   .put("state",if(s?.state==PlaybackState.STATE_PLAYING) "playing" else if(s?.state==PlaybackState.STATE_PAUSED) "paused" else "stopped")
   .put("commands",JSONArray(commands)).put("volume",c.playbackInfo?.currentVolume ?: 0).put("maxVolume",c.playbackInfo?.maxVolume ?: 0)
  runCatching {
   val bitmap=m?.getBitmap(MediaMetadata.METADATA_KEY_ALBUM_ART) ?: m?.getBitmap(MediaMetadata.METADATA_KEY_ART)
   if(bitmap!=null) {
    val scale=minOf(1f,512f/maxOf(bitmap.width,bitmap.height))
    val small=android.graphics.Bitmap.createScaledBitmap(bitmap,maxOf(1,(bitmap.width*scale).toInt()),maxOf(1,(bitmap.height*scale).toInt()),true)
    val out=ByteArrayOutputStream();small.compress(android.graphics.Bitmap.CompressFormat.JPEG,78,out)
    if(out.size()<=256*1024) data.put("artwork",Base64.encodeToString(out.toByteArray(),Base64.NO_WRAP))
    if(small!==bitmap) small.recycle()
   }
  }
  Hub.media.value=data;Hub.broadcast?.invoke(data)
 }
 private fun execute(command:String,payload:JSONObject):String? {
  val c=current
  if(c==null) {
   val key=MediaCommands.keyCode(command) ?: return "no_session"
   if(!canUseSystemKeys())return "no_session"
   return try { val audio=context.getSystemService(AudioManager::class.java);mediaKeys(key) { audio.dispatchMediaKeyEvent(it);true };null } catch(_:RuntimeException) { "media_unavailable" }
  }
  val available=Hub.media.value.optJSONArray("commands") ?: return "unsupported"
  if((0 until available.length()).none { available.optString(it)==command }) return "unsupported"
  val t=c.transportControls
  return try {
   when(command) {
    "play" -> t.play();"pause" -> t.pause();"toggle" -> if(c.playbackState?.state==PlaybackState.STATE_PLAYING) t.pause() else t.play()
    "next" -> t.skipToNext();"previous" -> t.skipToPrevious()
    "seek" -> { val value=payload.optLong("positionMs",payload.optLong("value",-1));val duration=Hub.media.value.optLong("durationMs");if(value<0 || duration>0 && value>duration) return "invalid_position";t.seekTo(value) }
    "volume" -> { val value=payload.optDouble("volume",payload.optDouble("value",-1.0));if(!ProtocolGuard.volumeValid(value)) return "invalid_volume";c.setVolumeTo(kotlin.math.round(value*(c.playbackInfo?.maxVolume ?: 0)).toInt(),0) }
    else -> return "unsupported"
   };null
  } catch(_:SecurityException) { "permission_denied" } catch(_:RuntimeException) { "media_unavailable" }
 }
 private fun mediaKeys(key:Int,dispatch:(KeyEvent)->Boolean):Boolean { val now=SystemClock.uptimeMillis();val down=dispatch(KeyEvent(now,now,KeyEvent.ACTION_DOWN,key,0));val up=dispatch(KeyEvent(now,SystemClock.uptimeMillis(),KeyEvent.ACTION_UP,key,0));return down&&up }
 fun stop() { settingsJob?.cancel();settingsJob=null;if(listening)manager.removeOnActiveSessionsChangedListener(listener);listening=false;controllers.forEach { (c,cb)->c.unregisterCallback(cb) };controllers.clear();Hub.control=null;Hub.select=null;Hub.refreshMedia=null;Hub.sources.value=emptyList();Hub.media.value=JSONObject().put("state","stopped").put("commands",JSONArray());Hub.broadcast?.invoke(Hub.media.value) }
}

