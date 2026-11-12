package org.droidmedialink.android
import org.junit.Assert.*
import org.junit.Test
class ProtocolGuardTest {
 @Test fun timestampBoundariesAndOverflow() {
  val now=1_800_000_000_000L
  assertTrue(ProtocolGuard.timestampValid(now,now))
  assertFalse(ProtocolGuard.timestampValid(now-120001,now))
  assertFalse(ProtocolGuard.timestampValid(Long.MIN_VALUE,now))
  assertFalse(ProtocolGuard.timestampValid(Long.MAX_VALUE,now))
 }
 @Test fun volumeRejectsInvalidNumbers() {
  assertTrue(ProtocolGuard.volumeValid(0.5));assertTrue(ProtocolGuard.volumeValid(0.0));assertTrue(ProtocolGuard.volumeValid(1.0))
  listOf(Double.NaN,Double.POSITIVE_INFINITY,-0.01,1.01).forEach { assertFalse(ProtocolGuard.volumeValid(it)) }
 }
 @Test fun replaySurvivesInterveningCommandsAndExpires() {
  val replay=ReplayWindow();assertTrue(replay.accept("first",0))
  repeat(300) { assertTrue(replay.accept("command-$it",it.toLong())) }
  assertFalse(replay.accept("first",1000));assertTrue(replay.accept("first",120001))
 }
}
