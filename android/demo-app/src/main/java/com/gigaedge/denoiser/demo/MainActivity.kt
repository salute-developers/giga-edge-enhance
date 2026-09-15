package com.gigaedge.denoiser.demo

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Bundle
import android.view.View
import android.widget.AdapterView
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.CheckBox
import android.widget.Spinner
import android.widget.TextView
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import com.gigaedge.denoiser.Features
import com.gigaedge.denoiser.LowPerformanceEvent
import com.gigaedge.denoiser.Models
import com.gigaedge.denoiser.Settings
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicBoolean

class MainActivity : AppCompatActivity() {
    private lateinit var processingValueText: TextView
    private lateinit var statusText: TextView
    private lateinit var fileResultText: TextView
    private lateinit var liveToggleButton: Button
    private lateinit var fileToggleButton: Button
    private lateinit var settingsButton: Button
    private lateinit var lowPerformanceText: TextView
    private lateinit var noiseSuppressionSpinner: Spinner
    private lateinit var normalizationCheckbox: CheckBox
    private lateinit var recordLiveDumpsCheckbox: CheckBox
    private lateinit var liveController: LiveAudioDemoController
    private lateinit var fileProcessor: FileProcessor
    private lateinit var backgroundExecutor: ExecutorService
    private var availableModels: List<String> = emptyList()
    private var currentSettings: Settings = Settings()

    private val isFileProcessing = AtomicBoolean(false)
    private var pendingInputUri: Uri? = null

    @Volatile
    private var normalizationActive: Boolean = true

    @Volatile
    private var noiseSuppressionModel: String? = null

    private val microphonePermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestPermission(),
    ) { granted ->
        if (granted) {
            startLiveDemo()
        } else {
            updateStatus("Microphone permission denied")
        }
    }

    private val openAudioDocumentLauncher = registerForActivityResult(
        ActivityResultContracts.OpenDocument(),
    ) { uri ->
        if (uri == null) {
            updateStatus("Audio file selection cancelled")
            return@registerForActivityResult
        }
        if (!fileProcessor.isSupportedAudioSelection(uri)) {
            fileResultText.text = getString(R.string.file_result_failed, getString(R.string.unsupported_audio_file))
            updateStatus("Selected file is not a supported WAV audio file")
            return@registerForActivityResult
        }
        pendingInputUri = uri
        updateStatus("Choose where to save the processed WAV file")
        createOutputDocumentLauncher.launch(fileProcessor.suggestedOutputName(uri))
    }

    private val createOutputDocumentLauncher = registerForActivityResult(
        ActivityResultContracts.CreateDocument("audio/wav"),
    ) { uri ->
        val inputUri = pendingInputUri
        pendingInputUri = null
        if (uri == null) {
            updateStatus("Output file selection cancelled")
            return@registerForActivityResult
        }
        if (inputUri == null) {
            updateStatus("Missing input audio selection")
            return@registerForActivityResult
        }
        startFileProcessing(inputUri, uri)
    }

    private val settingsLauncher = registerForActivityResult(
        ActivityResultContracts.StartActivityForResult(),
    ) { result ->
        if (result.resultCode == RESULT_OK) {
            currentSettings = SettingsStore.load(this)
            if (liveController.isRunning() || isFileProcessing.get()) {
                updateStatus("Settings saved; they apply next run")
            } else {
                updateStatus(getString(R.string.settings_saved))
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        liveController = LiveAudioDemoController(this)
        fileProcessor = FileProcessor(this)
        backgroundExecutor = Executors.newSingleThreadExecutor()
        currentSettings = SettingsStore.load(this)

        processingValueText = findViewById(R.id.processingValueText)
        statusText = findViewById(R.id.statusText)
        fileResultText = findViewById(R.id.fileResultText)
        liveToggleButton = findViewById(R.id.liveToggleButton)
        fileToggleButton = findViewById(R.id.fileToggleButton)
        settingsButton = findViewById(R.id.settingsButton)
        lowPerformanceText = findViewById(R.id.lowPerformanceText)
        noiseSuppressionSpinner = findViewById(R.id.noiseSuppressionSpinner)
        normalizationCheckbox = findViewById(R.id.normalizationCheckbox)
        recordLiveDumpsCheckbox = findViewById(R.id.recordLiveDumpsCheckbox)

        availableModels = Models.available(this)
        noiseSuppressionModel = availableModels.firstOrNull()
        val noiseSuppressionOptions = listOf(getString(R.string.noise_suppression_off)) + availableModels
        val modelAdapter = ArrayAdapter(
            this,
            android.R.layout.simple_spinner_item,
            noiseSuppressionOptions,
        )
        modelAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        noiseSuppressionSpinner.adapter = modelAdapter
        if (availableModels.isNotEmpty()) {
            noiseSuppressionSpinner.setSelection(1)
        }
        renderProcessing()

        settingsButton.setOnClickListener {
            settingsLauncher.launch(Intent(this, SettingsActivity::class.java))
        }

        liveToggleButton.setOnClickListener {
            if (liveController.isRunning()) {
                stopLiveDemo()
            } else {
                requestPermissionAndStartLiveDemo()
            }
        }
        fileToggleButton.setOnClickListener {
            if (isFileProcessing.get()) {
                stopFileProcessing()
            } else {
                startFileSelectionFlow()
            }
        }
        normalizationCheckbox.setOnCheckedChangeListener { _, isChecked ->
            normalizationActive = isChecked
            liveController.setFeatures(currentFeatures())
        }
        noiseSuppressionSpinner.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(parent: AdapterView<*>?, view: View?, position: Int, id: Long) {
                noiseSuppressionModel = if (position == 0) null else availableModels.getOrNull(position - 1)
                liveController.setFeatures(currentFeatures())
            }

            override fun onNothingSelected(parent: AdapterView<*>?) {
                noiseSuppressionModel = null
                liveController.setFeatures(currentFeatures())
            }
        }

        updateStatus("Ready")
        fileResultText.text = getString(R.string.file_result_idle)
        renderActionButtons()
    }

    override fun onDestroy() {
        if (liveController.isRunning()) {
            liveController.stop(::updateStatus, ::onLiveRunningChanged)
        }
        liveController.close()
        isFileProcessing.set(false)
        backgroundExecutor.shutdownNow()
        super.onDestroy()
    }

    private fun requestPermissionAndStartLiveDemo() {
        if (isFileProcessing.get()) {
            updateStatus("Stop file processing before starting live demo")
            return
        }
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED) {
            startLiveDemo()
        } else {
            microphonePermissionLauncher.launch(Manifest.permission.RECORD_AUDIO)
        }
    }

    private fun startLiveDemo() {
        liveController.start(
            features = currentFeatures(),
            settings = currentSettings,
            recordDumpsEnabled = recordLiveDumpsCheckbox.isChecked,
            onStatus = ::updateStatus,
            onLowPerformance = ::onLowPerformance,
            onRunningChanged = ::onLiveRunningChanged,
        )
    }

    private fun stopLiveDemo() {
        liveController.stop(::updateStatus, ::onLiveRunningChanged)
    }

    private fun startFileSelectionFlow() {
        if (liveController.isRunning()) {
            updateStatus("Stop live demo before processing a file")
            return
        }

        updateStatus("Choose a WAV audio file to process in real time")
        openAudioDocumentLauncher.launch(arrayOf("audio/wav", "audio/x-wav", "audio/wave", "audio/*"))
    }

    private fun startFileProcessing(inputUri: Uri, outputUri: Uri) {
        if (!isFileProcessing.compareAndSet(false, true)) {
            updateStatus("File processing is already running")
            return
        }
        renderActionButtons()
        updateStatus("Processing file in real time")
        fileResultText.text = getString(
            R.string.file_result_running,
            fileProcessor.describeUri(inputUri),
            fileProcessor.describeUri(outputUri),
        )

        backgroundExecutor.execute {
            try {
                val result = fileProcessor.processUriToUri(
                    inputUri = inputUri,
                    outputUri = outputUri,
                    settings = currentSettings,
                    currentFeatures = { currentFeatures() },
                    shouldContinue = { isFileProcessing.get() },
                )
                runOnUiThread {
                    fileResultText.text = getString(
                        R.string.file_result_complete,
                        result.inputName,
                        result.outputName,
                        result.inputFramesProcessed,
                        result.outputFramesWritten,
                        result.sampleRateHz,
                    )
                    updateStatus("File processing completed")
                }
            } catch (t: Throwable) {
                runOnUiThread {
                    fileResultText.text = getString(R.string.file_result_failed, t.message ?: "unknown error")
                    updateStatus(if (t is InterruptedException) "File processing stopped" else "File processing failed")
                }
            } finally {
                isFileProcessing.set(false)
                runOnUiThread {
                    renderActionButtons()
                }
            }
        }
    }

    private fun stopFileProcessing() {
        if (!isFileProcessing.compareAndSet(true, false)) {
            updateStatus("File processing is not running")
            return
        }
        updateStatus("Stopping file processing")
        renderActionButtons()
    }

    private fun renderProcessing() {
        processingValueText.text = getString(R.string.processing_chain_fixed)
    }

    private fun currentFeatures(): Features {
        return Features(
            normalization = normalizationActive,
            noiseSuppression = noiseSuppressionModel,
        )
    }

    private fun updateStatus(status: String) {
        runOnUiThread {
            statusText.text = status
        }
    }

    private fun onLiveRunningChanged(isRunning: Boolean) {
        runOnUiThread {
            liveToggleButton.text = getString(if (isRunning) R.string.stop_live_demo else R.string.start_live_demo)
            if (isRunning) {
                lowPerformanceText.text = getString(R.string.low_performance_none)
            }
            renderActionButtons()
        }
    }

    private fun onLowPerformance(event: LowPerformanceEvent) {
        runOnUiThread {
            lowPerformanceText.text = getString(
                R.string.low_performance_event,
                event.currentLatencySeconds * 1_000.0,
            )
        }
    }

    private fun renderActionButtons() {
        val liveRunning = liveController.isRunning()
        val fileRunning = isFileProcessing.get()
        liveToggleButton.text = getString(if (liveRunning) R.string.stop_live_demo else R.string.start_live_demo)
        fileToggleButton.text = getString(if (fileRunning) R.string.stop_file_processing else R.string.select_file_and_process)
        liveToggleButton.isEnabled = !fileRunning || liveRunning
        fileToggleButton.isEnabled = !liveRunning || fileRunning
        settingsButton.isEnabled = true
        noiseSuppressionSpinner.isEnabled = !fileRunning
        normalizationCheckbox.isEnabled = !fileRunning
        recordLiveDumpsCheckbox.isEnabled = !fileRunning && !liveRunning
    }
}
