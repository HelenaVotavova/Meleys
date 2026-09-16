package cz.meleys.lenkawidget

import android.app.PendingIntent
import android.appwidget.AppWidgetManager
import android.appwidget.AppWidgetProvider
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.text.SpannableString
import android.text.Spanned
import android.text.style.StyleSpan
import android.graphics.Typeface
import android.view.View
import android.widget.RemoteViews
import org.json.JSONArray
import org.json.JSONObject
import java.time.LocalDate
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

class LenkaWidgetProvider : AppWidgetProvider() {
    override fun onUpdate(context: Context, manager: AppWidgetManager, ids: IntArray) = update(context, manager, ids)

    override fun onAppWidgetOptionsChanged(context: Context, manager: AppWidgetManager, id: Int, options: Bundle) {
        update(context, manager, intArrayOf(id))
    }

    override fun onReceive(context: Context, intent: Intent) {
        super.onReceive(context, intent)
        if (intent.action == ACTION_REFRESH) WidgetUpdater.refresh(context)
    }

    private fun update(context: Context, manager: AppWidgetManager, ids: IntArray) {
        val store = SecureStore(context)
        val dashboard = parseDashboard(store.dashboard)
        val child = parseChild(dashboard)
        ids.forEach { id ->
            val view = RemoteViews(context.packageName, R.layout.widget_lenka)
            if (child == null) {
                view.setTextViewText(R.id.widget_date, "Lenčin přehled")
                view.setTextViewText(R.id.widget_message, store.error ?: "Otevři aplikaci a přihlas se.")
                view.setViewVisibility(R.id.widget_message, View.VISIBLE)
                setLessonBoxes(view, null)
                view.setViewVisibility(R.id.widget_changes, View.GONE)
                view.setViewVisibility(R.id.widget_tests, View.GONE)
                view.setViewVisibility(R.id.widget_homework, View.GONE)
                view.setViewVisibility(R.id.widget_meal, View.GONE)
                view.setViewVisibility(R.id.widget_clothing, View.GONE)
            } else {
                view.setTextViewText(R.id.widget_date, formatDate(child.optString("date")))
                view.setViewVisibility(R.id.widget_message, View.GONE)
                setLessonBoxes(view, child.optJSONArray("lessons"))
                setSection(view, R.id.widget_changes, "🔄 Suplování", child.optJSONArray("changes"), ::changeText)
                setSection(view, R.id.widget_tests, "📝 Testy", child.optJSONArray("exams"), ::examText)
                val displayedDate = child.optString("date")
                val pendingHomework = JSONArray(objects(child.optJSONArray("homework")).filter {
                    isCurrentHomework(it, displayedDate) && SecureStore.taskKey(it) !in store.completedTasks
                })
                setSection(view, R.id.widget_homework, "📚 Úkoly", pendingHomework, ::homeworkText)
                setLabelText(view, R.id.widget_meal, "🍽️ Oběd", menuText(dashboard))
                setLabelText(view, R.id.widget_clothing, "🧥 Oblečení", clothingText(dashboard))
                val height = manager.getAppWidgetOptions(id).getInt(AppWidgetManager.OPTION_APPWIDGET_MIN_HEIGHT, 180)
                view.setViewVisibility(R.id.widget_tests, if (height >= 150) View.VISIBLE else View.GONE)
                view.setViewVisibility(R.id.widget_homework, if (height >= 210) View.VISIBLE else View.GONE)
                view.setViewVisibility(R.id.widget_meal, if (height >= 250) View.VISIBLE else View.GONE)
                view.setViewVisibility(R.id.widget_clothing, if (height >= 310) View.VISIBLE else View.GONE)
            }
            view.setTextViewText(R.id.widget_updated, if (store.error == null) "↻ Aktualizovat" else "↻ ${store.error}")
            val refresh = Intent(context, LenkaWidgetProvider::class.java).setAction(ACTION_REFRESH)
            view.setOnClickPendingIntent(R.id.widget_updated, PendingIntent.getBroadcast(context, id, refresh, PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE))
            view.setOnClickPendingIntent(R.id.widget_header, PendingIntent.getActivity(context, id, Intent(context, MainActivity::class.java), PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE))
            manager.updateAppWidget(id, view)
        }
    }

    companion object {
        private const val ACTION_REFRESH = "cz.meleys.lenkawidget.REFRESH"
        fun refreshAll(context: Context) {
            val manager = AppWidgetManager.getInstance(context)
            val ids = manager.getAppWidgetIds(ComponentName(context, LenkaWidgetProvider::class.java))
            LenkaWidgetProvider().update(context, manager, ids)
        }
    }

    private fun parseDashboard(raw: String?): JSONObject? = runCatching { JSONObject(raw ?: return null) }.getOrNull()

    private fun parseChild(dashboard: JSONObject?): JSONObject? = runCatching {
        val children = dashboard?.getJSONArray("children") ?: return null
        (0 until children.length()).map { children.getJSONObject(it) }
            .firstOrNull { it.optString("name").contains("Lenka", true) } ?: children.optJSONObject(0)
    }.getOrNull()

    private fun formatDate(value: String): String = runCatching {
        val date = SimpleDateFormat("yyyy-MM-dd", Locale.ROOT).parse(value) ?: Date()
        SimpleDateFormat("EEEE d. M.", Locale("cs", "CZ")).format(date).replaceFirstChar { it.uppercase() }
    }.getOrDefault(value)

    private fun setLessonBoxes(view: RemoteViews, rows: JSONArray?) {
        val lessonIds = intArrayOf(R.id.lesson_1, R.id.lesson_2, R.id.lesson_3,
            R.id.lesson_4, R.id.lesson_5, R.id.lesson_6)
        val all = objects(rows)
        val clubs = all.filter(::isClub)
        fillLessonBoxes(view, lessonIds, expandLessons(all.filterNot(::isClub)))
        view.setViewVisibility(R.id.widget_clubs, if (clubs.isEmpty()) View.GONE else View.VISIBLE)
        if (clubs.isNotEmpty()) {
            val text = clubs.joinToString("; ") { "${it.optString("start")} ${it.optString("subject")}" }
            setLabelText(view, R.id.widget_clubs, "⭐ Kroužky", text)
        }
    }

    private fun expandLessons(lessons: List<JSONObject>): List<JSONObject> = lessons.flatMap { lesson ->
        val start = timeInMinutes(lesson.optString("start"))
        val end = timeInMinutes(lesson.optString("end"))
        val duration = if (start != null && end != null) end - start else 45
        val periods = when {
            duration >= 125 -> 3
            duration >= 70 -> 2
            else -> 1
        }
        List(periods) { lesson }
    }

    private fun timeInMinutes(value: String): Int? {
        val parts = value.split(":")
        if (parts.size != 2) return null
        return parts[0].toIntOrNull()?.times(60)?.plus(parts[1].toIntOrNull() ?: return null)
    }

    private fun fillLessonBoxes(view: RemoteViews, ids: IntArray, lessons: List<JSONObject>) {
        ids.forEachIndexed { index, id ->
            val lesson = lessons.getOrNull(index)
            view.setViewVisibility(id, View.VISIBLE)
            if (lesson != null) {
                val subject = lesson.optString("subject")
                val text = "${subjectEmoji(subject)}\n${abbreviate(subject)}"
                view.setTextViewText(id, if (lesson.optBoolean("cancelled")) "$text ×" else text)
            } else {
                view.setTextViewText(id, "—")
            }
        }
    }

    private fun isClub(lesson: JSONObject): Boolean {
        val name = lesson.optString("subject").lowercase(Locale("cs", "CZ"))
        val markers = listOf("krouž", "družin", "klub", "keramik", "flétn", "šach", "robot", "dramat")
        return markers.any(name::contains)
    }

    private fun abbreviate(subject: String): String {
        val name = subject.lowercase(Locale("cs", "CZ"))
        return when {
            name.contains("česk") -> "ČJ"
            name.contains("mat") -> "M"
            name.contains("angl") -> "AJ"
            name.contains("přírod") || name.contains("vědou") -> "PŘ"
            name.contains("prvou") -> "PRV"
            name.contains("vlasti") -> "VL"
            name.contains("informat") -> "INF"
            name.contains("těles") || name.contains("brusl") -> "TV"
            name.contains("hudeb") -> "HV"
            name.contains("výtvar") -> "VV"
            name.contains("pracovn") || name.contains("estet") -> "PČ"
            else -> subject.trim().take(4).uppercase(Locale("cs", "CZ"))
        }
    }

    private fun subjectEmoji(subject: String): String {
        val name = subject.lowercase(Locale("cs", "CZ"))
        return when {
            name.contains("česk") -> "📖"
            name.contains("mat") -> "➗"
            name.contains("angl") -> "🇬🇧"
            name.contains("přírod") || name.contains("vědou") || name.contains("prvou") -> "🌿"
            name.contains("vlasti") -> "🗺️"
            name.contains("informat") -> "💻"
            name.contains("těles") || name.contains("brusl") -> "🏃"
            name.contains("hudeb") -> "🎵"
            name.contains("výtvar") || name.contains("pracovn") || name.contains("estet") -> "🎨"
            else -> "📘"
        }
    }

    private fun changeText(row: JSONObject) = "${subjectEmoji(row.optString("subject"))} ${row.optString("start")} ${row.optString("subject")} – zrušeno"
    private fun examText(row: JSONObject) = "${subjectEmoji(row.optString("subject"))} ${row.optString("subject")}: ${row.optString("text")} (${row.optString("date")})"
    private fun homeworkText(row: JSONObject) = "${subjectEmoji(row.optString("subject"))} ${row.optString("subject")}: ${row.optString("text")} (${row.optString("due")})"
    private fun isCurrentHomework(row: JSONObject, displayedDate: String): Boolean {
        if (row.optBoolean("done")) return false
        return runCatching {
            !LocalDate.parse(row.optString("due")).isBefore(LocalDate.parse(displayedDate))
        }.getOrDefault(true)
    }
    private fun objects(rows: JSONArray?) = (0 until (rows?.length() ?: 0)).map { rows!!.getJSONObject(it) }
    private fun menuText(dashboard: JSONObject?): String = objects(dashboard?.optJSONArray("menu"))
        .joinToString("; ") { it.optString("name") }.ifBlank { "zatím není zveřejněn" }

    private fun clothingText(dashboard: JSONObject?): String {
        val clothing = dashboard?.optJSONObject("clothing") ?: return "předpověď není dostupná"
        fun period(key: String, label: String): String {
            val row = clothing.optJSONObject(key) ?: return "$label: bez předpovědi"
            return "$label ${row.optInt("temperature")} °C: ${row.optString("text")}"
        }
        val rain = clothing.optJSONObject("rain")
        val rainText = if (rain == null) {
            "Déšť 7:30–14:00: bez předpovědi"
        } else {
            val expected = if (rain.optBoolean("expected")) "ano" else "ne"
            "Déšť 7:30–14:00: $expected · ${rain.optInt("chance")} % · ${rain.optDouble("amount")} mm"
        }
        return "${period("morning", "Ráno")}\n${period("afternoon", "Odpoledne")}\n$rainText"
    }

    private fun setLabelText(view: RemoteViews, id: Int, title: String, details: String) {
        val text = "$title\n$details"
        val styled = SpannableString(text).apply {
            setSpan(StyleSpan(Typeface.BOLD), 0, title.length, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
        }
        view.setTextViewText(id, styled)
    }
    private fun setSection(view: RemoteViews, id: Int, title: String, rows: JSONArray?, format: (JSONObject) -> String) {
        val limit = if (id == R.id.widget_homework) 12 else 8
        val items = objects(rows)
        if (items.isEmpty() && id != R.id.widget_homework) {
            view.setViewVisibility(id, View.GONE)
            return
        }
        view.setViewVisibility(id, View.VISIBLE)
        val details = items.take(limit).joinToString("\n") { "• ${format(it)}" }
        val text = if (details.isBlank()) "$title: nic nového" else "$title\n$details"
        val styled = SpannableString(text).apply {
            setSpan(StyleSpan(Typeface.BOLD), 0, title.length, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
        }
        view.setTextViewText(id, styled)
    }
}
