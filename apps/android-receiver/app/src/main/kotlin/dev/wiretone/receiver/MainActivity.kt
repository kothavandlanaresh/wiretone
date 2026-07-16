package dev.wiretone.receiver

import android.app.Activity
import android.media.AudioManager
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.view.ViewGroup
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView

class MainActivity : Activity() {
    companion object {
        init {
            System.loadLibrary("wiretone_native")
        }
    }

    private external fun nativeVersion(): String
    private external fun nativeOpenPlayback(): Boolean
    private external fun nativeStartPlayback(): Boolean
    private external fun nativeStopPlayback(): Boolean
    private external fun nativeClosePlayback()
    private external fun nativePlaybackStatus(): String

    private val handler = Handler(Looper.getMainLooper())
    private lateinit var statusView: TextView

    private val refreshTask = object : Runnable {
        override fun run() {
            refreshStatus()
            handler.postDelayed(this, 250L)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setVolumeControlStream(AudioManager.STREAM_MUSIC)

        statusView = TextView(this).apply {
            textSize = 16f
            setPadding(32, 32, 32, 32)
        }

        val startButton = Button(this).apply {
            text = getString(R.string.start_silence_output)
            setOnClickListener {
                nativeStartPlayback()
                refreshStatus()
            }
        }

        val stopButton = Button(this).apply {
            text = getString(R.string.stop_output)
            setOnClickListener {
                nativeStopPlayback()
                refreshStatus()
            }
        }

        val controls = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER
            addView(
                startButton,
                LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f)
            )
            addView(
                stopButton,
                LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f)
            )
        }

        val content = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            addView(
                controls,
                LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT
                )
            )
            addView(
                statusView,
                LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT
                )
            )
        }

        setContentView(
            ScrollView(this).apply {
                addView(
                    content,
                    ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT
                    )
                )
            }
        )

        nativeOpenPlayback()
        refreshStatus()
    }

    override fun onStart() {
        super.onStart()
        handler.post(refreshTask)
    }

    override fun onStop() {
        handler.removeCallbacks(refreshTask)
        nativeStopPlayback()
        refreshStatus()
        super.onStop()
    }

    override fun onDestroy() {
        handler.removeCallbacks(refreshTask)
        nativeClosePlayback()
        super.onDestroy()
    }

    private fun refreshStatus() {
        statusView.text = buildString {
            append(getString(R.string.app_name))
            append(" receiver\n\nNative bridge: ")
            append(nativeVersion())
            append("\n\n")
            append(nativePlaybackStatus())
            append("\n\nPhase 3.1 renders silence only. Network audio is not connected.")
        }
    }
}
