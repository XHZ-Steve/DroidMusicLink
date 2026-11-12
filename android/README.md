# DroidMediaLink Android

Kotlin / Compose Material 3. Android is a passive WSS server; the application never streams audio or searches for PCs.

## Build

JDK 17, Android SDK 36, AGP 8.13.0, Gradle 8.14.3, Kotlin 2.2.20, Compose BOM 2025.09.01. Minimum API26, target API36.

```powershell
.\gradlew.bat assembleDebug testDebugUnitTest lintDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

Debug builds are test signed. Release signing requires DML_ANDROID_KEYSTORE, DML_ANDROID_STORE_PASSWORD, DML_ANDROID_KEY_ALIAS and DML_ANDROID_KEY_PASSWORD in the build environment.

## Connect

1. Open the application. Recommended settings provides media access, battery settings and the system application settings page. Manufacturer startup options differ; no universal OEM startup API is assumed. Reopen this dialog from Settings.
2. Enable the connection service on Wi-Fi/Ethernet. Select the phone in Windows discovery.
3. Compare the six digits on both screens, then confirm on the phone. Reject a mismatch. The service connects immediately after approval.
4. Revoke a computer in Devices to block its access immediately.

No address, certificate fingerprint or pairing-code entry is presented in the product. NSD advertises `_droidmedialink._tcp.` with stable deviceId/name/pairingVersion=2. WSS listens on the active local interface, port18123.

## Pairing security

Protocol envelopes remain protocolVersion=1; pairing is v2. `pair_begin` contains clientId/clientName/clientCommit=SHA256(clientNonce32). The phone chooses a fresh random serverNonce32 and replies `pair_challenge` with serverCommit. The PC reveals clientNonce in `pair_reveal`; the phone validates the commitment and replies `pair_compare` with serverNonce/deviceId/name. Nonces and commitments use hex. The PC validates serverCommit and independently computes the code; the phone never supplies its code to the PC.

SAS=SHA256(UTF8("DroidMediaLink-SAS-v2") + actual TLS certificate SHA256 raw32 + clientNonce raw32 + serverNonce raw32 + UTF8(clientId)); first four bytes unsigned big-endian modulo1000000, left padded to six digits. Both nonces are committed before either is revealed. The pending approval is bound to this WebSocket and expires120 seconds after pair_begin. Five attempts per host in five minutes. Cancel/connection closure clears the owning dialog. Legacy pair_request is unsupported.

Only explicit phone approval generates a fresh random32-byte token, sends pair_approve and authorizes control. Reconnect requires the saved certificate pin and token on Windows. Tokens are encrypted by Android Keystore AES-GCM; TLS private keys stay in Keystore. UI approval is never bypassed by a discovered identity or fingerprint. Messages are bounded, timestamp checked, rate limited and replay protected. Commands are rechecked for authorization on the media-controller handler.

## Appearance and dependencies

Material3 controls, typography and accessibility remain native Compose components. Optional Liquid Glass uses Kyant0 AndroidLiquidGlass's published Backdrop library, fixed `io.github.kyant0:backdrop:1.0.1`, Apache-2.0. It records the real content behind the navigation surface and uses the library's AGSL RuntimeShader lens refraction plus RenderEffect blur. This is enabled only on API33+, where RuntimeShader exists; older devices render Material surfaces. There is no continuous animation or rendering timer.

Official upstream checked2026-11-11: https://github.com/Kyant0/AndroidLiquidGlass ; docs https://kyant.gitbook.io/backdrop ; Maven metadata https://repo.maven.apache.org/maven2/io/github/kyant0/backdrop/maven-metadata.xml . Latest upstream2.0.1 requires Kotlin2.4.10/Compose1.12/compileSDK37, outside this tested SDK36 toolchain. Fixed1.0.1 is the Compose1.9 compatible line (published POM Kotlin2.2.21, Compose1.9.5). Do not silently replace it with floating latest. License text is bundled as app/src/main/assets/licenses/Apache-2.0.txt; attribution in Backdrop-NOTICE.txt.

Other dependencies: AndroidX/Compose/DataStore Apache-2.0; Java-WebSocket1.6.0 MIT; SLF4J2.0.17 MIT; Lucide1.69.0 ISC/MIT/Feather notices (bundled). No incompatible source was copied.

## Verification and limits

Current build/test/lint output: testing/redesign-final-build.txt. Unit tests include a deterministic cross-platform SAS vector, commitment mismatch and identity/certificate binding, plus existing timestamp/volume/replay guards. APK paths and device checks are recorded in testing/redesign-verification.txt. API30 screenshots verify the Material fallback. Real pairing integration is recorded by the Windows verifier under artifacts/.

Physical Wi-Fi NSD, third-party players, screen-off battery behavior, Bluetooth routing and manufacturer startup restrictions still require real devices. API37 local-network permission is not implemented because target is36. Update checking reads GitHub release metadata; it does not install updates. Settings import/export excludes identities and tokens. Corrupt pairing ciphertext fails closed; clearing app storage revokes saved identities. Certificate expiry after ten years requires re-pairing.
