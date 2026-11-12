plugins { id("com.android.application"); id("org.jetbrains.kotlin.android"); id("org.jetbrains.kotlin.plugin.compose") }
android {
 namespace = "org.droidmedialink.android"
 compileSdk = 36
 defaultConfig { applicationId = "org.droidmedialink.android"; minSdk = 26; targetSdk = 36; versionCode = 1; versionName = "0.1.0"; testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner" }
 signingConfigs {
  val keyPath = providers.environmentVariable("DML_ANDROID_KEYSTORE").orNull
  if(keyPath != null) create("release") {
   storeFile = file(keyPath)
   storePassword = providers.environmentVariable("DML_ANDROID_STORE_PASSWORD").orNull
   keyAlias = providers.environmentVariable("DML_ANDROID_KEY_ALIAS").orNull
   keyPassword = providers.environmentVariable("DML_ANDROID_KEY_PASSWORD").orNull
  }
 }
 buildTypes { getByName("release") { signingConfig = signingConfigs.findByName("release") } }
 bundle { language { enableSplit = false } }
 buildFeatures { compose = true; buildConfig = true }
 compileOptions { sourceCompatibility = JavaVersion.VERSION_17; targetCompatibility = JavaVersion.VERSION_17 }

 packaging { resources.excludes += "/META-INF/{AL2.0,LGPL2.1}" }
}
dependencies {
 implementation("io.github.kyant0:backdrop:1.0.1")
 implementation(platform("androidx.compose:compose-bom:2025.09.01"))
 implementation("androidx.activity:activity-compose:1.11.0")
 implementation("androidx.compose.material3:material3")
 implementation("androidx.lifecycle:lifecycle-runtime-compose:2.9.4")
 implementation("androidx.datastore:datastore-preferences:1.1.7")
 implementation("org.java-websocket:Java-WebSocket:1.6.0")
 implementation("org.slf4j:slf4j-nop:2.0.17")
 testImplementation("junit:junit:4.13.2")
 androidTestImplementation("androidx.test:runner:1.6.2")
 androidTestImplementation("androidx.test.ext:junit:1.2.1")
}

kotlin { compilerOptions { jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17) } }
