package cz.meleys.lenkawidget

import android.graphics.Color
import android.graphics.drawable.GradientDrawable
import android.os.Bundle
import android.text.method.LinkMovementMethod
import android.text.util.Linkify
import android.view.Gravity
import android.view.View
import android.widget.*
import androidx.appcompat.app.AppCompatActivity
import org.json.JSONArray
import org.json.JSONObject
import java.time.LocalDate
import java.util.Locale

class MainActivity : AppCompatActivity() {
    private lateinit var store: SecureStore
    private lateinit var status: TextView
    private lateinit var loginPanel: View
    private lateinit var dashboardPanel: View

    override fun onCreate(state: Bundle?) {
        super.onCreate(state)
        setContentView(R.layout.activity_main)
        store = SecureStore(this)
        status = findViewById(R.id.status)
        loginPanel = findViewById(R.id.login_panel)
        dashboardPanel = findViewById(R.id.dashboard_panel)
        val username = findViewById<EditText>(R.id.username)
        val password = findViewById<EditText>(R.id.password)

        findViewById<Button>(R.id.login).setOnClickListener {
            val user = username.text.toString().trim()
            val pass = password.text.toString()
            if (user.isBlank() || pass.isBlank()) { status.text = "Vyplň uživatelské jméno a heslo."; return@setOnClickListener }
            status.text = "Přihlašuji a načítám přehled…"
            it.isEnabled = false
            Thread {
                try {
                    val result = DashboardApi.login(user, pass)
                    store.token = result.token
                    store.dashboard = result.dashboard
                    WidgetUpdater.schedule(this)
                    runOnUiThread { password.text.clear(); it.isEnabled = true; showState() }
                } catch (error: Exception) {
                    runOnUiThread { it.isEnabled = true; status.text = error.message ?: "Přihlášení se nezdařilo." }
                }
            }.start()
        }
        findViewById<Button>(R.id.refresh).setOnClickListener { refreshDashboard() }
        findViewById<Button>(R.id.logout).setOnClickListener {
            val token = store.token
            store.clear()
            if (token != null) Thread { runCatching { DashboardApi.logout(token) } }.start()
            LenkaWidgetProvider.refreshAll(this)
            showState()
        }
        showState()
    }

    private fun showState() {
        val signedIn = store.token != null
        loginPanel.visibility = if (signedIn) View.GONE else View.VISIBLE
        dashboardPanel.visibility = if (signedIn) View.VISIBLE else View.GONE
        status.text = if (signedIn) "" else "Přihlas Lenčiným účtem EduPage."
        if (signedIn) renderDashboard()
    }

    private fun refreshDashboard() {
        val token = store.token ?: return
        status.text = "Aktualizuji…"
        Thread {
            try {
                store.dashboard = DashboardApi.dashboard(token)
                store.error = null
                runOnUiThread { status.text = "Aktualizováno"; renderDashboard(); LenkaWidgetProvider.refreshAll(this) }
            } catch (error: Exception) {
                runOnUiThread { status.text = error.message ?: "Aktualizace selhala." }
            }
        }.start()
    }

    private fun renderDashboard() {
        val root = runCatching { JSONObject(store.dashboard ?: return) }.getOrNull() ?: return
        val children = root.optJSONArray("children") ?: return
        val child = (0 until children.length()).map { children.getJSONObject(it) }
            .firstOrNull { it.optString("name").contains("Lenka", true) } ?: children.optJSONObject(0) ?: return
        findViewById<TextView>(R.id.date).text = child.optString("date")
        renderSchedule(expandLessons(objects(child.optJSONArray("lessons"))))
        renderLines(R.id.changes_list, child.optJSONArray("changes")) { "${it.optString("start")} ${it.optString("subject")} – zrušeno" }
        val currentExams = JSONArray(objects(child.optJSONArray("exams")).filter(::isCurrentExam))
        renderLines(R.id.tests_list, currentExams) { "${it.optString("subject")}: ${it.optString("text")} · ${it.optString("date")}" }
        renderHomework(child.optJSONArray("homework"), child.optString("date"))
        renderLines(R.id.meal_list, root.optJSONArray("menu")) { it.optString("name") }
        findViewById<TextView>(R.id.clothing).text = clothingText(root.optJSONObject("clothing"))
    }

    private fun renderSchedule(lessons: List<JSONObject>) {
        val row = findViewById<LinearLayout>(R.id.schedule_row)
        row.removeAllViews()
        repeat(6) { index ->
            val lesson = lessons.getOrNull(index)
            val box = TextView(this).apply {
                text = lesson?.let { "${emoji(it.optString("subject"))}\n${abbr(it.optString("subject"))}" } ?: "—"
                gravity = Gravity.CENTER
                textSize = 13f
                setTextColor(Color.rgb(23, 62, 56))
                background = GradientDrawable().apply { setColor(Color.rgb(225, 238, 233)); cornerRadius = dp(3).toFloat() }
                setPadding(dp(2), dp(5), dp(2), dp(5))
            }
            row.addView(box, LinearLayout.LayoutParams(0, dp(52), 1f).apply { marginEnd = dp(4) })
        }
    }

    private fun renderHomework(rows: JSONArray?, displayedDate: String) {
        val target = findViewById<LinearLayout>(R.id.homework_list)
        target.removeAllViews()
        val completed = store.completedTasks
        objects(rows).filter { isCurrentHomework(it, displayedDate) }.forEach { task ->
            val checkbox = CheckBox(this).apply {
                text = "${emoji(task.optString("subject"))} ${task.optString("subject")}: ${task.optString("text")}\nTermín: ${task.optString("due")}"
                textSize = 15f
                isChecked = SecureStore.taskKey(task) in completed
                Linkify.addLinks(this, Linkify.WEB_URLS)
                linksClickable = true
                movementMethod = LinkMovementMethod.getInstance()
                setOnCheckedChangeListener { _, checked ->
                    store.setTaskCompleted(task, checked)
                    LenkaWidgetProvider.refreshAll(this@MainActivity)
                }
            }
            target.addView(checkbox)
        }
        if (target.childCount == 0) addLine(target, "Žádné aktuální úkoly")
    }

    private fun isCurrentHomework(task: JSONObject, displayedDate: String): Boolean {
        if (task.optBoolean("done")) return false
        return runCatching {
            !LocalDate.parse(task.optString("due")).isBefore(LocalDate.parse(displayedDate))
        }.getOrDefault(true)
    }

    private fun isCurrentExam(row: JSONObject) = runCatching {
        !LocalDate.parse(row.optString("date")).isBefore(LocalDate.now())
    }.getOrDefault(false)

    private fun renderLines(containerId: Int, rows: JSONArray?, format: (JSONObject) -> String) {
        val target = findViewById<LinearLayout>(containerId)
        target.removeAllViews()
        objects(rows).forEach { addLine(target, "• ${format(it)}") }
        if (target.childCount == 0) addLine(target, "Nic nového")
    }

    private fun addLine(target: LinearLayout, value: String) {
        target.addView(TextView(this).apply { text = value; textSize = 15f; setPadding(0, dp(3), 0, dp(3)) })
    }

    private fun clothingText(value: JSONObject?): String {
        if (value == null) return "Předpověď není dostupná"
        fun line(key: String, label: String): String {
            val row = value.optJSONObject(key) ?: return "$label: bez předpovědi"
            return "$label ${row.optInt("temperature")} °C: ${row.optString("text")}"
        }
        val rain = value.optJSONObject("rain")
        val rainLine = rain?.let { "Déšť 7:30–14:00: ${if (it.optBoolean("expected")) "ano" else "ne"} · ${it.optInt("chance")} % · ${it.optDouble("amount")} mm" } ?: ""
        return "${line("morning", "Ráno")}\n${line("afternoon", "Odpoledne")}\n$rainLine"
    }

    private fun expandLessons(rows: List<JSONObject>) = rows.flatMap { lesson ->
        val duration = (minutes(lesson.optString("end")) ?: 45) - (minutes(lesson.optString("start")) ?: 0)
        List(if (duration >= 125) 3 else if (duration >= 70) 2 else 1) { lesson }
    }
    private fun minutes(value: String): Int? = value.split(":").takeIf { it.size == 2 }?.let { (it[0].toIntOrNull() ?: return null) * 60 + (it[1].toIntOrNull() ?: return null) }
    private fun objects(rows: JSONArray?) = (0 until (rows?.length() ?: 0)).map { rows!!.getJSONObject(it) }
    private fun abbr(subject: String): String { val n = subject.lowercase(Locale("cs", "CZ")); return when { n.contains("česk") -> "ČJ"; n.contains("mat") -> "M"; n.contains("angl") -> "AJ"; n.contains("přírod") || n.contains("vědou") -> "PŘ"; n.contains("vlasti") -> "VL"; n.contains("informat") -> "INF"; n.contains("těles") -> "TV"; n.contains("hudeb") -> "HV"; n.contains("výtvar") -> "VV"; n.contains("pracovn") -> "PČ"; else -> subject.take(4).uppercase() } }
    private fun emoji(subject: String): String { val n = subject.lowercase(Locale("cs", "CZ")); return when { n.contains("česk") -> "📖"; n.contains("mat") -> "➗"; n.contains("angl") -> "🇬🇧"; n.contains("přírod") || n.contains("vědou") -> "🌿"; n.contains("vlasti") -> "🗺️"; n.contains("informat") -> "💻"; n.contains("těles") -> "🏃"; n.contains("hudeb") -> "🎵"; n.contains("výtvar") || n.contains("pracovn") -> "🎨"; else -> "📘" } }
    private fun dp(value: Int) = (value * resources.displayMetrics.density).toInt()
}
