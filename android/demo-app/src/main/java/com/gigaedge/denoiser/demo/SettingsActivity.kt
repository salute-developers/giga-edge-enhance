package com.gigaedge.denoiser.demo

import android.os.Bundle
import android.widget.Button
import android.widget.CheckBox
import android.widget.EditText
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import com.gigaedge.denoiser.Settings

class SettingsActivity : AppCompatActivity() {
    private lateinit var lowPerformanceWindowSecondsEdit: EditText
    private lateinit var lowPerformanceMaxRefillTimesEdit: EditText
    private lateinit var lowPerformanceMaxRefillTimeFactorEdit: EditText
    private lateinit var minLatencyEdit: EditText
    private lateinit var latencyIncreaseStepEdit: EditText
    private lateinit var maxProcessingChunkSecondsEdit: EditText
    private lateinit var telemetryRoundingMillisecondsEdit: EditText
    private lateinit var useVadNormalizationCheckbox: CheckBox
    private lateinit var reverseProcessingOrderCheckbox: CheckBox
    private lateinit var customNormalizationConfigEdit: EditText
    private lateinit var settingsErrorText: TextView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_settings)

        bindViews()
        renderSettings(SettingsStore.load(this))
        findViewById<Button>(R.id.cancelSettingsButton).setOnClickListener { finish() }
        findViewById<Button>(R.id.saveSettingsButton).setOnClickListener { saveSettings() }
    }

    private fun bindViews() {
        lowPerformanceWindowSecondsEdit = findViewById(R.id.lowPerformanceWindowSecondsEdit)
        lowPerformanceMaxRefillTimesEdit = findViewById(R.id.lowPerformanceMaxRefillTimesEdit)
        lowPerformanceMaxRefillTimeFactorEdit = findViewById(R.id.lowPerformanceMaxRefillTimeFactorEdit)
        minLatencyEdit = findViewById(R.id.minLatencyEdit)
        latencyIncreaseStepEdit = findViewById(R.id.latencyIncreaseStepEdit)
        maxProcessingChunkSecondsEdit = findViewById(R.id.maxProcessingChunkSecondsEdit)
        telemetryRoundingMillisecondsEdit = findViewById(R.id.telemetryRoundingMillisecondsEdit)
        useVadNormalizationCheckbox = findViewById(R.id.useVadNormalizationCheckbox)
        reverseProcessingOrderCheckbox = findViewById(R.id.reverseProcessingOrderCheckbox)
        customNormalizationConfigEdit = findViewById(R.id.customNormalizationConfigEdit)
        settingsErrorText = findViewById(R.id.settingsErrorText)
    }

    private fun renderSettings(settings: Settings) {
        lowPerformanceWindowSecondsEdit.setText(settings.lowPerformanceWindowSeconds.toString())
        lowPerformanceMaxRefillTimesEdit.setText(settings.lowPerformanceMaxRefillTimes.toString())
        lowPerformanceMaxRefillTimeFactorEdit.setText(settings.lowPerformanceMaxRefillTimeFactor.toString())
        minLatencyEdit.setText(settings.minLatency.toString())
        latencyIncreaseStepEdit.setText(settings.latencyIncreaseStep.toString())
        maxProcessingChunkSecondsEdit.setText(settings.maxProcessingChunkSeconds.toString())
        telemetryRoundingMillisecondsEdit.setText(settings.telemetryRoundingMilliseconds.toString())
        useVadNormalizationCheckbox.isChecked = settings.useVadNormalization
        reverseProcessingOrderCheckbox.isChecked = settings.unstableReverseProcessingOrder
        customNormalizationConfigEdit.setText(settings.unstableCustomNormalizationConfig.orEmpty())
    }

    private fun saveSettings() {
        try {
            val settings = Settings(
                lowPerformanceWindowSeconds = lowPerformanceWindowSecondsEdit.requireDouble(),
                lowPerformanceMaxRefillTimes = lowPerformanceMaxRefillTimesEdit.requireInt(),
                lowPerformanceMaxRefillTimeFactor = lowPerformanceMaxRefillTimeFactorEdit.requireDouble(),
                minLatency = minLatencyEdit.requireDouble(),
                latencyIncreaseStep = latencyIncreaseStepEdit.requireDouble(),
                maxProcessingChunkSeconds = maxProcessingChunkSecondsEdit.requireDouble(),
                telemetryRoundingMilliseconds = telemetryRoundingMillisecondsEdit.requireDouble(),
                useVadNormalization = useVadNormalizationCheckbox.isChecked,
                unstableReverseProcessingOrder = reverseProcessingOrderCheckbox.isChecked,
                unstableCustomNormalizationConfig = customNormalizationConfigEdit.text.toString().ifBlank { null },
            )
            SettingsStore.save(this, settings)
            setResult(RESULT_OK)
            finish()
        } catch (exception: IllegalArgumentException) {
            settingsErrorText.text = exception.message ?: getString(R.string.file_result_failed, "invalid settings")
        }
    }

    private fun EditText.requireDouble(): Double = text.toString().trim().toDoubleOrNull()
        ?: throw IllegalArgumentException("${hintOrName()} must be a number")

    private fun EditText.requireInt(): Int = text.toString().trim().toIntOrNull()
        ?: throw IllegalArgumentException("${hintOrName()} must be an integer")

    private fun EditText.hintOrName(): String = hint?.toString()?.takeIf { it.isNotBlank() } ?: "Setting"
}
