package org.droidmedialink.android

import android.Manifest
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.PowerManager
import android.provider.Settings
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import androidx.core.content.ContextCompat
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.compose.LocalLifecycleOwner

// Public Android permission state is rechecked on resume. OEM auto-start has no
// reliable grant API and is explicitly confirmed by the user, never inferred.
@Composable
fun PermissionOnboarding(onComplete:()->Unit) {
 val context=LocalContext.current
 val lifecycle=LocalLifecycleOwner.current.lifecycle
 val pager=rememberPagerState(pageCount={4})
 var step by rememberSaveable { mutableIntStateOf(0) }
 var requestAttempted by rememberSaveable { mutableStateOf(false) }
 var startupOpened by rememberSaveable { mutableStateOf(false) }
 var unavailable by remember { mutableStateOf(false) }
 val complete by rememberUpdatedState(onComplete)
 fun granted(index:Int):Boolean=when(index) {
  0->MediaMonitor.hasAccess(context)
  1->Build.VERSION.SDK_INT<33||ContextCompat.checkSelfPermission(context,Manifest.permission.POST_NOTIFICATIONS)==PackageManager.PERMISSION_GRANTED
  2->context.getSystemService(PowerManager::class.java).isIgnoringBatteryOptimizations(context.packageName)
  else->false
 }
 fun advance() { if(step==3)complete() else {step++;unavailable=false} }
 fun refresh() { while(step<3&&granted(step)) {step++;unavailable=false} }
 fun open(intent:Intent,fallback:Intent?=null) {
  unavailable=runCatching{context.startActivity(intent);false}.getOrElse{if(fallback!=null)runCatching{context.startActivity(fallback);false}.getOrDefault(true) else true}
 }
 val notificationPermission=rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { if(granted(1)&&step==1)advance() }
 DisposableEffect(lifecycle) {
  val observer=LifecycleEventObserver { _,event->if(event==Lifecycle.Event.ON_RESUME)refresh() }
  lifecycle.addObserver(observer)
  onDispose { lifecycle.removeObserver(observer) }
 }
 LaunchedEffect(Unit){refresh()}
 LaunchedEffect(step){pager.animateScrollToPage(step)}
 Dialog(onDismissRequest={},properties=DialogProperties(usePlatformDefaultWidth=false,dismissOnBackPress=false,decorFitsSystemWindows=false)) {
  Surface(Modifier.fillMaxSize(),color=MaterialTheme.colorScheme.surface,contentColor=MaterialTheme.colorScheme.onSurface) {
   Column(Modifier.fillMaxSize().safeDrawingPadding().padding(28.dp),horizontalAlignment=Alignment.CenterHorizontally) {
    Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically) {
     Text(stringResource(R.string.recommended_settings),Modifier.weight(1f),style=MaterialTheme.typography.titleMedium)
     Text(stringResource(R.string.guide_progress,step+1,4),color=MaterialTheme.colorScheme.onSurfaceVariant)
    }
    HorizontalPager(state=pager,userScrollEnabled=false,modifier=Modifier.weight(1f)) { index->
     Column(Modifier.fillMaxWidth(),verticalArrangement=Arrangement.spacedBy(24.dp),horizontalAlignment=Alignment.CenterHorizontally) {
      LucideIcon(when(index){0->Lucide.MusicNote;1->Lucide.Notifications;2->Lucide.Wifi;else->Lucide.Settings},null,Modifier.size(64.dp),tint=MaterialTheme.colorScheme.primary)
      Text(stringResource(when(index){0->R.string.media_permission;1->R.string.notification_permission;2->R.string.battery_settings;else->R.string.startup_settings}),style=MaterialTheme.typography.headlineMedium,textAlign=TextAlign.Center)
      Text(stringResource(when(index){0->R.string.guide_media;1->R.string.guide_notifications;2->R.string.guide_battery;else->R.string.startup_help}),style=MaterialTheme.typography.bodyLarge,color=MaterialTheme.colorScheme.onSurfaceVariant,textAlign=TextAlign.Center)
      if(index==3)SettingSwitch(R.string.auto_start,Hub.bool("autoStart"),{Hub.set("autoStart",it)},{open(Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS,Uri.parse("package:"+context.packageName)))})
     }
    }
    if(unavailable)Text(stringResource(R.string.settings_unavailable),color=MaterialTheme.colorScheme.error)
    Row(horizontalArrangement=Arrangement.spacedBy(8.dp),modifier=Modifier.padding(bottom=24.dp)) {
     repeat(4) { index->Box(Modifier.size(if(index==step)8.dp else 6.dp).background(MaterialTheme.colorScheme.primary.copy(alpha=if(index==step)1f else .25f),CircleShape)) }
    }
    Button(modifier=Modifier.fillMaxWidth().heightIn(min=52.dp),onClick={
     when(step) {
      0->if(granted(0))advance() else open(Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS))
      1->if(granted(1))advance() else if(Build.VERSION.SDK_INT>=33) {
       if(requestAttempted)open(Intent(Settings.ACTION_APP_NOTIFICATION_SETTINGS).putExtra(Settings.EXTRA_APP_PACKAGE,context.packageName))
       else {requestAttempted=true;notificationPermission.launch(Manifest.permission.POST_NOTIFICATIONS)}
      }
      2->if(granted(2))advance() else open(Intent(Settings.ACTION_IGNORE_BATTERY_OPTIMIZATION_SETTINGS))
      3->if(startupOpened)complete() else {startupOpened=true;open(Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS,Uri.parse("package:"+context.packageName)))}
     }
    }){Text(stringResource(if(step==3&&startupOpened)R.string.done else R.string.open_settings))}
    TextButton(onClick={advance()},modifier=Modifier.fillMaxWidth()){Text(stringResource(R.string.skip))}
   }
  }
 }
}
