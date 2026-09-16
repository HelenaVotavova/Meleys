package cz.meleys.jakubwidget

import android.content.Context
import androidx.work.*
import java.util.concurrent.TimeUnit

class WidgetUpdater(context: Context, params: WorkerParameters) : CoroutineWorker(context, params) {
    override suspend fun doWork(): Result {
        val store = SecureStore(applicationContext)
        val token = store.token ?: return Result.success()
        return try {
            store.dashboard = DashboardApi.dashboard(token)
            store.error = null
            JakubWidgetProvider.refreshAll(applicationContext)
            Result.success()
        } catch (error: Exception) {
            store.error = error.message
            if (error.message?.contains("vypršelo") == true) store.token = null
            JakubWidgetProvider.refreshAll(applicationContext)
            Result.retry()
        }
    }

    companion object {
        private val constraints = Constraints.Builder().setRequiredNetworkType(NetworkType.CONNECTED).build()
        fun schedule(context: Context) {
            val request = PeriodicWorkRequestBuilder<WidgetUpdater>(30, TimeUnit.MINUTES).setConstraints(constraints).build()
            WorkManager.getInstance(context).enqueueUniquePeriodicWork("jakub-widget", ExistingPeriodicWorkPolicy.UPDATE, request)
            refresh(context)
        }
        fun refresh(context: Context) = WorkManager.getInstance(context).enqueue(
            OneTimeWorkRequestBuilder<WidgetUpdater>().setConstraints(constraints).build()
        )
    }
}
