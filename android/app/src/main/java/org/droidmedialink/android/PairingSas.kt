package org.droidmedialink.android

import java.security.MessageDigest

/** Commit both random nonces before revealing either; compare on both screens before approval. */
object PairingSas {
 fun hex(bytes: ByteArray) = bytes.joinToString("") { "%02x".format(it) }
 fun bytes(hex: String): ByteArray { require(hex.matches(Regex("[a-fA-F0-9]{64}"))); return hex.chunked(2).map { it.toInt(16).toByte() }.toByteArray() }
 fun commitment(nonce: ByteArray) = MessageDigest.getInstance("SHA-256").digest(nonce)
 fun matches(nonce: ByteArray, commitment: String) = MessageDigest.isEqual(commitment(nonce),bytes(commitment))
 fun code(fingerprint: String, clientNonce: ByteArray, serverNonce: ByteArray, clientId: String): String {
  require(clientNonce.size==32 && serverNonce.size==32)
  val hash=commitment("DroidMediaLink-SAS-v2".toByteArray(Charsets.UTF_8)+bytes(fingerprint)+clientNonce+serverNonce+clientId.toByteArray(Charsets.UTF_8))
  val value=hash.take(4).fold(0L) { n,b -> (n shl 8) or (b.toLong() and 255) }
  return (value%1000000).toString().padStart(6,'0')
 }
}
