plugins { id("com.android.application"); id("org.jetbrains.kotlin.android") }

android {
    namespace = "cz.meleys.lenkawidget"
    compileSdk = 35
    defaultConfig { applicationId = "cz.meleys.lenkawidget"; minSdk = 26; targetSdk = 35; versionCode = 17; versionName = "3.4" }
    compileOptions { sourceCompatibility = JavaVersion.VERSION_17; targetCompatibility = JavaVersion.VERSION_17 }
    kotlinOptions { jvmTarget = "17" }
}

dependencies {
    implementation("androidx.appcompat:appcompat:1.7.0")
    implementation("androidx.work:work-runtime-ktx:2.9.1")
    implementation("androidx.security:security-crypto:1.1.0-alpha06")
    implementation("com.squareup.okhttp3:okhttp:4.12.0")
}
