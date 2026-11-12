package org.droidmedialink.android
import android.media.MediaMetadata
import android.media.session.MediaSession
import android.media.session.PlaybackState
import android.os.Handler
import android.os.Looper
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Test
import org.junit.Assert.*
import org.junit.runner.RunWith
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

/** Isolated Android instrumentation fixture. Never included in the production APK. No audio stream. */
@RunWith(AndroidJUnit4::class)
class MediaSessionFixtureTest {
 @Test fun realMediaSessionReceivesRemoteControls() {
  val instrumentation=InstrumentationRegistry.getInstrumentation()
  val context=instrumentation.targetContext
  instrumentation.startActivitySync(android.content.Intent(context,MainActivity::class.java).addFlags(android.content.Intent.FLAG_ACTIVITY_NEW_TASK))
  context.startForegroundService(android.content.Intent(context,PassiveLinkService::class.java))
  val callbacks=CountDownLatch(3)
  var playing=false
  var position=0L
  var receivedPlay=false;var receivedPause=false;var receivedSeek=false
  lateinit var session:MediaSession
  instrumentation.runOnMainSync {
   session=MediaSession(context,"DroidMediaLink isolated integration test")
   fun publish() {
    session.setPlaybackState(PlaybackState.Builder().setActions(PlaybackState.ACTION_PLAY or PlaybackState.ACTION_PAUSE or PlaybackState.ACTION_PLAY_PAUSE or PlaybackState.ACTION_SEEK_TO)
     .setState(if(playing)PlaybackState.STATE_PLAYING else PlaybackState.STATE_PAUSED,position,if(playing)1f else 0f).build())
   }
   session.setCallback(object:MediaSession.Callback() {
    override fun onPlay() { playing=true;publish();if(!receivedPlay) { receivedPlay=true;callbacks.countDown() } }
    override fun onPause() { playing=false;publish();if(!receivedPause) { receivedPause=true;callbacks.countDown() } }
    override fun onSeekTo(pos:Long) { position=pos;publish();if(pos==42000L && !receivedSeek) { receivedSeek=true;callbacks.countDown() } }
   },Handler(Looper.getMainLooper()))
   session.setMetadata(MediaMetadata.Builder().putString(MediaMetadata.METADATA_KEY_TITLE,"TEST SESSION").putString(MediaMetadata.METADATA_KEY_ARTIST,"Isolated instrumentation fixture").putString(MediaMetadata.METADATA_KEY_ALBUM,"No audio stream").putLong(MediaMetadata.METADATA_KEY_DURATION,180000).build())
   publish();session.isActive=true
  }
  try {
   assertTrue("Remote play, pause and seek(42000) must reach the Android MediaSession within 120s",callbacks.await(120,TimeUnit.SECONDS))
   assertTrue(receivedPlay);assertTrue(receivedPause);assertTrue(receivedSeek)
   // Opt-in inspection window; standard test runs finish as soon as assertions pass.
   val holdSeconds=InstrumentationRegistry.getArguments().getString("holdSeconds")?.toLongOrNull()?.coerceIn(0,600) ?: 0
   Thread.sleep(holdSeconds*1000)
  } finally { instrumentation.runOnMainSync { session.isActive=false;session.release() } }
 }
}
