package cz.meleys.lenkawidget

import android.os.Bundle
import android.view.View
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity

class MainActivity : AppCompatActivity() {
    override fun onCreate(state: Bundle?) {
        super.onCreate(state)
        setContentView(R.layout.activity_main)
        val username = findViewById<EditText>(R.id.username)
        val password = findViewById<EditText>(R.id.password)
        val status = findViewById<TextView>(R.id.status)
        val login = findViewById<Button>(R.id.login)
        val logout = findViewById<Button>(R.id.logout)

        fun showState() {
            val signedIn = SecureStore(this).token != null
            status.text = if (signedIn) "Přihlášeno. Widget se aktualizuje každých 30 minut." else "Přihlas Lenčiným účtem EduPage."
            logout.visibility = if (signedIn) View.VISIBLE else View.GONE
        }
        showState()
        login.setOnClickListener {
            val user = username.text.toString().trim()
            val pass = password.text.toString()
            if (user.isBlank() || pass.isBlank()) { status.text = "Vyplň uživatelské jméno a heslo."; return@setOnClickListener }
            login.isEnabled = false
            status.text = "Přihlašuji a načítám přehled…"
            Thread {
                try {
                    val result = DashboardApi.login(user, pass)
                    SecureStore(this).apply { token = result.token; dashboard = result.dashboard }
                    WidgetUpdater.schedule(this)
                    LenkaWidgetProvider.refreshAll(this)
                    runOnUiThread { password.text.clear(); login.isEnabled = true; showState() }
                } catch (error: Exception) {
                    runOnUiThread { login.isEnabled = true; status.text = error.message ?: "Přihlášení se nezdařilo." }
                }
            }.start()
        }
        logout.setOnClickListener {
            val store = SecureStore(this)
            val oldToken = store.token
            store.clear()
            if (oldToken != null) Thread { runCatching { DashboardApi.logout(oldToken) } }.start()
            LenkaWidgetProvider.refreshAll(this)
            showState()
        }
    }
}
