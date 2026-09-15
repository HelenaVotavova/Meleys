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
        val child = parseChild(store.dashboard)
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
            } else {
                view.setTextViewText(R.id.widget_date, formatDate(child.optString("date")))
                view.setViewVisibility(R.id.widget_message, View.GONE)
                setLessonBoxes(view, child.optJSONArray("lessons"))
                setSection(view, R.id.widget_changes, "Suplování", child.optJSONArray("changes"), ::changeText)
                setSection(view, R.id.widget_tests, "Testy", child.optJSONArray("exams"), ::examText)
                setSection(view, R.id.widget_homework, "Úkoly", child.optJSONArray("homework"), ::homeworkText)
                val height = manager.getAppWidgetOptions(id).getInt(AppWidgetManager.OPTION_APPWIDGET_MIN_HEIGHT, 180)
                view.setViewVisibility(R.id.widget_tests, if (height >= 150) View.VISIBLE else View.GONE)
                view.setViewVisibility(R.id.widget_homework, if (height >= 210) View.VISIBLE else View.GONE)
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

    private fun parseChild(raw: String?): JSONObject? = runCatching {
        val children = JSONObject(raw ?: return null).getJSONArray("children")
        (0 until children.length()).map { children.getJSONObject(it) }
            .firstOrNull { it.optString("name").contains("Lenka", true) } ?: children.optJSONObject(0)
    }.getOrNull()

    private fun formatDate(value: String): String = runCatching {
        val date = SimpleDateFormat("yyyy-MM-dd", Locale.ROOT).parse(value) ?: Date()
        SimpleDateFormat("EEEE d. M.", Locale("cs", "CZ")).format(date).replaceFirstChar { it.uppercase() }
    }.getOrDefault(value)

    private fun setLessonBoxes(view: RemoteViews, rows: JSONArray?) {
        val ids = intArrayOf(R.id.lesson_1, R.id.lesson_2, R.id.lesson_3, R.id.lesson_4,
            R.id.lesson_5, R.id.lesson_6, R.id.lesson_7)
        val lessons = objects(rows)
        ids.forEachIndexed { index, id ->
            val lesson = lessons.getOrNull(index)
            view.setViewVisibility(id, if (lesson == null) View.GONE else View.VISIBLE)
            if (lesson != null) {
                val text = abbreviate(lesson.optString("subject"))
                view.setTextViewText(id, if (lesson.optBoolean("cancelled")) "$text ×" else text)
            }
        }
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

    private fun changeText(row: JSONObject) = "${row.optString("start")} ${row.optString("subject")} – zrušeno"
    private fun examText(row: JSONObject) = "${row.optString("subject")}: ${row.optString("text")} (${row.optString("date")})"
    private fun homeworkText(row: JSONObject) = "${row.optString("subject")}: ${row.optString("text")} (${row.optString("due")})"
    private fun objects(rows: JSONArray?) = (0 until (rows?.length() ?: 0)).map { rows!!.getJSONObject(it) }
    private fun setSection(view: RemoteViews, id: Int, title: String, rows: JSONArray?, format: (JSONObject) -> String) {
        val limit = 5
        val details = objects(rows).take(limit).joinToString("\n") { "• ${format(it)}" }
        val text = if (details.isBlank()) "$title: nic nového" else "$title\n$details"
        val styled = SpannableString(text).apply {
            setSpan(StyleSpan(Typeface.BOLD), 0, title.length, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
        }
        view.setTextViewText(id, styled)
    }
}
