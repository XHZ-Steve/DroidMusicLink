package org.droidmedialink.android

import android.content.Intent
import android.media.MediaMetadata
import android.media.session.MediaSession
import android.media.session.PlaybackState
import android.os.Handler
import android.os.Looper
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

/** Real framework callbacks; this isolated session never plays audio. */
@RunWith(AndroidJUnit4::class)
class PausedMediaControlTest {
 @Test fun pausedSessionAcceptsBasicCommandsWithoutActionBits() {
  val instrumentation=InstrumentationRegistry.getInstrumentation()
  val context=instrumentation.targetContext
  assertTrue("Grant media access before this integration test",MediaMonitor.hasAccess(context))
  instrumentation.startActivitySync(Intent(context,MainActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
  context.startForegroundService(Intent(context,PassiveLinkService::class.java))
  val received=CountDownLatch(4)
  lateinit var session:MediaSession
  instrumentation.runOnMainSync {
   session=MediaSession(context,"DroidMediaLink paused control TEST")
   session.setCallback(object:MediaSession.Callback() {
    override fun onPlay() { received.countDown() }
    override fun onPause() { received.countDown() }
    override fun onSkipToNext() { received.countDown() }
    override fun onSkipToPrevious() { received.countDown() }
   },Handler(Looper.getMainLooper()))
   session.setMetadata(MediaMetadata.Builder().putString(MediaMetadata.METADATA_KEY_TITLE,"TEST SESSION paused controls").build())
   session.setPlaybackState(PlaybackState.Builder().setActions(0).setState(PlaybackState.STATE_PAUSED,0,0f).build())
   session.isActive=true
  }
  try {
   val deadline=System.nanoTime()+TimeUnit.SECONDS.toNanos(10)
   while(Hub.sources.value.none { it.first==context.packageName } && System.nanoTime()<deadline)Thread.sleep(50)
   instrumentation.waitForIdleSync()
   instrumentation.runOnMainSync {
    Hub.select?.invoke(context.packageName)
    assertEquals("TEST SESSION paused controls",Hub.media.value.optString("title"))
    for(command in listOf("play","pause","next","previous")) {
     assertTrue(Hub.media.value.getJSONArray("commands").toString().contains(command))
     assertNull("Command must reach a paused session without action flags",Hub.control?.invoke(command,org.json.JSONObject()))
    }
   }
   assertTrue("All four Android transport callbacks must arrive",received.await(10,TimeUnit.SECONDS))
  } finally {
   instrumentation.runOnMainSync { session.isActive=false;session.release();Hub.select?.invoke(null) }
  }
 }
}
