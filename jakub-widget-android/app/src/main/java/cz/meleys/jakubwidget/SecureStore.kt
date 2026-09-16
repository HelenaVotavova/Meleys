package cz.meleys.jakubwidget

import android.content.Context
import androidx.security.crypto.EncryptedSharedPreferences
import androidx.security.crypto.MasterKey
import org.json.JSONObject

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
    val completedTasks: Set<String>
        get() = prefs.getStringSet("completed_tasks", emptySet())?.toSet() ?: emptySet()
    fun setTaskCompleted(task: JSONObject, completed: Boolean) {
        val values = completedTasks.toMutableSet()
        if (completed) values.add(taskKey(task)) else values.remove(taskKey(task))
        prefs.edit().putStringSet("completed_tasks", values).apply()
    }
    fun clear() = prefs.edit().remove("token").remove("dashboard").remove("error").apply()

    companion object {
        fun taskKey(task: JSONObject): String {
            val id = task.optString("id")
            return if (id.isNotBlank() && id != "null") id else listOf(
                task.optString("due"), task.optString("subject"), task.optString("text")
            ).joinToString("|")
        }
    }
}
