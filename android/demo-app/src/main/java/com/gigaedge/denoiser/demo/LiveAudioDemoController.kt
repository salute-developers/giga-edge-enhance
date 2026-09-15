package com.gigaedge.denoiser.demo

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.media.AudioAttributes
import android.media.AudioDeviceInfo
import android.media.AudioFormat
import android.media.AudioManager
import android.media.AudioRecord
import android.media.AudioTrack
import android.media.MediaRecorder
import android.os.Build
import android.util.Log
import androidx.core.content.ContextCompat
import com.gigaedge.denoiser.Features
import com.gigaedge.denoiser.LowPerformanceEvent
import com.gigaedge.denoiser.Processor
import com.gigaedge.denoiser.Settings
import java.io.Closeable
import java.io.File
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger

internal class LiveAudioDemoController(
    private val context: Context,
) {
    private val deviceLock = Any()
    private val closed = AtomicBoolean(false)
    private val sessionActive = AtomicBoolean(false)
    private val sessionCounter = AtomicInteger(0)

    @Volatile
    private var workerThread: Thread? = null

    @Volatile
    private var recordConfig: RecordConfig? = null

    @Volatile
    private var track: AudioTrack? = null

    @Volatile
    private var sampleRateHz: Int = 0

    @Volatile
    private var chunkFrames: Int = 0

    @Volatile
    private var requestedSessionId: Int = 0

    @Volatile
    private var requestedRecordDumpsEnabled: Boolean = false

    @Volatile
    private var requestedFeatures: Features = Features()

    @Volatile
    private var requestedSettings: Settings = Settings()

    @Volatile
    private var statusCallback: ((String) -> Unit)? = null

    @Volatile
    private var lowPerformanceCallback: ((LowPerformanceEvent) -> Unit)? = null

    fun hasRecordPermission(): Boolean {
        return ContextCompat.checkSelfPermission(context, Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED
    }

    fun isRunning(): Boolean = sessionActive.get()

    fun start(
        features: Features,
        settings: Settings,
        recordDumpsEnabled: Boolean,
        onStatus: (String) -> Unit,
        onLowPerformance: (LowPerformanceEvent) -> Unit,
        onRunningChanged: (Boolean) -> Unit,
    ) {
        if (!hasRecordPermission()) {
            onStatus("Microphone permission is required for live mode")
            return
        }
        if (closed.get()) {
            onStatus("Live controller is closed")
            return
        }
        if (!sessionActive.compareAndSet(false, true)) {
            onStatus("Live demo is already running")
            return
        }

        val sessionId = sessionCounter.incrementAndGet()
        requestedSessionId = sessionId
        requestedFeatures = features
        requestedSettings = settings
        requestedRecordDumpsEnabled = recordDumpsEnabled
        statusCallback = onStatus
        lowPerformanceCallback = onLowPerformance
        log(
            sessionId,
            "start requested features=$features settings=$settings recordDumpsEnabled=$recordDumpsEnabled",
        )

        if (!ensureAudioThreadStarted(onStatus)) {
            onRunningChanged(false)
            return
        }
        onRunningChanged(true)
        onStatus("Starting live demo")
    }

    fun stop(onStatus: (String) -> Unit, onRunningChanged: (Boolean) -> Unit) {
        val sessionId = requestedSessionId
        if (!sessionActive.compareAndSet(true, false)) {
            onStatus("Live demo is not running")
            return
        }

        log(sessionId, "stop requested")
        onRunningChanged(false)
        onStatus("Live demo stopped")
    }

    fun setFeatures(features: Features) {
        requestedFeatures = features
    }

    fun close() {
        if (!closed.compareAndSet(false, true)) {
            return
        }
        sessionActive.set(false)
        workerThread?.join(4000)
        workerThread = null
        synchronized(deviceLock) {
            log(requestedSessionId, "releasing persistent audio devices")
            runCatching { recordConfig?.record?.stop() }
            runCatching { track?.pause() }
            runCatching { track?.flush() }
            runCatching { track?.stop() }
            runCatching { recordConfig?.record?.release() }
            runCatching { track?.release() }
            recordConfig = null
            track = null
            sampleRateHz = 0
            chunkFrames = 0
        }
    }

    private fun ensureAudioThreadStarted(onStatus: (String) -> Unit): Boolean {
        if (workerThread != null) {
            return true
        }

        synchronized(deviceLock) {
            if (workerThread != null) {
                return true
            }
            val sessionId = requestedSessionId
            if (!ensurePersistentDevices(sessionId)) {
                sessionActive.set(false)
                onStatus("Failed to initialize live audio devices")
                return false
            }
            val thread = Thread {
                try {
                    runAudioThread()
                } finally {
                    synchronized(deviceLock) {
                        if (workerThread === Thread.currentThread()) {
                            workerThread = null
                        }
                    }
                }
            }
            thread.name = "GigaDenoiserLiveAudio"
            workerThread = thread
            thread.start()
            return true
        }
    }

    private fun runAudioThread() {
        val currentRecordConfig = recordConfig ?: return
        val currentTrack = track ?: return
        val localSampleRateHz = sampleRateHz
        val localChunkFrames = chunkFrames
        val inputPcm = ShortArray(localChunkFrames)
        var activeState: ActiveSessionState? = null

        log(0, "audio thread starting persistent mic capture")
        currentRecordConfig.record.startRecording()
        if (currentRecordConfig.record.recordingState != AudioRecord.RECORDSTATE_RECORDING) {
            log(0, "persistent microphone capture did not start state=${currentRecordConfig.record.recordingState}")
            statusCallback?.invoke("Microphone capture did not start")
            sessionActive.set(false)
            return
        }

        while (!closed.get()) {
            val framesRead = currentRecordConfig.record.read(inputPcm, 0, inputPcm.size, AudioRecord.READ_NON_BLOCKING)
            if (framesRead < 0) {
                log(activeState?.sessionId ?: requestedSessionId, "AudioRecord read failed code=$framesRead")
                statusCallback?.invoke("AudioRecord read failed: $framesRead")
                sessionActive.set(false)
                break
            }
            if (framesRead == 0) {
                if (!sessionActive.get() && activeState != null) {
                    activeState = tearDownActiveSession(activeState, currentTrack)
                }
                Thread.sleep(5)
                continue
            }

            if (!sessionActive.get()) {
                if (activeState != null) {
                    activeState = tearDownActiveSession(activeState, currentTrack)
                }
                continue
            }

            activeState = activeSessionForCurrentRequest(
                activeState,
                currentRecordConfig,
                currentTrack,
                localSampleRateHz,
                localChunkFrames,
            )

            try {
                processChunk(activeState, currentRecordConfig, currentTrack, inputPcm, framesRead)
            } catch (exception: IllegalArgumentException) {
                log(activeState.sessionId, "feature update failed", exception)
                statusCallback?.invoke(exception.message ?: "Feature update failed")
                sessionActive.set(false)
            }
        }

        activeState?.close()
        runCatching { currentTrack.pause() }
        runCatching { currentTrack.flush() }
        runCatching { currentTrack.stop() }
        runCatching { currentRecordConfig.record.stop() }
        log(0, "audio thread stopped")
    }

    private fun createActiveSession(config: ActiveSessionConfig): ActiveSessionState {
        runCatching { config.track.pause() }
        runCatching { config.track.flush() }
        val state = ActiveSessionState(
            sessionId = config.sessionId,
            processor = Processor(
                context = context,
                sampleRateHz = config.sampleRateHz,
                settings = config.settings,
                callbackExecutor = ContextCompat.getMainExecutor(context),
            ),
            delayLine = FloatDelayLine(config.sampleRateHz / 2),
            inputFloat = FloatArray(config.chunkFrames),
            processedFloat = FloatArray(config.chunkFrames),
            delayedFloat = FloatArray(config.chunkFrames),
            outputPcm = ShortArray(config.chunkFrames),
            inputDumpWriter = createDumpWriter(config.recordDumpsEnabled, "giga-denoiser-live-input.wav", config.sampleRateHz),
            outputDumpWriter = createDumpWriter(config.recordDumpsEnabled, "giga-denoiser-live-output.wav", config.sampleRateHz),
            delayFrames = config.sampleRateHz / 2,
            features = config.features,
        )
        state.processor.onLowPerformance = { event ->
            lowPerformanceCallback?.invoke(event)
        }
        state.processor.setFeatures(config.features)
        val startStatus = buildString {
            append("Live demo running: 0.5 s delayed mic monitor")
            append(" [normalization=${config.features.normalization}")
            append(", noiseSuppression=${config.features.noiseSuppression ?: "off"}")
            append(", source=${config.recordConfig.sourceName}")
            append(", input=${describeInputDevice(config.recordConfig.record)}")
            append(", output=${describeOutputDevice(config.track)}")
            append(", sampleRate=${config.sampleRateHz}")
            append(", chunkFrames=${config.chunkFrames}]")
        }
        state.startStatus = startStatus
        log(config.sessionId, startStatus)
        if (config.recordDumpsEnabled) {
            log(config.sessionId, "recording live WAV dumps to app-internal storage")
        }
        statusCallback?.invoke("Live demo buffering 0.5-second delay")
        return state
    }

    private fun activeSessionForCurrentRequest(
        activeState: ActiveSessionState?,
        currentRecordConfig: RecordConfig,
        currentTrack: AudioTrack,
        localSampleRateHz: Int,
        localChunkFrames: Int,
    ): ActiveSessionState {
        val requestedId = requestedSessionId
        if (activeState?.sessionId == requestedId) {
            return activeState
        }
        if (activeState != null) {
            log(
                activeState.sessionId,
                "session id changed to $requestedId; tearing down old active session before restart",
            )
            tearDownActiveSession(activeState, currentTrack)
        }
        return createActiveSession(
            ActiveSessionConfig(
                sessionId = requestedId,
                features = requestedFeatures,
                settings = requestedSettings,
                recordDumpsEnabled = requestedRecordDumpsEnabled,
                recordConfig = currentRecordConfig,
                track = currentTrack,
                sampleRateHz = localSampleRateHz,
                chunkFrames = localChunkFrames,
            ),
        )
    }

    private fun createDumpWriter(enabled: Boolean, fileName: String, sampleRateHz: Int): WavPcm16Writer? {
        if (!enabled) {
            return null
        }
        return WavPcm16Writer(File(context.filesDir, fileName), sampleRateHz, 1)
    }

    private fun processChunk(
        state: ActiveSessionState,
        recordConfig: RecordConfig,
        track: AudioTrack,
        inputPcm: ShortArray,
        framesRead: Int,
    ) {
        state.inputDumpWriter?.writeSamples(inputPcm, framesRead)
        maybeRecoverSilentInput(state, recordConfig, inputPcm, framesRead)
        val features = requestedFeatures
        if (state.features != features) {
            state.processor.setFeatures(features)
            state.features = features
            log(state.sessionId, "features=$features")
        }
        AudioConversion.pcm16ToFloat(inputPcm, state.inputFloat, framesRead)
        state.processor.process(state.inputFloat, state.processedFloat, framesRead)
        state.delayLine.process(state.processedFloat, state.delayedFloat, framesRead)
        AudioConversion.floatToPcm16(state.delayedFloat, state.outputPcm, framesRead)
        state.outputDumpWriter?.writeSamples(state.outputPcm, framesRead)

        if (state.bufferedFrames < state.delayFrames) {
            state.bufferedFrames = minOf(state.delayFrames, state.bufferedFrames + framesRead)
            if (state.bufferedFrames >= state.delayFrames) {
                track.play()
                statusCallback?.invoke(state.startStatus)
                log(state.sessionId, "delay buffer primed, starting playback")
            }
        }

        if (state.bufferedFrames >= state.delayFrames) {
            writeNonBlocking(track, state.outputPcm, framesRead, state.sessionId)
        }
    }

    private fun maybeRecoverSilentInput(
        state: ActiveSessionState,
        recordConfig: RecordConfig,
        inputPcm: ShortArray,
        framesRead: Int,
    ) {
        if (hasNonZeroSample(inputPcm, framesRead)) {
            state.leadingSilentInputChunks = 0
            return
        }
        state.leadingSilentInputChunks++
        if (state.silentInputRecoveryAttempted || state.leadingSilentInputChunks < LEADING_SILENT_INPUT_RECOVERY_CHUNKS) {
            return
        }

        state.silentInputRecoveryAttempted = true
        log(
            state.sessionId,
            "persistent microphone returned ${state.leadingSilentInputChunks} leading all-zero chunks; " +
                "restarting capture on the same AudioRecord instance recordState=${recordConfig.record.recordingState}",
        )
        runCatching { recordConfig.record.stop() }
            .onFailure { log(state.sessionId, "AudioRecord.stop() failed during silent-input recovery", it) }
        runCatching { recordConfig.record.startRecording() }
            .onFailure { log(state.sessionId, "AudioRecord.startRecording() failed during silent-input recovery", it) }
        log(
            state.sessionId,
            "silent-input recovery complete recordState=${recordConfig.record.recordingState}",
        )
        state.leadingSilentInputChunks = 0
    }

    private fun hasNonZeroSample(buffer: ShortArray, frameCount: Int): Boolean {
        for (index in 0 until frameCount) {
            if (buffer[index].toInt() != 0) {
                return true
            }
        }
        return false
    }

    private fun tearDownActiveSession(state: ActiveSessionState, track: AudioTrack): ActiveSessionState? {
        log(state.sessionId, "resetting active session while keeping persistent input alive")
        runCatching { track.pause() }
            .onFailure { log(state.sessionId, "track.pause() failed during active session reset", it) }
        runCatching { track.flush() }
            .onFailure { log(state.sessionId, "track.flush() failed during active session reset", it) }
        runCatching { state.close() }
            .onFailure { log(state.sessionId, "active session close failed", it) }
        log(state.sessionId, "active session reset complete")
        return null
    }

    private fun ensurePersistentDevices(sessionId: Int): Boolean {
        if (recordConfig != null && track != null) {
            log(
                sessionId,
                "reusing persistent audio devices source=${recordConfig?.sourceName} " +
                    "sampleRate=$sampleRateHz chunkFrames=$chunkFrames",
            )
            return true
        }

        val localSampleRateHz = 48_000
        val localChunkFrames = chunkFramesForSampleRate(localSampleRateHz)
        val inputChannelMask = AudioFormat.CHANNEL_IN_MONO
        val outputChannelMask = AudioFormat.CHANNEL_OUT_MONO
        val encoding = AudioFormat.ENCODING_PCM_16BIT
        val minRecordBufferSize = AudioRecord.getMinBufferSize(localSampleRateHz, inputChannelMask, encoding)
        val minTrackBufferSize = AudioTrack.getMinBufferSize(localSampleRateHz, outputChannelMask, encoding)
        if (minRecordBufferSize <= 0 || minTrackBufferSize <= 0) {
            log(sessionId, "unsupported audio config record=$minRecordBufferSize track=$minTrackBufferSize")
            return false
        }

        val inputFormat = AudioFormat.Builder()
            .setEncoding(encoding)
            .setSampleRate(localSampleRateHz)
            .setChannelMask(inputChannelMask)
            .build()
        val outputFormat = AudioFormat.Builder()
            .setEncoding(encoding)
            .setSampleRate(localSampleRateHz)
            .setChannelMask(outputChannelMask)
            .build()
        val recordBufferBytes = maxOf(minRecordBufferSize * 2, localChunkFrames * 16)
        val trackBufferBytes = maxOf(minTrackBufferSize * 2, localChunkFrames * 8)

        val localRecordConfig = createAudioRecord(sessionId, inputFormat, recordBufferBytes) ?: return false
        val localTrack = createAudioTrack(outputFormat, trackBufferBytes) ?: run {
            localRecordConfig.record.release()
            return false
        }
        preferBuiltInMic(localRecordConfig.record, sessionId)

        recordConfig = localRecordConfig
        track = localTrack
        sampleRateHz = localSampleRateHz
        chunkFrames = localChunkFrames
        log(
            sessionId,
            "created persistent audio devices source=${localRecordConfig.sourceName} " +
                "sampleRate=$localSampleRateHz chunkFrames=$localChunkFrames trackBufferBytes=$trackBufferBytes",
        )
        return true
    }

    private fun writeNonBlocking(track: AudioTrack, buffer: ShortArray, frameCount: Int, sessionId: Int) {
        var offset = 0
        while (offset < frameCount && sessionActive.get() && requestedSessionId == sessionId) {
            val written = track.write(buffer, offset, frameCount - offset, AudioTrack.WRITE_NON_BLOCKING)
            if (written < 0) {
                log(sessionId, "AudioTrack write failed code=$written")
                statusCallback?.invoke("AudioTrack write failed: $written")
                sessionActive.set(false)
                return
            }
            if (written == 0) {
                Thread.sleep(5)
                continue
            }
            offset += written
        }
    }

    private fun createAudioRecord(
        sessionId: Int,
        inputFormat: AudioFormat,
        recordBufferBytes: Int,
    ): RecordConfig? {
        val candidates = buildList {
            if (isProbablyEmulator()) {
                add(MediaRecorder.AudioSource.MIC)
                add(MediaRecorder.AudioSource.VOICE_RECOGNITION)
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                    add(MediaRecorder.AudioSource.UNPROCESSED)
                }
            } else {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                    add(MediaRecorder.AudioSource.UNPROCESSED)
                }
                add(MediaRecorder.AudioSource.VOICE_RECOGNITION)
                add(MediaRecorder.AudioSource.MIC)
            }
        }.distinct()

        for (source in candidates) {
            val record = runCatching {
                AudioRecord.Builder()
                    .setAudioSource(source)
                    .setAudioFormat(inputFormat)
                    .setBufferSizeInBytes(recordBufferBytes)
                    .build()
            }.getOrNull()

            if (record == null) continue
            if (record.state == AudioRecord.STATE_INITIALIZED) {
                log(sessionId, "initialized AudioRecord with source=${sourceName(source)} bufferBytes=$recordBufferBytes")
                return RecordConfig(record, sourceName(source))
            }
            record.release()
        }

        return null
    }

    private fun createAudioTrack(outputFormat: AudioFormat, trackBufferBytes: Int): AudioTrack? {
        val localTrack = runCatching {
            AudioTrack.Builder()
                .setAudioAttributes(
                    AudioAttributes.Builder()
                        .setUsage(AudioAttributes.USAGE_MEDIA)
                        .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
                        .build(),
                )
                .setAudioFormat(outputFormat)
                .setBufferSizeInBytes(trackBufferBytes)
                .setTransferMode(AudioTrack.MODE_STREAM)
                .setSessionId(AudioManager.AUDIO_SESSION_ID_GENERATE)
                .build()
        }.getOrNull()

        return if (localTrack?.state == AudioTrack.STATE_INITIALIZED) localTrack else {
            localTrack?.release()
            null
        }
    }

    private fun preferBuiltInMic(record: AudioRecord, sessionId: Int) {
        if (isProbablyEmulator()) {
            log(sessionId, "skipping preferred input device on emulator")
            return
        }
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            return
        }
        val audioManager = context.getSystemService(AudioManager::class.java) ?: return
        val builtInMic = audioManager.getDevices(AudioManager.GET_DEVICES_INPUTS)
            .firstOrNull { it.type == AudioDeviceInfo.TYPE_BUILTIN_MIC }
        if (builtInMic != null) {
            val success = record.setPreferredDevice(builtInMic)
            log(sessionId, "preferred built-in mic=${builtInMic.productName} success=$success")
        }
    }

    private fun describeInputDevice(record: AudioRecord): String {
        val device = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) record.routedDevice else null
        return device?.productName?.toString() ?: "microphone"
    }

    private fun describeOutputDevice(track: AudioTrack): String {
        val device = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) track.routedDevice else null
        return device?.productName?.toString() ?: "speaker"
    }

    private fun sourceName(source: Int): String {
        return when (source) {
            MediaRecorder.AudioSource.UNPROCESSED -> "UNPROCESSED"
            MediaRecorder.AudioSource.VOICE_RECOGNITION -> "VOICE_RECOGNITION"
            MediaRecorder.AudioSource.MIC -> "MIC"
            else -> source.toString()
        }
    }

    private fun isProbablyEmulator(): Boolean {
        return Build.FINGERPRINT.contains("generic", ignoreCase = true) ||
            Build.HARDWARE.contains("ranchu", ignoreCase = true) ||
            Build.MODEL.contains("sdk_gphone", ignoreCase = true) ||
            Build.PRODUCT.contains("sdk", ignoreCase = true)
    }

    private fun chunkFramesForSampleRate(sampleRateHz: Int): Int {
        return (sampleRateHz * CHUNK_DURATION_MS + 500) / 1000
    }

    private fun log(sessionId: Int, message: String, throwable: Throwable? = null) {
        val text = "[session=$sessionId] $message"
        if (throwable == null) {
            Log.i(TAG, text)
        } else {
            Log.e(TAG, text, throwable)
        }
    }

    private data class RecordConfig(
        val record: AudioRecord,
        val sourceName: String,
    )

    private data class ActiveSessionConfig(
        val sessionId: Int,
        val features: Features,
        val settings: Settings,
        val recordDumpsEnabled: Boolean,
        val recordConfig: RecordConfig,
        val track: AudioTrack,
        val sampleRateHz: Int,
        val chunkFrames: Int,
    )

    private data class ActiveSessionState(
        val sessionId: Int,
        val processor: Processor,
        val delayLine: FloatDelayLine,
        val inputFloat: FloatArray,
        val processedFloat: FloatArray,
        val delayedFloat: FloatArray,
        val outputPcm: ShortArray,
        val inputDumpWriter: WavPcm16Writer?,
        val outputDumpWriter: WavPcm16Writer?,
        val delayFrames: Int,
        var features: Features,
        var startStatus: String = "",
        var bufferedFrames: Int = 0,
        var leadingSilentInputChunks: Int = 0,
        var silentInputRecoveryAttempted: Boolean = false,
    ) : Closeable {
        override fun close() {
            processor.close()
            inputDumpWriter?.close()
            outputDumpWriter?.close()
        }
    }

    companion object {
        const val TAG: String = "GigaDenoiserLiveDemo"
        private const val CHUNK_DURATION_MS = 16
        private const val LEADING_SILENT_INPUT_RECOVERY_CHUNKS = 120
    }
}
