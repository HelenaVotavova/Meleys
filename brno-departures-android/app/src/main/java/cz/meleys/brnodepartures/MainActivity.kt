package cz.meleys.brnodepartures

import android.graphics.Color
import android.os.Bundle
import android.view.View
import android.widget.*
import androidx.appcompat.app.AppCompatActivity
import org.json.JSONObject

class MainActivity : AppCompatActivity() {
    private lateinit var store: DepartureStore
    private lateinit var status: TextView
    private lateinit var routes: LinearLayout
    private lateinit var settings: LinearLayout

    override fun onCreate(state: Bundle?) {
        super.onCreate(state)
        setContentView(R.layout.activity_main)
        store = DepartureStore(this)
        status = findViewById(R.id.status)
        routes = findViewById(R.id.routes)
        settings = findViewById(R.id.settings)
        findViewById<Button>(R.id.refresh).setOnClickListener { refresh() }
        render()
        refresh()
    }

    private fun refresh() {
        status.text = "Načítám aktuální odjezdy…"
        Thread {
            try {
                store.data = DepartureApi.load(); store.error = null
                runOnUiThread { status.text = "Aktualizováno"; render(); DeparturesWidgetProvider.refreshAll(this) }
            } catch (error: Exception) {
                runOnUiThread { status.text = error.message ?: "Načtení selhalo." }
            }
        }.start()
    }

    private fun render() {
        val groups = DepartureFormat.groups(store.data)
        routes.removeAllViews(); settings.removeAllViews()
        groups.forEach { group ->
            renderSetting(group)
            val title = TextView(this).apply {
                text = group.optString("label"); textSize = 18f; setTextColor(Color.rgb(23, 107, 97))
                setTypeface(typeface, android.graphics.Typeface.BOLD); setPadding(0, dp(18), 0, dp(5))
            }
            routes.addView(title)
            val departures = DepartureFormat.departures(group)
            if (departures.isEmpty()) addText(routes, "Žádný odjezd v následujících 25 minutách.")
            departures.forEach { departure -> addText(routes, departureText(departure)) }
        }
        if (groups.isEmpty()) addText(routes, "Data zatím nejsou načtená.")
    }

    private fun renderSetting(group: JSONObject) {
        val id = group.optString("id")
        settings.addView(CheckBox(this).apply {
            text = group.optString("label"); isChecked = store.visible(id); textSize = 15f
            setOnCheckedChangeListener { _, checked ->
                store.setVisible(id, checked)
                DeparturesWidgetProvider.refreshAll(this@MainActivity)
            }
        })
    }

    private fun departureText(row: JSONObject): String {
        val expected = DepartureFormat.time(row.optInt("expected"))
        val planned = DepartureFormat.time(row.optInt("planned"))
        val state = if (row.optBoolean("live")) "živý odhad · ${DepartureFormat.delay(row.optInt("delay"))}" else "vozidlo ještě nevypraveno · jízdní řád"
        return "Linka ${row.optString("line")}  $expected\n$state · plán $planned · směr ${row.optString("destination")}"
    }

    private fun addText(parent: LinearLayout, value: String) = parent.addView(TextView(this).apply {
        text = value; textSize = 15f; setPadding(dp(8), dp(7), dp(8), dp(7)); setBackgroundColor(Color.rgb(245, 248, 247))
    })
    private fun dp(value: Int) = (value * resources.displayMetrics.density).toInt()
}
