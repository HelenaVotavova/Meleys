package cz.meleys.brnodepartures

import org.json.JSONArray
import org.json.JSONObject

object DepartureFormat {
    fun groups(raw: String?): List<JSONObject> = runCatching {
        val rows = JSONObject(raw ?: return emptyList()).getJSONArray("groups")
        (0 until rows.length()).map { rows.getJSONObject(it) }
    }.getOrDefault(emptyList())

    fun departures(group: JSONObject): List<JSONObject> {
        val rows = group.optJSONArray("departures") ?: JSONArray()
        return (0 until rows.length()).map { rows.getJSONObject(it) }
    }

    fun time(seconds: Int): String {
        val value = ((seconds % 86400) + 86400) % 86400
        return "%02d:%02d".format(value / 3600, value % 3600 / 60)
    }

    fun delay(seconds: Int): String = when {
        seconds >= 60 -> "+${seconds / 60} min"
        seconds <= -60 -> "${seconds / 60} min"
        else -> "včas"
    }

    fun icon(line: String) = if (line in setOf("25", "26", "32")) "🚎" else "🚋"
}
