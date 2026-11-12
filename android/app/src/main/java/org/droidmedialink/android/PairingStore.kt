package org.droidmedialink.android

import android.content.Context
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import android.util.Base64
import org.json.JSONObject
import java.math.BigInteger
import java.security.*
import java.util.Date
import javax.crypto.*
import javax.crypto.spec.GCMParameterSpec
import javax.net.ssl.*
import javax.security.auth.x500.X500Principal

class PairingStore(private val context: Context) {
 private val keys = KeyStore.getInstance("AndroidKeyStore").apply { load(null) }
 private val file = java.io.File(context.filesDir, "paired.enc")
 private val random = SecureRandom()
 init {
  if (!keys.containsAlias("identity-v2")) {
   KeyPairGenerator.getInstance("RSA", "AndroidKeyStore").apply {
    initialize(KeyGenParameterSpec.Builder("identity-v2", KeyProperties.PURPOSE_SIGN or KeyProperties.PURPOSE_VERIFY or KeyProperties.PURPOSE_DECRYPT )
     .setKeySize(2048).setDigests(KeyProperties.DIGEST_NONE, KeyProperties.DIGEST_SHA256, KeyProperties.DIGEST_SHA384, KeyProperties.DIGEST_SHA512)
     .setSignaturePaddings(KeyProperties.SIGNATURE_PADDING_RSA_PKCS1, KeyProperties.SIGNATURE_PADDING_RSA_PSS)
     .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE, KeyProperties.ENCRYPTION_PADDING_RSA_PKCS1, KeyProperties.ENCRYPTION_PADDING_RSA_OAEP)
     .setCertificateSubject(X500Principal("CN=DroidMediaLink"))
     .setCertificateSerialNumber(BigInteger(128, random)).setCertificateNotBefore(Date(System.currentTimeMillis()-86400000))
     .setCertificateNotAfter(Date(System.currentTimeMillis()+10L*365*86400000)).build())
   }.generateKeyPair()
  }
  if (!keys.containsAlias("pairing")) KeyGenerator.getInstance("AES", "AndroidKeyStore").apply {
   init(KeyGenParameterSpec.Builder("pairing", KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT).setBlockModes(KeyProperties.BLOCK_MODE_GCM).setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE).build())
  }.generateKey()
 }
 val fingerprint: String get() = MessageDigest.getInstance("SHA-256").digest(keys.getCertificate("identity-v2").encoded).joinToString("") { "%02X".format(it) }
 fun tls(): SSLContext {
  // A one-entry keystore prevents an unrelated AndroidKeyStore alias being selected for TLS.
  val manager = object : X509ExtendedKeyManager() {
   override fun getClientAliases(k: String?, i: Array<java.security.Principal>?) = null
   override fun chooseClientAlias(k: Array<String>?, i: Array<java.security.Principal>?, s: java.net.Socket?) = null
   override fun getServerAliases(k: String?, i: Array<java.security.Principal>?) = if(k == "RSA") arrayOf("identity-v2") else null
   override fun chooseServerAlias(k: String?, i: Array<java.security.Principal>?, s: java.net.Socket?) = if(k == "RSA") "identity-v2" else null
   override fun chooseEngineServerAlias(k: String?, i: Array<java.security.Principal>?, e: SSLEngine?) = if(k == "RSA") "identity-v2" else null
   override fun getCertificateChain(a: String?) = arrayOf(keys.getCertificate("identity-v2") as java.security.cert.X509Certificate)
   override fun getPrivateKey(a: String?) = keys.getKey("identity-v2", null) as PrivateKey
  }
  return SSLContext.getInstance("TLSv1.2").apply { init(arrayOf(manager), null, random) }
 }
 @Synchronized fun read(): JSONObject {
  if (!file.exists()) return JSONObject()
  val bytes = file.readBytes()
  require(bytes.size >= 28)
  return JSONObject(String(Cipher.getInstance("AES/GCM/NoPadding").run { init(Cipher.DECRYPT_MODE, keys.getKey("pairing",null), GCMParameterSpec(128,bytes.copyOfRange(0,12))); doFinal(bytes.copyOfRange(12,bytes.size)) }, Charsets.UTF_8))
 }
 @Synchronized private fun write(data: JSONObject) {
  val cipher = Cipher.getInstance("AES/GCM/NoPadding").apply { init(Cipher.ENCRYPT_MODE,keys.getKey("pairing",null)) }
  val temp = java.io.File(file.path+".tmp"); temp.writeBytes(cipher.iv+cipher.doFinal(data.toString().toByteArray()))
  check(temp.renameTo(file))
  refresh()
 }
 fun refresh() { val data=read(); Hub.peers.value = data.keys().asSequence().map { it to data.getJSONObject(it).getString("name") }.toList();Hub.lastSeen.value=data.keys().asSequence().associateWith { data.getJSONObject(it).optLong("lastSeen") } }
 @Synchronized fun touch(id:String) { val data=read();data.optJSONObject(id)?.put("lastSeen",System.currentTimeMillis());write(data) }
 @Synchronized fun add(id: String, name: String): String {
  val token = Base64.encodeToString(ByteArray(32).also { random.nextBytes(it) }, Base64.NO_WRAP)
  Hub.revoke?.invoke(id)
  write(read().put(id,JSONObject().put("name",name).put("token",token).put("lastSeen",System.currentTimeMillis())))
  return token
 }
 @Synchronized fun authenticate(id: String, token: String): Boolean {
  val expected = read().optJSONObject(id)?.optString("token") ?: return false
  return token.length <= 128 && MessageDigest.isEqual(expected.toByteArray(),token.toByteArray())
 }
 @Synchronized fun revoke(id: String) { write(read().apply { remove(id) }); Hub.revoke?.invoke(id) }
}

