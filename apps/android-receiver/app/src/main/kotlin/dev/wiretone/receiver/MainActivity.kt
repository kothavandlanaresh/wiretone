package dev.wiretone.receiver

import android.app.Activity
import android.os.Bundle
import android.view.Gravity
import android.widget.TextView

class MainActivity : Activity() {
    companion object {
        init {
            System.loadLibrary("wiretone_native")
        }
    }

    private external fun nativeVersion(): String

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val status = TextView(this).apply {
            gravity = Gravity.CENTER
            textSize = 20f
            text = getString(
                R.string.app_name
            ) + " receiver shell\n\nNative bridge: " + nativeVersion() +
                "\n\nPhase 0 only — audio playback is not implemented yet."
        }

        setContentView(status)
    }
}
