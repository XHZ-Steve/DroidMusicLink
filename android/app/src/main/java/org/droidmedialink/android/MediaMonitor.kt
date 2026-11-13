package org.droidmedialink.android

import android.content.ComponentName
import android.content.Context
import android.content.BroadcastReceiver
import android.content.Intent
import android.content.IntentFilter
import android.database.ContentObserver
import android.provider.Settings
import android.net.Uri
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import androidx.core.content.ContextCompat
import android.app.NotificationManager
import android.media.AudioManager
import android.media.Rating
import android.media.MediaMetadata
import android.media.session.*
import android.os.*
import android.service.notification.NotificationListenerService
import android.util.Base64
import android.view.KeyEvent
import org.json.*
import java.io.ByteArrayOutputStream
import kotlinx.coroutines.*
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
 private val audio=context.getSystemService(AudioManager::class.java)
 private var observingVolume=false
 private var receivingVolume=false
 private var lastVolume=""
 private var artworkKey=""
 private var artworkEncoded=""
 private var artworkAttempted=false
 private var artworkJob:Job?=null
 private val volumeRefresh=Runnable { publishVolume() }
 private val volumeConfirm=Runnable { publishVolume(true) }
 private val volumeSettle=Runnable { publishVolume(true) }
 private val ratingRefresh=Runnable { publish() }
 private val volumeObserver=object:ContentObserver(handler){override fun onChange(selfChange:Boolean){scheduleVolumeRefresh()}}
 // OEM compatibility supplement; always read AudioManager rather than trust extras.
 private val volumeReceiver=object:BroadcastReceiver(){override fun onReceive(context:Context?,intent:Intent?){if(intent?.action=="android.media.VOLUME_CHANGED_ACTION")scheduleVolumeRefresh()}}
 private fun scheduleVolumeRefresh(){handler.removeCallbacks(volumeRefresh);handler.postDelayed(volumeRefresh,25)}
 private fun volumeSupported(info:MediaController.PlaybackInfo?)=if(info?.playbackType==MediaController.PlaybackInfo.PLAYBACK_TYPE_REMOTE) info.volumeControl==android.media.VolumeProvider.VOLUME_CONTROL_ABSOLUTE else !audio.isVolumeFixed
 private fun volumeState(info:MediaController.PlaybackInfo?=current?.playbackInfo):JSONObject {
  val local=info?.playbackType!=MediaController.PlaybackInfo.PLAYBACK_TYPE_REMOTE
  return JSONObject().put("volume",if(local)audio.getStreamVolume(AudioManager.STREAM_MUSIC) else info?.currentVolume?:0)
   .put("maxVolume",if(local)audio.getStreamMaxVolume(AudioManager.STREAM_MUSIC) else info?.maxVolume?:0)
 }
 private fun publishVolume(force:Boolean=false,info:MediaController.PlaybackInfo?=current?.playbackInfo){
  val c=current?:return
  val data=runCatching { volumeState(info) }.getOrNull()?:return
  val commands=Hub.media.value.optJSONArray("commands")?:JSONArray()
  val values=(0 until commands.length()).map { commands.optString(it) }.filter { it!="volume" }.toMutableList()
  if(volumeSupported(info))values.add("volume")
  data.put("commands",JSONArray(values))
  val key="${c.sessionToken.hashCode()}:${data.optInt("volume")}:${data.optInt("maxVolume")}:${volumeSupported(info)}"
  if(!force&&key==lastVolume)return
  lastVolume=key
  val snapshot=Hub.media.value;val state=JSONObject();for(key in snapshot.keys())state.put(key,snapshot.get(key));for(name in listOf("volume","maxVolume","commands"))state.put(name,data.get(name))
  val playback=c.playbackState
  val speed=(playback?.playbackSpeed?:1f).takeIf { it.isFinite() }?:1f
  val position=(playback?.position?:0L)+if(playback?.state==PlaybackState.STATE_PLAYING)((SystemClock.elapsedRealtime()-playback.lastPositionUpdateTime)*speed).toLong()else 0L
  state.put("positionMs",position.coerceAtLeast(0)).put("speed",speed).put("state",if(playback?.state==PlaybackState.STATE_PLAYING)"playing" else if(playback?.state==PlaybackState.STATE_PAUSED)"paused" else "stopped")
  Hub.media.value=state;Hub.broadcastVolume?.invoke(data)
 }
 private fun encodeBitmap(bitmap:Bitmap,owned:Boolean):String? {
  var small:Bitmap?=null
  try {
   val scale=minOf(1f,2048f/maxOf(bitmap.width,bitmap.height))
   small=Bitmap.createScaledBitmap(bitmap,maxOf(1,(bitmap.width*scale).toInt()),maxOf(1,(bitmap.height*scale).toInt()),true)
   val out=ByteArrayOutputStream()
   if(!small.compress(Bitmap.CompressFormat.JPEG,92,out)||out.size()==0)return null
   if(out.size()>1536*1024){out.reset();if(!small.compress(Bitmap.CompressFormat.JPEG,85,out)||out.size()==0||out.size()>1536*1024)return null}
   return Base64.encodeToString(out.toByteArray(),Base64.NO_WRAP)
  }finally{if(small!==bitmap)small?.recycle();if(owned)bitmap.recycle()}
 }
 private fun readArtwork(metadata:MediaMetadata):String? {
  val bitmap=metadata.getBitmap(MediaMetadata.METADATA_KEY_ALBUM_ART)?:metadata.getBitmap(MediaMetadata.METADATA_KEY_ART)?:metadata.getBitmap(MediaMetadata.METADATA_KEY_DISPLAY_ICON)
  var best:ByteArray?=null;var bestExtent=bitmap?.let { maxOf(it.width,it.height) }?:0
  for(key in listOf(MediaMetadata.METADATA_KEY_ART_URI,MediaMetadata.METADATA_KEY_ALBUM_ART_URI,MediaMetadata.METADATA_KEY_DISPLAY_ICON_URI)) {
   val address=metadata.getString(key)?:continue
   val uri=Uri.parse(address);if(uri.scheme !in listOf("content","android.resource"))continue
   try {
    val input=context.contentResolver.openInputStream(uri)?:continue
    val bytes=input.use { it.readBytesBounded(2*1024*1024) }?:continue
    val bounds=BitmapFactory.Options().apply { inJustDecodeBounds=true };BitmapFactory.decodeByteArray(bytes,0,bytes.size,bounds)
    if(bounds.outWidth<=0||bounds.outHeight<=0||bounds.outWidth>8192||bounds.outHeight>8192)continue
    if(maxOf(bounds.outWidth,bounds.outHeight)>=bestExtent){best=bytes;bestExtent=maxOf(bounds.outWidth,bounds.outHeight)}
   }catch(_:Exception){ /* Try the next published URI, then the supplied Bitmap. */ }
  }
  best?.let { bytes ->
   if(bestExtent<=2048)return Base64.encodeToString(bytes,Base64.NO_WRAP)
   val options=BitmapFactory.Options().apply { inSampleSize=1 };while(bestExtent/options.inSampleSize>4096)options.inSampleSize*=2
   BitmapFactory.decodeByteArray(bytes,0,bytes.size,options)?.let { decoded -> encodeBitmap(decoded,true)?.let{return it} }
  }
  return bitmap?.let { encodeBitmap(it,false) }
 }
 private fun java.io.InputStream.readBytesBounded(limit:Int):ByteArray? {
  val output=ByteArrayOutputStream();val buffer=ByteArray(8192)
  while(output.size()<=limit){val count=read(buffer,0,minOf(buffer.size,limit+1-output.size()));if(count<=0)break;output.write(buffer,0,count)}
  return if(output.size()<=limit)output.toByteArray() else null
 }
 private fun ensureArtwork(c:MediaController,metadata:MediaMetadata?,key:String){
  if(key!=artworkKey){artworkJob?.cancel();artworkJob=null;artworkKey=key;artworkEncoded="";artworkAttempted=false}
  if(artworkEncoded.isNotEmpty()||artworkAttempted||artworkJob?.isActive==true)return
  artworkAttempted=true
  artworkJob=Hub.scope.launch {
   for(wait in listOf(0L,600L,1800L,4500L)){
    if(wait>0)delay(wait)
    if(artworkKey!=key||current?.sessionToken!=c.sessionToken)return@launch
    val snapshot=c.metadata?:metadata
    val encoded=try{withContext(Dispatchers.IO){snapshot?.let { readArtwork(it) }}}catch(cancelled:CancellationException){throw cancelled}catch(_:Exception){null}
    if(artworkKey!=key||current?.sessionToken!=c.sessionToken)return@launch
    if(!encoded.isNullOrEmpty()){artworkEncoded=encoded;publish();return@launch}
   }
  }
 }

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
  if(!observingVolume)runCatching { context.contentResolver.registerContentObserver(Settings.System.CONTENT_URI,true,volumeObserver);observingVolume=true }
  if(!receivingVolume)runCatching { ContextCompat.registerReceiver(context,volumeReceiver,IntentFilter("android.media.VOLUME_CHANGED_ACTION"),ContextCompat.RECEIVER_EXPORTED);receivingVolume=true }
  Hub.select = { manual = it; publish() }
  Hub.control = { command,payload -> execute(command,payload) }
  Hub.refreshMedia = { refresh() }
  refresh()
  settingsJob=Hub.scope.launch { Hub.preferences.map { Triple(Hub.string("manual"),Hub.string("preferred"),Hub.string("excluded")) }.distinctUntilChanged().collect { manual=it.first.ifEmpty { null };publish() } }
 }
 fun refresh() {
  artworkAttempted=false
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
    override fun onExtrasChanged(extras:Bundle?){publish()}
    override fun onMetadataChanged(metadata: MediaMetadata?) { if(current?.sessionToken==c.sessionToken&&artworkEncoded.isEmpty()){artworkJob?.cancel();artworkJob=null;artworkAttempted=false};publish() }
    override fun onPlaybackStateChanged(state: PlaybackState?) = publish()
    override fun onAudioInfoChanged(info: MediaController.PlaybackInfo) { if(current?.sessionToken==c.sessionToken)publishVolume(true,info) }
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
   artworkJob?.cancel();artworkJob=null;artworkKey="";artworkEncoded="";artworkAttempted=false;lastVolume=""
   val data=JSONObject().put("state","stopped").put("commands",JSONArray(if(canUseSystemKeys())MediaCommands.basic else emptyList<String>()))
   for(key in listOf("title","artist","album","appName","packageName","sessionId","artwork"))data.put(key,"")
   for(key in listOf("durationMs","positionMs","speed","volume","maxVolume"))data.put(key,0)
   data.put("favorite",false).put("favoriteKnown",false).put("phoneLyrics","").put("phoneTranslation","")
   Hub.media.value=data;Hub.broadcast?.invoke(data);return
  }
  val m=c.metadata;val s=c.playbackState
  val actions=s?.actions ?: 0L
  val commands=MediaCommands.basic.toMutableList()
  fun supports(flag:Long,name:String) { if(actions and flag != 0L) commands.add(name) }
  supports(PlaybackState.ACTION_SEEK_TO,"seek")
  if(volumeSupported(c.playbackInfo)) commands.add("volume")
  val rating=runCatching{m?.getRating(MediaMetadata.METADATA_KEY_USER_RATING)}.getOrNull()
  val heartSupported=!m?.getString(MediaMetadata.METADATA_KEY_TITLE).isNullOrBlank() && actions and PlaybackState.ACTION_SET_RATING!=0L && (c.ratingType==Rating.RATING_HEART||rating?.ratingStyle==Rating.RATING_HEART)
  if(heartSupported)commands.add("favorite")
  val duration=(m?.getLong(MediaMetadata.METADATA_KEY_DURATION) ?: 0L).coerceAtLeast(0)
  val speed=(s?.playbackSpeed ?: 1f).takeIf { it.isFinite() } ?: 1f
  val position = (s?.position ?: 0L) + if(s?.state == PlaybackState.STATE_PLAYING) ((SystemClock.elapsedRealtime()-s.lastPositionUpdateTime)*speed).toLong() else 0L
  val data=JSONObject().put("title",m?.getString(MediaMetadata.METADATA_KEY_TITLE).orEmpty()).put("artist",m?.getString(MediaMetadata.METADATA_KEY_ARTIST).orEmpty()).put("album",m?.getString(MediaMetadata.METADATA_KEY_ALBUM).orEmpty())
   .put("appName",appName(c)).put("packageName",c.packageName).put("sessionId",c.packageName+":"+c.sessionToken.hashCode())
   .put("durationMs",duration).put("positionMs",position.coerceAtLeast(0)).put("speed",speed)
   .put("state",if(s?.state==PlaybackState.STATE_PLAYING) "playing" else if(s?.state==PlaybackState.STATE_PAUSED) "paused" else "stopped")
   .put("commands",JSONArray(commands))
  data.put("favoriteKnown",rating?.ratingStyle==Rating.RATING_HEART&&rating.isRated)
   .put("favorite",rating?.ratingStyle==Rating.RATING_HEART&&rating.hasHeart())
   .put("phoneLyrics",PhoneLyrics.read(m,runCatching{c.extras}.getOrNull()))
  data.put("phoneTranslation",PhoneLyrics.translated(data.optString("phoneLyrics")))
  val volume=volumeState(c.playbackInfo);data.put("volume",volume.optInt("volume")).put("maxVolume",volume.optInt("maxVolume"))
  val key="${c.sessionToken.hashCode()}:${data.optString("title")}:${data.optString("artist")}:${data.optString("album")}:${m?.getString(MediaMetadata.METADATA_KEY_ALBUM_ART_URI)}:${m?.getString(MediaMetadata.METADATA_KEY_ART_URI)}:${m?.getString(MediaMetadata.METADATA_KEY_DISPLAY_ICON_URI)}"
  ensureArtwork(c,m,key)
  data.put("artwork",artworkEncoded)

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
    "favorite" -> {
     val value=payload.optDouble("value",Double.NaN)
     if(value!=0.0&&value!=1.0)return "invalid_favorite"
     if(payload.optString("sessionId")!=c.packageName+":"+c.sessionToken.hashCode() || payload.optString("title")!=c.metadata?.getString(MediaMetadata.METADATA_KEY_TITLE).orEmpty() || payload.optString("artist")!=c.metadata?.getString(MediaMetadata.METADATA_KEY_ARTIST).orEmpty() || payload.optString("album")!=c.metadata?.getString(MediaMetadata.METADATA_KEY_ALBUM).orEmpty())return "track_changed"
     val rating=runCatching{c.metadata?.getRating(MediaMetadata.METADATA_KEY_USER_RATING)}.getOrNull()
     if(((c.playbackState?.actions?:0L) and PlaybackState.ACTION_SET_RATING)==0L || (c.ratingType!=Rating.RATING_HEART&&rating?.ratingStyle!=Rating.RATING_HEART))return "unsupported"
     t.setRating(Rating.newHeartRating(value==1.0))
     handler.removeCallbacks(ratingRefresh);handler.postDelayed(ratingRefresh,250)
    }
    "seek" -> { val value=payload.optLong("positionMs",payload.optLong("value",-1));val duration=Hub.media.value.optLong("durationMs");if(value<0 || duration>0 && value>duration) return "invalid_position";t.seekTo(value) }
    "volume" -> { val value=payload.optDouble("volume",payload.optDouble("value",-1.0));if(!ProtocolGuard.volumeValid(value)) return "invalid_volume";if(c.playbackInfo?.playbackType==MediaController.PlaybackInfo.PLAYBACK_TYPE_REMOTE)c.setVolumeTo(kotlin.math.round(value*(c.playbackInfo?.maxVolume?:0)).toInt(),0)
     else audio.setStreamVolume(AudioManager.STREAM_MUSIC,kotlin.math.round(value*audio.getStreamMaxVolume(AudioManager.STREAM_MUSIC)).toInt(),0)
     publishVolume(true);handler.removeCallbacks(volumeConfirm);handler.removeCallbacks(volumeSettle);handler.postDelayed(volumeConfirm,120);handler.postDelayed(volumeSettle,400) }
    else -> return "unsupported"
   };null
  } catch(_:SecurityException) { "permission_denied" } catch(_:RuntimeException) { "media_unavailable" }
 }
 private fun mediaKeys(key:Int,dispatch:(KeyEvent)->Boolean):Boolean { val now=SystemClock.uptimeMillis();val down=dispatch(KeyEvent(now,now,KeyEvent.ACTION_DOWN,key,0));val up=dispatch(KeyEvent(now,SystemClock.uptimeMillis(),KeyEvent.ACTION_UP,key,0));return down&&up }
 fun stop() { handler.removeCallbacks(ratingRefresh);artworkJob?.cancel();artworkJob=null;handler.removeCallbacks(volumeRefresh);handler.removeCallbacks(volumeConfirm);handler.removeCallbacks(volumeSettle);if(observingVolume){context.contentResolver.unregisterContentObserver(volumeObserver);observingVolume=false};if(receivingVolume){runCatching { context.unregisterReceiver(volumeReceiver) };receivingVolume=false};settingsJob?.cancel();settingsJob=null;if(listening)manager.removeOnActiveSessionsChangedListener(listener);listening=false;controllers.forEach { (c,cb)->c.unregisterCallback(cb) };controllers.clear();Hub.control=null;Hub.select=null;Hub.refreshMedia=null;Hub.sources.value=emptyList();Hub.media.value=JSONObject().put("state","stopped").put("commands",JSONArray());Hub.broadcast?.invoke(Hub.media.value) }
}

