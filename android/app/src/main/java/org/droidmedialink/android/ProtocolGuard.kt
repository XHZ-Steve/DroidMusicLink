package org.droidmedialink.android

object ProtocolGuard {
 fun timestampValid(timestamp: Long, now: Long) = timestamp >= now - 120_000 && timestamp <= now + 120_000
 fun volumeValid(volume: Double) = volume.isFinite() && volume in 0.0..1.0
}
class ReplayWindow {
 private val entries = linkedMapOf<String,Long>()
 @Synchronized fun accept(id: String, now: Long): Boolean {
  entries.entries.removeAll { now - it.value > 120_000 }
  if(id.isEmpty() || id.length>128 || entries.containsKey(id) || entries.size>=4096) return false
  entries[id]=now
  return true
 }
}
