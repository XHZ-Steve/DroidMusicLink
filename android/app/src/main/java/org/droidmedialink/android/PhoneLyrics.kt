package org.droidmedialink.android

import android.media.MediaMetadata
import android.os.Bundle
import org.json.JSONObject

// Only player-published metadata is read. No notification text, private files,
// reflection, accessibility scraping, or third-party network access.
internal object PhoneLyrics {
 private const val limit=65536
 private val keyPattern=Regex("(?:^|[._])(?:lrc|lyrics|synced_lyrics|unsynced_lyrics)$",RegexOption.IGNORE_CASE)
 private val timeTag=Regex("\\[\\d{1,3}:\\d{2}(?:[.:]\\d{1,3})?]")
 private var previous:String?=null
 private var decoded=""
 private var translation=""
 private var songName=""
 private var artist=""
 private fun validate(text:String):String {
  if(text.isBlank()||text.length>limit||text.toByteArray(Charsets.UTF_8).size>limit)return ""
  if(text.any{it<' '&&it!='\n'&&it!='\r'&&it!='\t'})return ""
  return if(timeTag.findAll(text).take(2).count()>=2||text.lineSequence().count{it.isNotBlank()}>=3)text else ""
 }
 private fun decode(raw:String):String {
  if(raw.length>limit)return ""
  if(previous==raw)return decoded
  previous=raw;songName="";artist="";translation=""
  decoded=if(raw.trimStart().startsWith("{"))runCatching {
   val json=JSONObject(raw)
   songName=json.optString("songName");artist=json.optString("artist");translation=validate(json.optString("translationLyric"))
   // Documented player-owned lyricInfo payload; absence is normal.
   if(json.optBoolean("noLyric",false)) "" else validate(json.optString("rawLyric")).ifEmpty{validate(json.optString("lyric"))}
  }.getOrDefault("") else validate(raw)
  return decoded
 }
 private fun matching(text:String,metadata:MediaMetadata?):String {
  val title=metadata?.getString(MediaMetadata.METADATA_KEY_TITLE).orEmpty()
  val singer=metadata?.getString(MediaMetadata.METADATA_KEY_ARTIST).orEmpty()
  if(songName.isNotBlank()&&title.isNotBlank()&&!songName.trim().equals(title.trim(),true))return ""
  if(artist.isNotBlank()&&singer.isNotBlank()&&!artist.trim().equals(singer.trim(),true))return ""
  return text
 }
 fun translated(original:String):String=if(original.isNotEmpty()&&original.toByteArray(Charsets.UTF_8).size+translation.toByteArray(Charsets.UTF_8).size<=limit)translation else ""
 fun read(metadata:MediaMetadata?,extras:Bundle?):String {
  val keys=runCatching{metadata?.keySet().orEmpty().filter{it=="lyricInfo"||keyPattern.containsMatchIn(it)}.take(8)}.getOrDefault(emptyList())
  for(key in keys){val raw=runCatching{metadata?.getText(key)?.toString().orEmpty()}.getOrDefault("");matching(decode(raw),metadata).takeIf{it.isNotEmpty()}?.let{return it}}
  val extraKeys=runCatching{extras?.keySet().orEmpty().filter{it=="lyricInfo"||keyPattern.containsMatchIn(it)}.take(8)}.getOrDefault(emptyList())
  for(key in extraKeys){val raw=runCatching{extras?.getCharSequence(key)?.toString().orEmpty()}.getOrDefault("");matching(decode(raw),metadata).takeIf{it.isNotEmpty()}?.let{return it}}
  return ""
 }
}
