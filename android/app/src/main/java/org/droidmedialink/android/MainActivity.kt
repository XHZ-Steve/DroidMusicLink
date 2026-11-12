package org.droidmedialink.android

import com.kyant.backdrop.backdrops.rememberLayerBackdrop
import com.kyant.backdrop.backdrops.layerBackdrop
import com.kyant.backdrop.drawBackdrop
import com.kyant.backdrop.effects.blur
import com.kyant.backdrop.effects.lens
import android.Manifest
import android.content.*
import android.os.Build
import android.provider.Settings
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import android.os.Bundle
import androidx.compose.foundation.*
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.*
import androidx.compose.ui.graphics.*
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.lifecycleScope
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.first
import org.json.JSONObject

class MainActivity:ComponentActivity() {
 override fun attachBaseContext(base:Context) {
  val language=base.getSharedPreferences("locale",0).getString("language","").orEmpty()
  if(language.isEmpty())super.attachBaseContext(base) else {
   val config=android.content.res.Configuration(base.resources.configuration);config.setLocale(java.util.Locale.forLanguageTag(language));super.attachBaseContext(base.createConfigurationContext(config))
  }
 }
 private val exportSettings=registerForActivityResult(ActivityResultContracts.CreateDocument("application/json")) { uri -> if(uri!=null)Hub.scope.launch { runCatching { contentResolver.openOutputStream(uri)?.bufferedWriter()?.use { it.write(SettingsTransfer.export(this@MainActivity)) } }.onFailure { Hub.error.value=getString(R.string.settings_error) } } }
 private val importSettings=registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri -> if(uri!=null)Hub.scope.launch { runCatching { val text=contentResolver.openInputStream(uri)!!.bufferedReader().use { reader -> val out=StringBuilder();val buffer=CharArray(1024);while(out.length<=65536) { val count=reader.read(buffer);if(count<0)break;out.append(buffer,0,count) };require(out.length<=65536);out.toString() };SettingsTransfer.import(this@MainActivity,text);recreate() }.onFailure { Hub.error.value=getString(R.string.settings_error) } } }
 fun exportPreferences()=exportSettings.launch("DroidMediaLink-settings.json")
 fun importPreferences()=importSettings.launch(arrayOf("application/json"))
 fun language(value:String) { getSharedPreferences("locale",0).edit().putString("language",value).apply();Hub.set("language",value);recreate() }
 private val notifications=registerForActivityResult(ActivityResultContracts.RequestPermission()) { }
 override fun onCreate(savedInstanceState:Bundle?) {
  super.onCreate(savedInstanceState)
  setContent { DroidUI() }
  lifecycleScope.launch {
   val saved=settings.data.first()
   if(saved[androidx.datastore.preferences.core.booleanPreferencesKey("serviceEnabled")]==true && !Hub.running.value)
    runCatching { startForegroundService(Intent(this@MainActivity,PassiveLinkService::class.java)) }.onFailure { Hub.error.value=getString(R.string.service_error) }
  }
 }
 fun startLink() {
  if(Build.VERSION.SDK_INT>=33)notifications.launch(Manifest.permission.POST_NOTIFICATIONS)
  runCatching { startForegroundService(Intent(this,PassiveLinkService::class.java)) }.onFailure { Hub.error.value=getString(R.string.service_error) }
 }
 override fun onResume() { super.onResume();MediaMonitor.reconnect(this);Hub.refreshMedia?.invoke() }
}
@OptIn(ExperimentalMaterial3Api::class)
@Composable fun DroidUI() {
 val context=LocalContext.current
 val prefs = Hub.preferences.collectAsStateWithLifecycle().value
 val running by Hub.running.collectAsStateWithLifecycle()
 val listenerEnabled by Hub.mediaAccess.collectAsStateWithLifecycle()
 val error by Hub.error.collectAsStateWithLifecycle()
 val media by Hub.media.collectAsStateWithLifecycle()
 val sources by Hub.sources.collectAsStateWithLifecycle()
 val lastSeen by Hub.lastSeen.collectAsStateWithLifecycle()
 val peers by Hub.peers.collectAsStateWithLifecycle()
 val connected by Hub.connected.collectAsStateWithLifecycle()
 val pending by Hub.pending.collectAsStateWithLifecycle()
 var page by remember { mutableIntStateOf(0) }
 var confirmRevoke by remember { mutableStateOf<Pair<String,String>?>(null) }
 var help by remember { mutableStateOf<Int?>(null) }
 var recommendations by remember { mutableStateOf(!context.getSharedPreferences("onboarding",0).getBoolean("shown",false)) }
 var update by remember { mutableStateOf("") }
 var checking by remember { mutableStateOf(false) }
 val scope=rememberCoroutineScope()
 val dark=when(prefs[androidx.datastore.preferences.core.stringPreferencesKey("theme")]) { "dark"->true;"light"->false;else->isSystemInDarkTheme() }
 val scheme=if(Build.VERSION.SDK_INT>=31) { if(dark)dynamicDarkColorScheme(context) else dynamicLightColorScheme(context) } else if(dark)darkColorScheme(primary=Color(0xffbccbbd),primaryContainer=Color(0xff304b3b),secondaryContainer=Color(0xff34453a),surface=Color(0xff151c1b),surfaceContainer=Color(0xff1b2420)) else lightColorScheme(primary=Color(0xff486655),primaryContainer=Color(0xffdce8dc),secondaryContainer=Color(0xffe2e8df),onSecondaryContainer=Color(0xff202b22),surface=Color(0xfff4f7f1),surfaceContainer=Color(0xffedf0e9))
 val glass=Hub.bool("liquidGlass") && Build.VERSION.SDK_INT>=33
 val backdrop=rememberLayerBackdrop()
 MaterialTheme(colorScheme=scheme) {
  Box(Modifier.fillMaxSize()) {
   Box(Modifier.fillMaxSize().background(Brush.linearGradient(listOf(scheme.primaryContainer,scheme.surface,scheme.secondaryContainer))))
  Scaffold(containerColor=Color.Transparent) { padding->
   Column(Modifier.fillMaxSize().then(if(glass) Modifier.layerBackdrop(backdrop) else Modifier).padding(padding).background(Brush.verticalGradient(listOf(scheme.primaryContainer.copy(alpha=.45f),scheme.surface,scheme.surface))).verticalScroll(remember(page) { ScrollState(0) }).padding(horizontal=24.dp).padding(top=24.dp,bottom=130.dp),verticalArrangement=Arrangement.spacedBy(20.dp)) {
    Text(stringResource(when(page){1->R.string.devices;2->R.string.settings;else->R.string.app_name}),style=MaterialTheme.typography.headlineLarge,fontWeight=FontWeight.SemiBold)
    if(error.isNotEmpty() && page!=0)Text(error,color=scheme.error)
    if(page==0) {
     Row(Modifier.fillMaxWidth().padding(vertical=12.dp),verticalAlignment=Alignment.CenterVertically) {
      LucideIcon(Lucide.Wifi,null,Modifier.size(32.dp),tint=scheme.primary)
      Column(Modifier.weight(1f).padding(horizontal=16.dp)) { Text(stringResource(R.string.service),style=MaterialTheme.typography.titleMedium);Text(stringResource(if(running)R.string.running else R.string.stopped),color=scheme.onSurfaceVariant) }
      Switch(running,onCheckedChange={ if(it)(context as MainActivity).startLink() else { Hub.set("serviceEnabled",false);context.stopService(Intent(context,PassiveLinkService::class.java)) } })
     }
     if(error.isNotEmpty()) Text(error,color=scheme.error)
     if(!listenerEnabled) FilledTonalButton(onClick={context.startActivity(Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS))},modifier=Modifier.fillMaxWidth()) { LucideIcon(Lucide.Notifications,null);Spacer(Modifier.width(10.dp));Text(stringResource(R.string.grant_media)) }
     HorizontalDivider(color=scheme.outlineVariant.copy(alpha=.5f))
     MediaPane(media)
     if(sources.size>1) {
      Text(stringResource(R.string.source),style=MaterialTheme.typography.titleSmall)
      FlowRow(horizontalArrangement=Arrangement.spacedBy(8.dp)) {
       FilterChip(selected=Hub.string("manual").isEmpty(),onClick={Hub.set("manual","");Hub.select?.invoke(null)},label={Text(stringResource(R.string.automatic))})
       sources.forEach { (id,name)->FilterChip(selected=Hub.string("manual")==id,onClick={Hub.set("manual",id);Hub.select?.invoke(id)},label={Text(name)}) }
      }
     }
     Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically) { Text(stringResource(R.string.connected_computers),Modifier.weight(1f),style=MaterialTheme.typography.titleMedium);Text(connected.size.toString(),style=MaterialTheme.typography.headlineSmall,color=scheme.primary) }
     peers.filter { it.first in connected }.forEach { Text(it.second) }
     if(connected.isEmpty()) OutlinedButton(onClick={page=1}) { Text(stringResource(R.string.pair_computer)) }
    }
    if(page==1) {
     if(running) {
      Row(verticalAlignment=Alignment.CenterVertically,horizontalArrangement=Arrangement.spacedBy(14.dp)) {
       LucideIcon(Lucide.Wifi,null,Modifier.size(32.dp),tint=scheme.primary)
       Text(stringResource(R.string.ready_to_pair),style=MaterialTheme.typography.titleMedium)
      }
     } else Button(onClick={(context as MainActivity).startLink()}) { Text(stringResource(R.string.start)) }
     HorizontalDivider()
     peers.forEach { peer-> Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically) {
      LucideIcon(Lucide.Computer,null);Column(Modifier.weight(1f).padding(16.dp)) { Text(peer.second,style=MaterialTheme.typography.titleMedium);Text(if(peer.first in connected)stringResource(R.string.connected) else lastSeen[peer.first]?.takeIf { it>0 }?.let { java.text.DateFormat.getDateTimeInstance(java.text.DateFormat.SHORT,java.text.DateFormat.SHORT).format(java.util.Date(it)) } ?: stringResource(R.string.authorized),style=MaterialTheme.typography.bodySmall,color=scheme.onSurfaceVariant) }
      IconButton(onClick={confirmRevoke=peer}) { LucideIcon(Lucide.LinkOff,stringResource(R.string.revoke)) }
     } }
    }
    if(page==2) {
     TextButton(onClick={recommendations=true}) { Text(stringResource(R.string.recommended_settings)) }

      SettingSwitch(R.string.auto_start,Hub.bool("autoStart"),{Hub.set("autoStart",it)},{help=R.string.background_help})
      TextButton(onClick={context.startActivity(Intent(Settings.ACTION_IGNORE_BATTERY_OPTIMIZATION_SETTINGS))}) { Text(stringResource(R.string.battery_settings)) }
     
     SettingSwitch(R.string.liquid_glass,Build.VERSION.SDK_INT>=33 && Hub.bool("liquidGlass"),{Hub.set("liquidGlass",it)},{help=R.string.liquid_glass_help},enabled=Build.VERSION.SDK_INT>=33)

      Text(stringResource(R.string.theme),style=MaterialTheme.typography.titleMedium)
      FlowRow(horizontalArrangement=Arrangement.spacedBy(8.dp)) { listOf("" to R.string.system,"light" to R.string.light,"dark" to R.string.dark).forEach { (value,label)->FilterChip(Hub.string("theme")==value,{Hub.set("theme",value)},label={Text(stringResource(label))}) } }
     

      Text(stringResource(R.string.language),style=MaterialTheme.typography.titleMedium)
      FlowRow(horizontalArrangement=Arrangement.spacedBy(8.dp)) { listOf("" to R.string.system,"en" to R.string.english,"zh-CN" to R.string.chinese).forEach { (value,label)->FilterChip(Hub.string("language")==value,{(context as MainActivity).language(value)},label={Text(stringResource(label))}) } }
     

      OutlinedTextField(Hub.string("preferred"),{Hub.set("preferred",it)},label={Text(stringResource(R.string.preferred_app))},modifier=Modifier.fillMaxWidth(),singleLine=true)
      OutlinedTextField(Hub.string("excluded"),{Hub.set("excluded",it)},label={Text(stringResource(R.string.excluded_apps))},modifier=Modifier.fillMaxWidth(),singleLine=true)
     
     TextButton(onClick={context.startActivity(Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS))}) { Text(stringResource(R.string.media_permission)) }

      TextButton(enabled=!checking,onClick={checking=true;scope.launch {
       update=withContext(Dispatchers.IO) { try {
        val connection=java.net.URL("https://api.github.com/repos/XHZ-Steve/DroidMusicLink/releases/latest").openConnection() as javax.net.ssl.HttpsURLConnection
        connection.connectTimeout=10000;connection.readTimeout=10000;connection.setRequestProperty("Accept","application/vnd.github+json");connection.setRequestProperty("User-Agent","DroidMediaLink/"+BuildConfig.VERSION_NAME)
        try { if(connection.responseCode==404) context.getString(R.string.no_release) else { val json=JSONObject(connection.inputStream.bufferedReader().readText());json.getString("tag_name")+"\n"+json.optString("body").take(2000) } } finally { connection.disconnect() }
       } catch(_:Exception) { context.getString(R.string.update_error) } }
       checking=false
      }}) { Text(stringResource(R.string.check_updates)) }
      if(update.isNotEmpty())Text(update,style=MaterialTheme.typography.bodySmall)
     
     TextButton(onClick={help=R.string.privacy_help}) { Text(stringResource(R.string.privacy)) }

      FlowRow { TextButton(onClick={(context as MainActivity).exportPreferences()}) { Text(stringResource(R.string.export_settings)) };TextButton(onClick={(context as MainActivity).importPreferences()}) { Text(stringResource(R.string.import_settings)) };TextButton(onClick={scope.launch { SettingsTransfer.reset(context);(context as MainActivity).recreate() }}) { Text(stringResource(R.string.reset_settings)) } }
     
     Text("DroidMediaLink "+BuildConfig.VERSION_NAME,style=MaterialTheme.typography.labelMedium,color=scheme.onSurfaceVariant)
    }
   }
  }
  NavigationBar(modifier=if(glass) Modifier.align(Alignment.BottomCenter).padding(horizontal=16.dp,vertical=8.dp).drawBackdrop(backdrop=backdrop,shape={RoundedCornerShape(32.dp)},effects={blur(4.dp.toPx());lens(16.dp.toPx(),28.dp.toPx())},onDrawSurface={drawRect(scheme.surface.copy(alpha=.4f))}) else Modifier.align(Alignment.BottomCenter),containerColor=if(glass) Color.Transparent else scheme.surfaceContainer) {
   listOf(R.string.home to Lucide.Home,R.string.devices to Lucide.Computer,R.string.settings to Lucide.Settings).forEachIndexed { i,(text,icon)-> NavigationBarItem(selected=page==i,onClick={page=i},icon={LucideIcon(icon,stringResource(text))},label={Text(stringResource(text))}) }
  }
  }
  if(recommendations) AlertDialog(onDismissRequest={recommendations=false;context.getSharedPreferences("onboarding",0).edit().putBoolean("shown",true).apply()},title={Text(stringResource(R.string.recommended_settings))},text={Column {
   TextButton(onClick={context.startActivity(Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS))}) { Text(stringResource(R.string.media_permission)) }
   TextButton(onClick={context.startActivity(Intent(Settings.ACTION_IGNORE_BATTERY_OPTIMIZATION_SETTINGS))}) { Text(stringResource(R.string.battery_settings)) }
   TextButton(onClick={context.startActivity(Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS,android.net.Uri.parse("package:"+context.packageName)))}) { Text(stringResource(R.string.startup_settings)) }
   Text(stringResource(R.string.startup_help),style=MaterialTheme.typography.bodySmall)
  }},confirmButton={TextButton(onClick={recommendations=false;context.getSharedPreferences("onboarding",0).edit().putBoolean("shown",true).apply()}) { Text(stringResource(R.string.done)) }})
  pending?.let { request->AlertDialog(onDismissRequest={request.approve(false)},icon={LucideIcon(Lucide.Computer,null)},title={Text(stringResource(R.string.approve_pairing))},text={Column(verticalArrangement=Arrangement.spacedBy(16.dp)) { Text(request.name);Text(request.sas,style=MaterialTheme.typography.displayMedium,fontWeight=FontWeight.Medium);Text(stringResource(R.string.compare_sas)) }},confirmButton={TextButton(onClick={request.approve(true)}) { Text(stringResource(R.string.codes_match)) }},dismissButton={TextButton(onClick={request.approve(false)}) { Text(stringResource(R.string.reject)) }}) }
  confirmRevoke?.let { peer->AlertDialog(onDismissRequest={confirmRevoke=null},title={Text(stringResource(R.string.revoke))},text={Text(stringResource(R.string.revoke_warning,peer.second))},confirmButton={TextButton(onClick={PairingStore(context).revoke(peer.first);confirmRevoke=null}) { Text(stringResource(R.string.revoke)) }},dismissButton={TextButton(onClick={confirmRevoke=null}) { Text(stringResource(R.string.cancel)) }}) }
  help?.let { text->AlertDialog(onDismissRequest={help=null},text={Text(stringResource(text))},confirmButton={TextButton(onClick={help=null}) { Text(stringResource(R.string.close)) }}) }
 }
}
@Composable fun SettingSwitch(title:Int,value:Boolean,onChange:(Boolean)->Unit,onHelp:()->Unit,enabled:Boolean=true) {
 Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically) { Text(stringResource(title),Modifier.weight(1f));IconButton(onClick=onHelp) { LucideIcon(Lucide.HelpOutline,stringResource(R.string.help)) };Switch(value,onChange,enabled=enabled) }
}
@Composable fun MediaPane(media:JSONObject) {
 val artwork=media.optString("artwork")
 val bitmap=remember(artwork) { runCatching { val data=android.util.Base64.decode(artwork,android.util.Base64.DEFAULT);android.graphics.BitmapFactory.decodeByteArray(data,0,data.size)?.asImageBitmap() }.getOrNull() }
 Row(Modifier.fillMaxWidth().padding(vertical=12.dp),verticalAlignment=Alignment.CenterVertically) {
  Surface(shape=RoundedCornerShape(24.dp),color=MaterialTheme.colorScheme.secondaryContainer,modifier=Modifier.size(112.dp)) {
   if(bitmap!=null) Image(bitmap,null,contentScale=androidx.compose.ui.layout.ContentScale.Crop) else Box(contentAlignment=Alignment.Center) { LucideIcon(Lucide.MusicNote,null,Modifier.size(42.dp)) }
  }
  Column(Modifier.weight(1f).padding(start=20.dp),verticalArrangement=Arrangement.spacedBy(8.dp)) {
   Text(media.optString("title").ifEmpty { stringResource(R.string.no_media) },style=MaterialTheme.typography.titleLarge,maxLines=2,overflow=TextOverflow.Ellipsis)
   if(media.optString("artist").isNotEmpty()) Text(media.optString("artist"),maxLines=2,overflow=TextOverflow.Ellipsis,style=MaterialTheme.typography.bodyMedium,color=MaterialTheme.colorScheme.onSurfaceVariant)
   if(media.optString("appName").isNotEmpty()) Text(media.optString("appName"),maxLines=1,overflow=TextOverflow.Ellipsis,style=MaterialTheme.typography.labelMedium,color=MaterialTheme.colorScheme.primary)
  }
 }
}
