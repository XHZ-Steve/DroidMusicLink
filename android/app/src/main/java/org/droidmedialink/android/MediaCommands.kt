package org.droidmedialink.android

import android.view.KeyEvent

object MediaCommands {
 val basic=listOf("play","pause","toggle","previous","next")
 fun keyCode(command:String):Int?=when(command) {
  "play"->KeyEvent.KEYCODE_MEDIA_PLAY
  "pause"->KeyEvent.KEYCODE_MEDIA_PAUSE
  "toggle"->KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE
  "previous"->KeyEvent.KEYCODE_MEDIA_PREVIOUS
  "next"->KeyEvent.KEYCODE_MEDIA_NEXT
  else->null
 }
}
