package cz.meleys.brnodepartures

import android.app.PendingIntent
import android.appwidget.AppWidgetManager
import android.appwidget.AppWidgetProvider
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.view.View
import android.widget.RemoteViews
import org.json.JSONObject
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

class DeparturesWidgetProvider : AppWidgetProvider() {
    override fun onUpdate(context: Context, manager: AppWidgetManager, ids: IntArray) {
        update(context, manager, ids)
        WidgetUpdater.schedule(context)
    }
    override fun onEnabled(context: Context) = WidgetUpdater.schedule(context)
    override fun onReceive(context: Context, intent: Intent) {
        super.onReceive(context, intent)
        if (intent.action == ACTION_REFRESH) WidgetUpdater.schedule(context)
    }

    private fun update(context: Context, manager: AppWidgetManager, ids: IntArray) {
        val store = DepartureStore(context)
        val groups = DepartureFormat.groups(store.data).filter { store.visible(it.optString("id")) }
        val rowIds = intArrayOf(R.id.route_1, R.id.route_2, R.id.route_3, R.id.route_4, R.id.route_5, R.id.route_6)
        ids.forEach { widgetId ->
            val views = RemoteViews(context.packageName, R.layout.widget_departures)
            rowIds.forEachIndexed { index, id ->
                val group = groups.getOrNull(index)
                views.setViewVisibility(id, if (group == null) View.GONE else View.VISIBLE)
                if (group != null) views.setTextViewText(id, compact(group))
            }
            val updated = SimpleDateFormat("H:mm", Locale("cs", "CZ")).format(Date())
            views.setTextViewText(R.id.updated, store.error?.let { "⚠ $it" } ?: "↻ Aktualizovat · $updated")
            val refresh = Intent(context, DeparturesWidgetProvider::class.java).setAction(ACTION_REFRESH)
            views.setOnClickPendingIntent(R.id.updated, PendingIntent.getBroadcast(context, widgetId, refresh, PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE))
            views.setOnClickPendingIntent(R.id.header, PendingIntent.getActivity(context, widgetId, Intent(context, MainActivity::class.java), PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE))
            manager.updateAppWidget(widgetId, views)
        }
    }

    private fun compact(group: JSONObject): String {
        val values = DepartureFormat.departures(group).take(4).joinToString("  ·  ") {
            val source = if (it.optBoolean("live")) DepartureFormat.delay(it.optInt("delay")) else "JŘ"
            "${it.optString("line")} ${DepartureFormat.time(it.optInt("expected"))} ($source)"
        }
        return "${group.optString("label")}\n${values.ifBlank { "Žádný odjezd do 25 minut" }}"
    }

    companion object {
        private const val ACTION_REFRESH = "cz.meleys.brnodepartures.REFRESH"
        fun refreshAll(context: Context) {
            val manager = AppWidgetManager.getInstance(context)
            val ids = manager.getAppWidgetIds(ComponentName(context, DeparturesWidgetProvider::class.java))
            DeparturesWidgetProvider().update(context, manager, ids)
        }
    }
}
