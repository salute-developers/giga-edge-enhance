package com.gigaedge.denoiser.demo

import android.content.ContentResolver
import android.content.Context
import android.net.Uri
import android.os.SystemClock
import android.provider.OpenableColumns
import com.gigaedge.denoiser.Features
import com.gigaedge.denoiser.Processor
import com.gigaedge.denoiser.Settings
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
import java.io.InputStream
import java.io.OutputStream
internal class FileProcessor(
    private val context: Context,
) {
    fun isSupportedAudioSelection(inputUri: Uri): Boolean {
        val mimeType = context.contentResolver.getType(inputUri)?.lowercase()
        if (mimeType == "audio/wav" || mimeType == "audio/x-wav" || mimeType == "audio/wave" || mimeType == "audio/vnd.wave") {
            return true
        }
        val name = describeUri(inputUri).lowercase()
        return name.endsWith(".wav") || name.endsWith(".wave")
    }

    fun describeUri(uri: Uri): String {
        if (uri.scheme == ContentResolver.SCHEME_CONTENT) {
            context.contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use { cursor ->
                if (cursor.moveToFirst()) {
                    val index = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
                    if (index >= 0) {
                        return cursor.getString(index)
                    }
                }
            }
        }
        return uri.lastPathSegment ?: "audio.wav"
    }

    fun suggestedOutputName(inputUri: Uri): String {
        val inputName = describeUri(inputUri)
        val dotIndex = inputName.lastIndexOf('.')
        val baseName = if (dotIndex > 0) inputName.substring(0, dotIndex) else inputName
        return "$baseName-gigadenoiser.wav"
    }

    fun processUriToUri(
        inputUri: Uri,
        outputUri: Uri,
        settings: Settings,
        currentFeatures: () -> Features,
        shouldContinue: () -> Boolean,
    ): FileProcessingResult {
        require(isSupportedAudioSelection(inputUri)) { "Only WAV audio files are currently supported" }

        val inputName = describeUri(inputUri)
        val outputName = describeUri(outputUri)
        val tempOutputFile = File.createTempFile("giga-denoiser-output-", ".wav", context.cacheDir)
        var result: FileProcessingResult? = null

        try {
            openInputStream(inputUri).use { inputStream ->
                WavPcm16Reader(inputStream).use { reader ->
                    WavPcm16Writer(tempOutputFile, reader.format.sampleRateHz, reader.format.channelCount).use { writer ->
                        Processor(context, reader.format.sampleRateHz, settings).use { processor ->
                            val chunkSizeFrames = chunkFramesForSampleRate(reader.format.sampleRateHz)
                            val inputPcm = ShortArray(chunkSizeFrames)
                            val outputPcm = ShortArray(chunkSizeFrames)
                            val inputFloat = FloatArray(chunkSizeFrames)
                            val processedFloat = FloatArray(chunkSizeFrames)
                            var features = currentFeatures()
                            processor.setFeatures(features)
                            var inputFramesProcessed = 0
                            var outputFramesWritten = 0
                            var nextDeadlineNs = SystemClock.elapsedRealtimeNanos()

                            while (shouldContinue()) {
                                val framesRead = reader.readSamples(inputPcm)
                                if (framesRead == 0) {
                                    break
                                }
                                val nextFeatures = currentFeatures()
                                if (features != nextFeatures) {
                                    features = nextFeatures
                                    processor.setFeatures(features)
                                }
                                AudioConversion.pcm16ToFloat(inputPcm, inputFloat, framesRead)
                                processor.process(inputFloat, processedFloat, framesRead)
                                AudioConversion.floatToPcm16(processedFloat, outputPcm, framesRead)
                                writer.writeSamples(outputPcm, framesRead)
                                inputFramesProcessed += framesRead
                                outputFramesWritten += framesRead
                                nextDeadlineNs = paceRealtime(nextDeadlineNs, framesRead, reader.format.sampleRateHz)
                            }

                            ensure(shouldContinue()) { "File processing cancelled" }

                            result = FileProcessingResult(
                                inputName = inputName,
                                outputName = outputName,
                                sampleRateHz = reader.format.sampleRateHz,
                                inputFramesProcessed = inputFramesProcessed,
                                outputFramesWritten = outputFramesWritten,
                            )
                        }
                    }
                }
            }
            openOutputStream(outputUri).use { outputStream ->
                copyFileToStream(tempOutputFile, outputStream)
            }
            return requireNotNull(result) { "Missing file processing result" }
        } finally {
            tempOutputFile.delete()
        }
    }

    private fun paceRealtime(previousDeadlineNs: Long, framesProcessed: Int, sampleRateHz: Int): Long {
        val startDeadlineNs = if (previousDeadlineNs == 0L) SystemClock.elapsedRealtimeNanos() else previousDeadlineNs
        val nextDeadlineNs = startDeadlineNs + (framesProcessed * 1_000_000_000L / sampleRateHz)
        val sleepNs = nextDeadlineNs - SystemClock.elapsedRealtimeNanos()
        if (sleepNs > 0L) {
            Thread.sleep(sleepNs / 1_000_000L, (sleepNs % 1_000_000L).toInt())
        }
        return nextDeadlineNs
    }

    private fun chunkFramesForSampleRate(sampleRateHz: Int): Int {
        return (sampleRateHz * CHUNK_DURATION_MS + 500) / 1000
    }

    private fun openInputStream(uri: Uri): InputStream {
        return when (uri.scheme) {
            ContentResolver.SCHEME_FILE, null -> FileInputStream(requireNotNull(uri.path) { "Missing file path" })
            else -> requireNotNull(context.contentResolver.openInputStream(uri)) { "Unable to open input audio file" }
        }
    }

    private fun openOutputStream(uri: Uri): OutputStream {
        return when (uri.scheme) {
            ContentResolver.SCHEME_FILE, null -> FileOutputStream(requireNotNull(uri.path) { "Missing file path" })
            else -> requireNotNull(context.contentResolver.openOutputStream(uri, "w")) { "Unable to open output audio file" }
        }
    }

    private fun ensure(condition: Boolean, lazyMessage: () -> String) {
        if (!condition) {
            throw InterruptedException(lazyMessage())
        }
    }

    companion object {
        private const val CHUNK_DURATION_MS = 16
    }
}
