package cz.meleys.lenkawidget

import android.content.Context
import androidx.security.crypto.EncryptedSharedPreferences
import androidx.security.crypto.MasterKey

class SecureStore(context: Context) {
    private val prefs = EncryptedSharedPreferences.create(
        context, "private_session", MasterKey.Builder(context).setKeyScheme(MasterKey.KeyScheme.AES256_GCM).build(),
        EncryptedSharedPreferences.PrefKeyEncryptionScheme.AES256_SIV,
        EncryptedSharedPreferences.PrefValueEncryptionScheme.AES256_GCM
    )
    var token: String?
        get() = prefs.getString("token", null)
        set(value) = prefs.edit().putString("token", value).apply()
    var dashboard: String?
        get() = prefs.getString("dashboard", null)
        set(value) = prefs.edit().putString("dashboard", value).apply()
    var error: String?
        get() = prefs.getString("error", null)
        set(value) = prefs.edit().putString("error", value).apply()
    fun clear() = prefs.edit().clear().apply()
}
