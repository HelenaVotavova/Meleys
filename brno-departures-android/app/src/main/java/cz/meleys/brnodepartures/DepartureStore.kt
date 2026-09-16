package cz.meleys.brnodepartures

import android.content.Context

class DepartureStore(context: Context) {
    private val prefs = context.getSharedPreferences("departures", Context.MODE_PRIVATE)
    var data: String?
        get() = prefs.getString("data", null)
        set(value) = prefs.edit().putString("data", value).apply()
    var error: String?
        get() = prefs.getString("error", null)
        set(value) = prefs.edit().putString("error", value).apply()
    fun visible(id: String) = prefs.getBoolean("visible_$id", true)
    fun setVisible(id: String, value: Boolean) = prefs.edit().putBoolean("visible_$id", value).apply()
    var windowMinutes: Int
        get() = prefs.getInt("window_minutes", 25)
        set(value) = prefs.edit().putInt("window_minutes", value.coerceIn(5, 60)).apply()
}
