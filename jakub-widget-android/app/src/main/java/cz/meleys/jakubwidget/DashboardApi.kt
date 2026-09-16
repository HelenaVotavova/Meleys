package cz.meleys.jakubwidget

import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import org.json.JSONObject
import java.util.concurrent.TimeUnit

data class LoginResult(val token: String, val dashboard: String)

object DashboardApi {
    private const val BASE_URL = "https://edupage.helenavotavova.cz"
    private val jsonType = "application/json; charset=utf-8".toMediaType()
    private val client = OkHttpClient.Builder().connectTimeout(15, TimeUnit.SECONDS).readTimeout(40, TimeUnit.SECONDS).build()

    fun login(username: String, password: String): LoginResult {
        val body = JSONObject().put("username", username).put("password", password).put("subdomain", "zsuvoz").toString().toRequestBody(jsonType)
        val request = Request.Builder().url("$BASE_URL/api/mobile/login").post(body).build()
        client.newCall(request).execute().use { response ->
            val text = response.body?.string().orEmpty()
            if (!response.isSuccessful) throw Exception(message(text, "Přihlášení se nezdařilo (${response.code})."))
            val json = JSONObject(text)
            return LoginResult(json.getString("token"), json.getJSONObject("dashboard").toString())
        }
    }

    fun dashboard(token: String): String {
        val request = Request.Builder().url("$BASE_URL/api/mobile/dashboard").header("Authorization", "Bearer $token").build()
        client.newCall(request).execute().use { response ->
            val text = response.body?.string().orEmpty()
            if (!response.isSuccessful) throw Exception(message(text, if (response.code == 401) "Přihlášení vypršelo." else "Aktualizace selhala (${response.code})."))
            return text
        }
    }

    fun logout(token: String) {
        val body = "".toRequestBody(jsonType)
        client.newCall(Request.Builder().url("$BASE_URL/api/mobile/logout").header("Authorization", "Bearer $token").post(body).build()).execute().close()
    }

    private fun message(body: String, fallback: String) = runCatching { JSONObject(body).optString("detail", fallback) }.getOrDefault(fallback)
}
