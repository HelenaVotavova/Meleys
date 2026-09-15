package cz.meleys.lenkawidget

import android.app.PendingIntent
import android.appwidget.AppWidgetManager
import android.appwidget.AppWidgetProvider
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.widget.RemoteViews
import java.text.SimpleDateFormat
import java.util.Calendar
import java.util.Locale

class LenkaWidgetProvider : AppWidgetProvider() {
    override fun onUpdate(context: Context, manager: AppWidgetManager, ids: IntArray) = update(context, manager, ids)

    private fun update(context: Context, manager: AppWidgetManager, ids: IntArray) {
        val day = Calendar.getInstance()
        if (day.get(Calendar.HOUR_OF_DAY) >= 15) day.add(Calendar.DAY_OF_MONTH, 1)
        while (day.get(Calendar.DAY_OF_WEEK) in listOf(Calendar.SATURDAY, Calendar.SUNDAY)) day.add(Calendar.DAY_OF_MONTH, 1)
        val lessons = schedules[day.get(Calendar.DAY_OF_WEEK)].orEmpty().joinToString("  ·  ")
        val prefs = context.getSharedPreferences("widget_data", Context.MODE_PRIVATE)
        ids.forEach { id ->
            val view = RemoteViews(context.packageName, R.layout.widget_lenka)
            view.setTextViewText(R.id.widget_date, SimpleDateFormat("EEEE d. M.", Locale("cs", "CZ")).format(day.time))
            view.setTextViewText(R.id.widget_schedule, lessons)
            view.setTextViewText(R.id.widget_homework, prefs.getString("homework", "Úkoly: otevři aplikaci a přihlas EduPage"))
            view.setTextViewText(R.id.widget_meal, prefs.getString("meal", "Jídelníček se načte při aktualizaci"))
            view.setTextViewText(R.id.widget_updated, "Klepnutím obnovit")
            val intent = Intent(context, MainActivity::class.java)
            view.setOnClickPendingIntent(R.id.widget_updated, PendingIntent.getActivity(context, 0, intent, PendingIntent.FLAG_IMMUTABLE))
            manager.updateAppWidget(id, view)
        }
    }

    companion object {
        private val schedules = mapOf(
            Calendar.MONDAY to listOf("8:00 M", "8:55 Čj", "10:00 Tv", "10:55 Vl", "11:50 Čj"),
            Calendar.TUESDAY to listOf("8:00 Pč", "8:55 Pč", "10:00 Tv", "10:55 M", "11:50 Aj", "12:45 Hv"),
            Calendar.WEDNESDAY to listOf("8:00 Aj", "8:55 Přv", "10:00 Čj", "10:55 Čj", "11:50 Inf"),
            Calendar.THURSDAY to listOf("8:00 Aj", "8:55 Čj", "10:00 M", "10:55 Vl", "11:50 Čj"),
            Calendar.FRIDAY to listOf("8:00 Čj", "8:55 M", "10:00 Přv", "10:55–12:35 Vv")
        )
        fun refreshAll(context: Context) {
            val manager = AppWidgetManager.getInstance(context)
            val ids = manager.getAppWidgetIds(ComponentName(context, LenkaWidgetProvider::class.java))
            LenkaWidgetProvider().update(context, manager, ids)
        }
    }
}
