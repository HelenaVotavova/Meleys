package cz.meleys.brnodepartures

import android.app.AlarmManager
import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.os.SystemClock

class MinuteRefreshReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        val pending = goAsync()
        Thread {
            try {
                val store = DepartureStore(context)
                store.data = DepartureApi.load(store.windowMinutes)
                store.error = null
                DeparturesWidgetProvider.refreshAll(context)
            } catch (error: Exception) {
                DepartureStore(context).error = error.message
            } finally {
                schedule(context)
                pending.finish()
            }
        }.start()
    }

    companion object {
        private const val ACTION = "cz.meleys.brnodepartures.MINUTE_REFRESH"
        private fun pendingIntent(context: Context) = PendingIntent.getBroadcast(
            context, 91, Intent(context, MinuteRefreshReceiver::class.java).setAction(ACTION),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        fun schedule(context: Context) {
            val alarm = context.getSystemService(Context.ALARM_SERVICE) as AlarmManager
            alarm.setAndAllowWhileIdle(AlarmManager.ELAPSED_REALTIME_WAKEUP,
                SystemClock.elapsedRealtime() + 60_000, pendingIntent(context))
        }
        fun cancel(context: Context) {
            val alarm = context.getSystemService(Context.ALARM_SERVICE) as AlarmManager
            alarm.cancel(pendingIntent(context))
        }
    }
}
