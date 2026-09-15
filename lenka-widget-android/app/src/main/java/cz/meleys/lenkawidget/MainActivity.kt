package cz.meleys.lenkawidget

import android.os.Bundle
import android.graphics.Bitmap
import android.webkit.CookieManager
import android.webkit.WebChromeClient
import android.webkit.WebResourceError
import android.webkit.WebResourceRequest
import android.webkit.WebView
import android.webkit.WebViewClient
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.security.crypto.EncryptedSharedPreferences
import androidx.security.crypto.MasterKey

class MainActivity : AppCompatActivity() {
    override fun onCreate(state: Bundle?) {
        super.onCreate(state); setContentView(R.layout.activity_main)
        val status = findViewById<TextView>(R.id.status)
        val web = findViewById<WebView>(R.id.web)
        web.settings.apply {
            javaScriptEnabled = true
            domStorageEnabled = true
            databaseEnabled = true
            javaScriptCanOpenWindowsAutomatically = true
            setSupportMultipleWindows(false)
        }
        val cookies = CookieManager.getInstance()
        cookies.setAcceptCookie(true)
        cookies.setAcceptThirdPartyCookies(web, true)
        web.webChromeClient = WebChromeClient()
        web.webViewClient = object : WebViewClient() {
            override fun onPageStarted(view: WebView, url: String, favicon: Bitmap?) {
                status.text = "Načítám přihlášení EduPage…"
            }

            override fun onPageFinished(view: WebView, url: String) {
                val cookie = cookies.getCookie("https://zsuvoz.edupage.org")
                val stillLoggingIn = url.contains("/login", ignoreCase = true) ||
                    url.contains("login.edupage.org", ignoreCase = true)
                if (url.contains("zsuvoz.edupage.org", ignoreCase = true) &&
                    !stillLoggingIn && !cookie.isNullOrBlank()) {
                    cookie.let {
                        securePrefs().edit().putString("edupage_cookie", it).apply()
                        cookies.flush()
                        status.text = "Přihlášení uloženo. Widget lze přidat na plochu."
                        LenkaWidgetProvider.refreshAll(this@MainActivity)
                    }
                } else {
                    status.text = "Přihlas se na zobrazené stránce EduPage."
                }
            }

            override fun onReceivedError(view: WebView, request: WebResourceRequest, error: WebResourceError) {
                if (request.isForMainFrame) {
                    status.text = "EduPage se nepodařilo načíst (${error.errorCode}): ${error.description}"
                }
            }
        }
        web.loadUrl("https://zsuvoz.edupage.org/login/")
    }

    private fun securePrefs() = EncryptedSharedPreferences.create(
        this, "private_session", MasterKey.Builder(this).setKeyScheme(MasterKey.KeyScheme.AES256_GCM).build(),
        EncryptedSharedPreferences.PrefKeyEncryptionScheme.AES256_SIV,
        EncryptedSharedPreferences.PrefValueEncryptionScheme.AES256_GCM
    )
}
