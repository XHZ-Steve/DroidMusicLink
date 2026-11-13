package org.droidmedialink.android

import android.app.*
import android.content.*
import android.net.*
import android.net.nsd.*
import android.os.*
import org.java_websocket.*
import org.java_websocket.drafts.Draft_6455
import org.java_websocket.handshake.ClientHandshake
import org.java_websocket.server.*
import org.json.*
import java.net.*
import java.security.SecureRandom
import java.util.*
import java.util.concurrent.ConcurrentHashMap
import kotlinx.coroutines.launch
import kotlinx.coroutines.flow.first

private data class Challenge(val clientId:String,val name:String,val clientCommit:String,val nonce:ByteArray,val expires:Long,var revealed:Boolean=false)

class PassiveLinkService : Service() {
 private lateinit var store: PairingStore
 private var mediaTracker:MediaSessionTracker?=null
 private var server: LinkServer?=null
 private var registration:NsdManager.RegistrationListener?=null
 private val handler=Handler(Looper.getMainLooper())
 private val networkCallback=object:ConnectivityManager.NetworkCallback() {
  override fun onAvailable(network:Network) { handler.post { restartNetwork() } }
  override fun onLost(network:Network) { handler.post { restartNetwork() } }
  override fun onLinkPropertiesChanged(network:Network, properties:LinkProperties) { handler.post { restartNetwork() } }
 }
 private var boundAddress:String?=null
 override fun onCreate() {
  super.onCreate()
  val notifications=getSystemService(NotificationManager::class.java)
  notifications.createNotificationChannel(NotificationChannel("link",getString(R.string.service),NotificationManager.IMPORTANCE_LOW))
  val launch=PendingIntent.getActivity(this,0,Intent(this,MainActivity::class.java),PendingIntent.FLAG_IMMUTABLE)
  val stop=PendingIntent.getService(this,1,Intent(this,javaClass).setAction("stop"),PendingIntent.FLAG_IMMUTABLE)
  startForeground(1,Notification.Builder(this,"link").setContentTitle(getString(R.string.app_name)).setContentText(getString(R.string.running)).setSmallIcon(R.drawable.ic_link).setContentIntent(launch).addAction(Notification.Action.Builder(null,getString(R.string.stop),stop).build()).build())
  try { store=PairingStore(this);store.refresh();Hub.running.value=true;Hub.set("serviceEnabled",true)
   mediaTracker=MediaSessionTracker(this).also { it.start() };MediaMonitor.reconnect(this)
   getSystemService(ConnectivityManager::class.java).registerDefaultNetworkCallback(networkCallback)
   restartNetwork()
  } catch(failure:Exception) { android.util.Log.e("DroidMediaLink", "Service startup failed", failure);Hub.error.value=getString(R.string.service_error);stopSelf() }
 }
 private fun restartNetwork() {
  val cm=getSystemService(ConnectivityManager::class.java)
  val active=cm.activeNetwork
  val caps=cm.getNetworkCapabilities(active)
  val ip=if(caps?.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)==true || caps?.hasTransport(NetworkCapabilities.TRANSPORT_ETHERNET)==true) cm.getLinkProperties(active)?.linkAddresses?.map { it.address }?.firstOrNull { it is Inet4Address && !it.isLoopbackAddress }?.hostAddress else null
  if(ip==boundAddress && server!=null)return
  stopNetwork();boundAddress=ip
  if(ip==null) { Hub.error.value=getString(R.string.wifi_required);return }
  Hub.error.value=""
  server=LinkServer(InetSocketAddress(ip,18123)).apply { setWebSocketFactory(DefaultSSLWebSocketServerFactory(store.tls()));isReuseAddr=true;connectionLostTimeout=45;start() }
 }
 private fun stopNetwork() {
  registration?.let { runCatching { getSystemService(NsdManager::class.java).unregisterService(it) } };registration=null
  server?.let { runCatching { it.stop(500) } };server=null;Hub.broadcast=null;Hub.broadcastVolume=null;Hub.revoke=null;Hub.connected.value=emptySet();Hub.pending.value=null
 }
 override fun onStartCommand(intent:Intent?,flags:Int,startId:Int):Int {
  if(intent?.action=="stop") { Hub.set("serviceEnabled",false);stopSelf();return START_NOT_STICKY }
  return START_STICKY
 }
 override fun onBind(intent:Intent?)=null
 override fun onDestroy() { runCatching { getSystemService(ConnectivityManager::class.java).unregisterNetworkCallback(networkCallback) };stopNetwork();mediaTracker?.stop();mediaTracker=null;Hub.running.value=false;super.onDestroy() }
 inner class LinkServer(address:InetSocketAddress):WebSocketServer(address,listOf(Draft_6455(listOf(org.java_websocket.extensions.DefaultExtension()),listOf(org.java_websocket.protocols.Protocol("")),524288))) {
  private val authenticated=ConcurrentHashMap<WebSocket,String>()
  private val lastPhoneLyrics=ConcurrentHashMap<WebSocket,String>()
  private val peerFeatures=ConcurrentHashMap<WebSocket,Set<String>>()
  private val seen=ConcurrentHashMap<WebSocket,ReplayWindow>()
  private val attempts=ConcurrentHashMap<String,Pair<Long,Int>>()
  private val requestIds=ConcurrentHashMap<String,ReplayWindow>()
  private val rates=ConcurrentHashMap<WebSocket,Pair<Long,Int>>()
  private val opened=ConcurrentHashMap<WebSocket,Long>()
  private var pendingOwner:WebSocket?=null
  private val challenges=ConcurrentHashMap<WebSocket,Challenge>()
  private fun sendMediaSnapshot(conn:WebSocket,data:JSONObject) {
   val lyrics=data.optString("phoneLyrics")
   val identity=listOf("sessionId","title","artist","album","durationMs").joinToString("\u0000"){data.optString(it)}+"\u0000"+lyrics+"\u0000"+data.optString("phoneTranslation")
   val snapshot=JSONObject()
   val repeated=lastPhoneLyrics.put(conn,identity)==identity
   for(key in data.keys())if((key!="phoneLyrics"&&key!="phoneTranslation")||!repeated)snapshot.put(key,data.get(key))
   send(conn,"session_snapshot",snapshot)
  }
  override fun onStart() {
   Hub.revoke={ id -> authenticated.entries.filter { it.value==id }.forEach { authenticated.remove(it.key);it.key.close(1008,"authorization_revoked") };Hub.connected.value=authenticated.values.toSet() }
   Hub.broadcast={ data -> authenticated.keys.forEach { sendMediaSnapshot(it,data) } }
   Hub.broadcastVolume={ data -> authenticated.keys.forEach { if(peerFeatures[it]?.contains("volume_state")==true)send(it,"volume_state",data)else sendMediaSnapshot(it,Hub.media.value) } }
   val nsd=getSystemService(NsdManager::class.java)
   registration=object:NsdManager.RegistrationListener {
    override fun onServiceRegistered(info:NsdServiceInfo) {}
    override fun onRegistrationFailed(info:NsdServiceInfo,errorCode:Int) { Hub.error.value=getString(R.string.discovery_error) }
    override fun onServiceUnregistered(info:NsdServiceInfo) {}
    override fun onUnregistrationFailed(info:NsdServiceInfo,errorCode:Int) {}
   }.also { nsd.registerService(NsdServiceInfo().apply { serviceName="DroidMediaLink-"+Build.MODEL.take(30);serviceType="_droidmedialink._tcp.";port=18123;setAttribute("version","1");setAttribute("pairingVersion","2");setAttribute("deviceId",store.fingerprint);setAttribute("name",Build.MODEL.take(80)) },NsdManager.PROTOCOL_DNS_SD,it) }
  }
  override fun onOpen(conn:WebSocket,handshake:ClientHandshake) {
   if(connections.size>8) { conn.close(1013,"capacity");return }
   opened[conn]=SystemClock.elapsedRealtime();seen[conn]=ReplayWindow()
   send(conn,"hello",JSONObject().put("name",Build.MODEL).put("deviceId",store.fingerprint).put("certificateFingerprint",store.fingerprint))
   handler.postDelayed({ if(conn.isOpen && !authenticated.containsKey(conn))conn.close(1008,"authentication_timeout") },120000)
  }
  override fun onClose(conn:WebSocket,code:Int,reason:String,remote:Boolean) { lastPhoneLyrics.remove(conn);challenges.remove(conn);authenticated.remove(conn);peerFeatures.remove(conn);seen.remove(conn);opened.remove(conn);rates.remove(conn);Hub.connected.value=authenticated.values.toSet();handler.post { if(pendingOwner===conn) { Hub.pending.value=null;pendingOwner=null } } }
  override fun onError(conn:WebSocket?,ex:Exception) { if(conn==null) { Hub.error.value=getString(R.string.network_error) } }
  private fun send(c:WebSocket,type:String,payload:JSONObject) { if(c.isOpen)runCatching { c.send(JSONObject().put("protocolVersion",1).put("messageId",UUID.randomUUID().toString()).put("timestamp",System.currentTimeMillis()).put("type",type).put("payload",payload).toString()) } }
  private fun error(c:WebSocket,code:String) = send(c,"error",JSONObject().put("code",code))
  override fun onMessage(conn:WebSocket,message:String) {
   try {
    require(message.length<=65536)
    val tick=SystemClock.elapsedRealtime();val rate=rates[conn];val messages=if(rate!=null && tick-rate.first<1000)rate.second+1 else 1
    rates[conn]=(if(rate!=null && tick-rate.first<1000)rate.first else tick) to messages
    if(messages>30) { conn.close(1008,"rate_limited");return }
    val m=JSONObject(message);require(m.getInt("protocolVersion")==1)
    val id=m.getString("messageId");require(id.length in 1..128)
    require(ProtocolGuard.timestampValid(m.getLong("timestamp"),System.currentTimeMillis()))
    val ids=seen[conn] ?: return
    if(!ids.accept(id,SystemClock.elapsedRealtime())) { error(conn,"duplicate_message");return }
    val payload=m.getJSONObject("payload")
    when(m.getString("type")) {
     "ping" -> send(conn,"pong",payload)
     "pair_begin" -> {
      require(!authenticated.containsKey(conn) && !challenges.containsKey(conn))
      val host=conn.remoteSocketAddress?.address?.hostAddress ?: "unknown"
      val now=SystemClock.elapsedRealtime();val old=attempts[host];val count=if(old!=null && now-old.first<300000)old.second+1 else 1
      attempts[host]=(if(old!=null && now-old.first<300000)old.first else now) to count
      if(count>5) { error(conn,"pairing_rate_limited");return }
      val clientId=payload.getString("clientId");val name=payload.getString("clientName");val commit=payload.getString("clientCommit")
      require(clientId.matches(Regex("[A-Za-z0-9._-]{1,128}")) && name.length in 1..80);PairingSas.bytes(commit)
      val challenge=Challenge(clientId,name,commit,ByteArray(32).also { SecureRandom().nextBytes(it) },now+120000)
      challenges[conn]=challenge;conn.setAttachment(clientId)
      send(conn,"pair_challenge",JSONObject().put("serverCommit",PairingSas.hex(PairingSas.commitment(challenge.nonce))))
     }
     "pair_reveal" -> {
      val challenge=challenges[conn] ?: run { error(conn,"pairing_expired");return }
      if(challenge.revealed || SystemClock.elapsedRealtime()>challenge.expires) { challenges.remove(conn);error(conn,"pairing_expired");return }
      val nonce=PairingSas.bytes(payload.getString("clientNonce"))
      if(!PairingSas.matches(nonce,challenge.clientCommit)) { challenges.remove(conn);error(conn,"commitment_mismatch");conn.close(1008,"commitment_mismatch");return }
      challenge.revealed=true
      val sas=PairingSas.code(store.fingerprint,nonce,challenge.nonce,challenge.clientId)
      handler.post {
       if(Hub.pending.value!=null) { challenges.remove(conn);error(conn,"pairing_busy");return@post }
       if(!conn.isOpen || challenges[conn]!==challenge)return@post
       send(conn,"pair_compare",JSONObject().put("serverNonce",PairingSas.hex(challenge.nonce)).put("deviceId",store.fingerprint).put("name",Build.MODEL))
       val pending=PendingPair(challenge.clientId,challenge.name,sas) { approved ->
        if(challenges.remove(conn,challenge) && approved && conn.isOpen && SystemClock.elapsedRealtime()<challenge.expires) {
         val token=store.add(challenge.clientId,challenge.name)
         send(conn,"pair_approve",JSONObject().put("clientId",challenge.clientId).put("token",token).put("deviceId",store.fingerprint).put("name",Build.MODEL))
         authenticated[conn]=challenge.clientId;Hub.connected.value=authenticated.values.toSet();sendMediaSnapshot(conn,Hub.media.value)
        } else error(conn,"pairing_rejected")
        Hub.pending.value=null
       }
       pendingOwner=conn;Hub.pending.value=pending
       handler.postDelayed({ if(Hub.pending.value===pending) { challenges.remove(conn);Hub.pending.value=null;error(conn,"pairing_expired") } },(challenge.expires-SystemClock.elapsedRealtime()).coerceAtLeast(1))
      }
     }
     "pair_cancel" -> { challenges.remove(conn);handler.post { if(pendingOwner===conn) { Hub.pending.value=null;pendingOwner=null } } }
     "auth" -> {
      val clientId=payload.getString("clientId");val token=payload.getString("token")
      if(!store.authenticate(clientId,token)) { error(conn,"unauthorized");conn.close(1008,"unauthorized");return }
      store.touch(clientId);authenticated[conn]=clientId;Hub.connected.value=authenticated.values.toSet();send(conn,"auth_result",JSONObject().put("authenticated",true));send(conn,"capabilities",JSONObject().put("features",JSONArray(listOf("volume_state","get_snapshot"))));sendMediaSnapshot(conn,Hub.media.value)
     }
     "capabilities" -> {
      val client=authenticated[conn]
      if(client==null||!store.read().has(client)){error(conn,"unauthorized");return}
      val values=payload.optJSONArray("features")?:JSONArray()
      peerFeatures[conn]=(0 until minOf(16,values.length())).map { values.optString(it) }.filter { it=="volume_state"||it=="get_snapshot" }.toSet()
     }
     "get_snapshot" -> {
      val client=authenticated[conn]
      if(client==null||!store.read().has(client)){error(conn,"unauthorized");return}
      handler.post {
       if(authenticated[conn]!=client||!store.read().has(client)){error(conn,"unauthorized");return@post}
       val refresh=Hub.refreshMedia;if(refresh!=null)refresh()else sendMediaSnapshot(conn,Hub.media.value)
      }
     }
     "command_request" -> {
      val client=authenticated[conn]
      if(client==null || !store.read().has(client)) { error(conn,"unauthorized");return }
      val requestId=payload.getString("requestId");require(requestId.length<=128)
      val requests=requestIds.getOrPut(client) { ReplayWindow() }
      if(!requests.accept(requestId,SystemClock.elapsedRealtime())) { error(conn,"duplicate_request");return }
      val command=payload.getString("command")
      handler.post {
       if(authenticated[conn]!=client || !store.read().has(client)) { error(conn,"unauthorized");return@post }
       val failure=Hub.control?.invoke(command,payload) ?: if(Hub.control==null) "no_session" else null
       send(conn,"command_result",JSONObject().put("requestId",requestId).put("success",failure==null).apply { if(failure!=null)put("error",failure);if(command=="volume"){put("volume",Hub.media.value.optInt("volume"));put("maxVolume",Hub.media.value.optInt("maxVolume"))} })
      }
     }
     else -> error(conn,"unknown_type")
    }
   } catch(_:Exception) { error(conn,"invalid_message") }
  }
 }
}
class BootReceiver:BroadcastReceiver() {
 override fun onReceive(context:Context,intent:Intent) {
  if(intent.action!=Intent.ACTION_BOOT_COMPLETED)return
  val result=goAsync()
  Hub.scope.launch {
   try {
    val prefs=context.settings.data.first()
    if(prefs[androidx.datastore.preferences.core.booleanPreferencesKey("serviceEnabled")]!=true || prefs[androidx.datastore.preferences.core.booleanPreferencesKey("autoStart")]!=true)return@launch
    try { context.startForegroundService(Intent(context,PassiveLinkService::class.java)) } catch(_:RuntimeException) {
     if(Build.VERSION.SDK_INT>=33 && context.checkSelfPermission(android.Manifest.permission.POST_NOTIFICATIONS)!=android.content.pm.PackageManager.PERMISSION_GRANTED)return@launch
     val nm=context.getSystemService(NotificationManager::class.java);nm.createNotificationChannel(NotificationChannel("recovery",context.getString(R.string.service),NotificationManager.IMPORTANCE_DEFAULT))
     val open=PendingIntent.getActivity(context,0,Intent(context,MainActivity::class.java),PendingIntent.FLAG_IMMUTABLE)
     nm.notify(2,Notification.Builder(context,"recovery").setSmallIcon(android.R.drawable.stat_notify_error).setContentTitle(context.getString(R.string.app_name)).setContentText(context.getString(R.string.restore_service)).setContentIntent(open).setAutoCancel(true).build())
    }
   } finally { result.finish() }
  }
 }
}
