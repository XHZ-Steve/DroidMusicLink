package org.droidmedialink.android
import org.junit.Assert.*
import org.junit.Test
class PairingSasTest {
 @Test(expected=IllegalArgumentException::class) fun rejectsMalformedNonce() { PairingSas.bytes("0011") }
 @Test fun commitmentAndSasBindEveryInput() {
  val a=ByteArray(32){it.toByte()};val b=ByteArray(32){(it+32).toByte()};val f="11".repeat(32)
  assertTrue(PairingSas.matches(a,PairingSas.hex(PairingSas.commitment(a))))
  assertFalse(PairingSas.matches(b,PairingSas.hex(PairingSas.commitment(a))))
  assertEquals("013943",PairingSas.code(f,a,b,"desktop-1"))
  assertNotEquals(PairingSas.code(f,a,b,"desktop-1"),PairingSas.code(f,a,b,"desktop-2"))
  assertNotEquals(PairingSas.code(f,a,b,"desktop-1"),PairingSas.code("22".repeat(32),a,b,"desktop-1"))
 }
}
