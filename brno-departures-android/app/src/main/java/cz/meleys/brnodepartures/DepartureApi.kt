package cz.meleys.brnodepartures

import okhttp3.OkHttpClient
import okhttp3.Request
import java.util.concurrent.TimeUnit

object DepartureApi {
    private const val URL = "https://brno.helenavotavova.cz/api/live-departures"
    private val client = OkHttpClient.Builder().connectTimeout(12, TimeUnit.SECONDS).readTimeout(25, TimeUnit.SECONDS).build()
    fun load(): String = client.newCall(Request.Builder().url(URL).build()).execute().use { response ->
        val body = response.body?.string().orEmpty()
        if (!response.isSuccessful) throw Exception("Načtení odjezdů selhalo (${response.code}).")
        body
    }
}
