package cz.meleys.brnodepartures

import android.content.Context
import androidx.work.*
import java.util.concurrent.TimeUnit

class WidgetUpdater(context: Context, params: WorkerParameters) : CoroutineWorker(context, params) {
    override suspend fun doWork(): Result {
        val store = DepartureStore(applicationContext)
        return try {
            store.data = DepartureApi.load(store.windowMinutes); store.error = null
            DeparturesWidgetProvider.refreshAll(applicationContext)
            schedule(applicationContext, 5)
            Result.success()
        } catch (error: Exception) {
            store.error = error.message
            DeparturesWidgetProvider.refreshAll(applicationContext)
            schedule(applicationContext, 5)
            Result.retry()
        }
    }
    companion object {
        fun schedule(context: Context, delayMinutes: Long = 0) {
            val request = OneTimeWorkRequestBuilder<WidgetUpdater>()
                .setInitialDelay(delayMinutes, TimeUnit.MINUTES)
                .setConstraints(Constraints.Builder().setRequiredNetworkType(NetworkType.CONNECTED).build()).build()
            val policy = if (delayMinutes == 0L) ExistingWorkPolicy.REPLACE else ExistingWorkPolicy.APPEND_OR_REPLACE
            WorkManager.getInstance(context).enqueueUniqueWork("brno-departures-refresh", policy, request)
        }
    }
}
