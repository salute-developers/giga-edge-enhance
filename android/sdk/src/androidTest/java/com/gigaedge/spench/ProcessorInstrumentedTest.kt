package com.gigaedge.spench

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.util.concurrent.CountDownLatch
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit

@RunWith(AndroidJUnit4::class)
class ProcessorInstrumentedTest {
    private val modelName = "quality48"
    private val hybridModelName = "hybrid48"
    private val sampleRateHz = 48_000
    private val chunkFrames = 768

    @Test
    fun availableModelsDiscoversPackagedAssets() {
        val models = Models.available(testContext())

        assertTrue(models.contains(modelName))
        assertTrue(models.contains(hybridModelName))
        assertTrue(models == models.sorted())
    }

    @Test
    fun processorEventuallyProducesFiniteOutput() {
        Processor(testContext(), sampleRateHz).use { processor ->
            processor.setFeatures(Features(normalization = true, noiseSuppression = modelName))
            val input = FloatArray(chunkFrames) { 0.25f }
            val output = pollForOutput(processor, input)

            assertTrue(output.all { it.isFinite() })
        }
    }

    @Test
    fun processorEventuallyProducesFiniteOutputWithHybrid48() {
        Processor(testContext(), sampleRateHz).use { processor ->
            processor.setFeatures(Features(normalization = true, noiseSuppression = hybridModelName))
            val input = FloatArray(chunkFrames) { 0.25f }
            val output = pollForOutput(processor, input)

            assertTrue(output.all { it.isFinite() })
        }
    }

    @Test
    fun resetKeepsProcessorUsable() {
        Processor(testContext(), sampleRateHz).use { processor ->
            processor.setFeatures(Features(normalization = true, noiseSuppression = modelName))
            val input = FloatArray(chunkFrames) { 0.5f }
            processor.reset()
            val output = pollForOutput(processor, input)

            assertTrue(output.all { it.isFinite() })
        }
    }

    @Test(expected = IllegalArgumentException::class)
    fun processorRejectsShortInputBuffer() {
        Processor(testContext(), sampleRateHz).use { processor ->
            processor.process(floatArrayOf(0.0f), FloatArray(2), 2)
        }
    }

    @Test(expected = IllegalArgumentException::class)
    fun processorRejectsShortOutputBuffer() {
        Processor(testContext(), sampleRateHz).use { processor ->
            processor.process(floatArrayOf(0.0f, 1.0f), FloatArray(1), 2)
        }
    }

    @Test
    fun closedProcessorRejectsFurtherUse() {
        val processor = Processor(testContext(), sampleRateHz)
        processor.close()

        var didThrow = false
        try {
            processor.reset()
        } catch (expected: IllegalStateException) {
            didThrow = true
        }

        assertTrue(didThrow)
    }

    @Test(expected = IllegalStateException::class)
    fun closedProcessorRejectsZeroFrameProcess() {
        val processor = Processor(testContext(), sampleRateHz)
        processor.close()

        processor.process(FloatArray(0), FloatArray(0), 0)
    }

    @Test
    fun processReturnsQuicklyWhenWorkerHasNoOutputYet() {
        Processor(testContext(), sampleRateHz).use { processor ->
            val input = FloatArray(256) { 0.5f }
            val output = FloatArray(input.size)

            val startNs = System.nanoTime()
            processor.process(input, output, input.size)
            val elapsedMs = TimeUnit.NANOSECONDS.toMillis(System.nanoTime() - startNs)

            assertTrue(elapsedMs < 20)
        }
    }

    @Test
    fun firstProcessZeroFillsBeforeWorkerProducesOutput() {
        Processor(testContext(), sampleRateHz).use { processor ->
            val input = FloatArray(64) { 0.5f }
            val output = FloatArray(input.size) { 1.0f }

            processor.process(input, output, input.size)

            assertArrayEquals(FloatArray(input.size), output, 1.0e-6f)
        }
    }

    @Test
    fun closeIsIdempotent() {
        val processor = Processor(testContext(), sampleRateHz)

        processor.close()
        processor.close()

        assertTrue(true)
    }

    @Test(expected = IllegalArgumentException::class)
    fun missingModelThrowsClearException() {
        Processor(testContext(), sampleRateHz).use { processor ->
            processor.setFeatures(Features(noiseSuppression = "missing-model"))
        }
    }

    @Test(expected = IllegalArgumentException::class)
    fun invalidSampleRateThrowsClearException() {
        Processor(testContext(), 0)
    }

    @Test
    fun defaultFeaturesPassInputThrough() {
        Processor(testContext(), sampleRateHz).use { processor ->
            val input = FloatArray(chunkFrames) { index -> index.toFloat() / chunkFrames.toFloat() }
            val output = FloatArray(input.size)

            repeat(50) {
                processor.process(input, output, input.size)
                if (output.contentEquals(input)) {
                    return
                }
                Thread.sleep(2)
            }

            assertArrayEquals(input, output, 1.0e-6f)
        }
    }

    @Test
    fun disabledFeaturesPassInputThrough() {
        Processor(testContext(), sampleRateHz).use { processor ->
            processor.setFeatures(Features(normalization = false, noiseSuppression = null))
            val input = FloatArray(chunkFrames) { index -> index.toFloat() / chunkFrames.toFloat() }
            val output = FloatArray(input.size)

            repeat(50) {
                processor.process(input, output, input.size)
                if (output.contentEquals(input)) {
                    return
                }
                Thread.sleep(2)
            }

            assertArrayEquals(input, output, 1.0e-6f)
        }
    }

    @Test
    fun normalizationOnlyKeepsProcessorUsable() {
        Processor(testContext(), sampleRateHz).use { processor ->
            processor.setFeatures(Features(normalization = true, noiseSuppression = null))
            val input = FloatArray(chunkFrames) { 0.25f }
            val output = pollForOutput(processor, input)

            assertTrue(output.all { it.isFinite() })
        }
    }

    @Test
    fun switchingNoiseSuppressionOnAndOffKeepsProcessorUsable() {
        Processor(testContext(), sampleRateHz).use { processor ->
            val input = FloatArray(chunkFrames) { 0.25f }

            processor.setFeatures(Features(normalization = true, noiseSuppression = modelName))
            assertTrue(pollForOutput(processor, input).all { it.isFinite() })

            processor.setFeatures(Features(normalization = false, noiseSuppression = null))
            assertPassThrough(processor, input)

            processor.setFeatures(Features(normalization = true, noiseSuppression = modelName))
            assertTrue(pollForOutput(processor, input).all { it.isFinite() })
        }
    }

    @Test
    fun switchingNoiseSuppressionOnAndOffKeepsProcessorUsableWithHybrid48() {
        Processor(testContext(), sampleRateHz).use { processor ->
            val input = FloatArray(chunkFrames) { 0.25f }

            processor.setFeatures(Features(normalization = true, noiseSuppression = hybridModelName))
            assertTrue(pollForOutput(processor, input).all { it.isFinite() })

            processor.setFeatures(Features(normalization = false, noiseSuppression = null))
            assertPassThrough(processor, input)

            processor.setFeatures(Features(normalization = true, noiseSuppression = hybridModelName))
            assertTrue(pollForOutput(processor, input).all { it.isFinite() })
        }
    }

    @Test
    fun resetKeepsAllFeatureCombinationsUsable() {
        Processor(testContext(), sampleRateHz).use { processor ->
            val input = FloatArray(chunkFrames) { 0.5f }

            processor.setFeatures(Features(normalization = true, noiseSuppression = null))
            processor.reset()
            assertTrue(pollForOutput(processor, input).all { it.isFinite() })

            processor.setFeatures(Features(normalization = true, noiseSuppression = modelName))
            processor.reset()
            assertTrue(pollForOutput(processor, input).all { it.isFinite() })

            processor.setFeatures(Features(normalization = false, noiseSuppression = null))
            processor.reset()
            assertPassThrough(processor, input)
        }
    }

    @Test
    fun telemetryInitiallyEmpty() {
        Processor(testContext(), sampleRateHz).use { processor ->
            assertTrue(processor.telemetry.normalizationCounters.isEmpty())
            assertTrue(processor.telemetry.noiseSuppressionCounters.isEmpty())
        }
    }

    @Test
    fun normalizationOnlyProcessingIncrementsNormalizationTelemetry() {
        Processor(testContext(), sampleRateHz).use { processor ->
            processor.setFeatures(Features(normalization = true))
            val input = FloatArray(chunkFrames) { 0.25f }
            pollForOutput(processor, input)

            val telemetry = processor.telemetry

            assertTrue(telemetry.normalizationCounters.isNotEmpty())
            assertTrue(telemetry.noiseSuppressionCounters.isEmpty())
        }
    }

    @Test
    fun configuredChunkWaitsForFullBatch() {
        val settings = Settings(minLatency = 0.05, maxProcessingChunkSeconds = 0.064)
        Processor(testContext(), sampleRateHz, settings).use { processor ->
            processor.setFeatures(Features(normalization = true))
            val input = FloatArray(chunkFrames) { 0.25f }
            val output = FloatArray(chunkFrames)

            repeat(2) {
                processor.process(input, output, input.size)
            }
            Thread.sleep(20)
            assertTrue(processor.telemetry.normalizationCounters.isEmpty())

            processor.process(input, output, input.size)
            repeat(50) {
                if (processor.telemetry.normalizationCounters.isNotEmpty()) {
                    return
                }
                Thread.sleep(2)
            }
            throw AssertionError("Timed out waiting for configured processing chunk")
        }
    }

    @Test
    fun denoiserEnabledProcessingIncrementsNoiseSuppressionTelemetry() {
        Processor(testContext(), sampleRateHz).use { processor ->
            processor.setFeatures(Features(noiseSuppression = modelName))
            val input = FloatArray(chunkFrames) { 0.25f }
            pollForOutput(processor, input)

            assertTrue(processor.telemetry.noiseSuppressionCounters.isNotEmpty())
        }
    }

    @Test
    fun telemetryRemainsCumulativeAfterReset() {
        Processor(testContext(), sampleRateHz).use { processor ->
            processor.setFeatures(Features(normalization = true))
            val input = FloatArray(chunkFrames) { 0.25f }
            pollForOutput(processor, input)
            val before = processor.telemetry

            processor.reset()

            val after = processor.telemetry
            assertCountersAtLeast(after.normalizationCounters, before.normalizationCounters)
            assertCountersAtLeast(after.noiseSuppressionCounters, before.noiseSuppressionCounters)
        }
    }

    @Test(expected = IllegalArgumentException::class)
    fun invalidSettingsThrowIllegalArgumentException() {
        Processor(testContext(), sampleRateHz, Settings(telemetryRoundingMilliseconds = Double.NaN))
    }

    @Test
    fun lowPerformanceCallbackExecutorPathDoesNotCrash() {
        val executor = Executors.newSingleThreadExecutor()
        try {
            Processor(
                testContext(),
                sampleRateHz,
                Settings(minLatency = 0.0, lowPerformanceMaxRefillTimes = 1),
                executor,
            ).use { processor ->
                val latch = CountDownLatch(1)
                val callerThread = Thread.currentThread().id
                var callbackThread = callerThread
                processor.onLowPerformance = {
                    callbackThread = Thread.currentThread().id
                    latch.countDown()
                }
                triggerLikelyUnderrun(processor)
                latch.await(2, TimeUnit.SECONDS)
                assertNotEquals(callerThread, callbackThread)
            }
        } finally {
            executor.shutdownNow()
        }
    }

    private fun pollForOutput(processor: Processor, input: FloatArray): FloatArray {
        val output = FloatArray(input.size)
        repeat(50) {
            processor.process(input, output, input.size)
            if (output.any { kotlin.math.abs(it) > 1.0e-6f }) {
                return output
            }
            Thread.sleep(2)
        }
        throw AssertionError("Timed out waiting for async output")
    }

    private fun assertPassThrough(processor: Processor, input: FloatArray) {
        val output = FloatArray(input.size)
        repeat(50) {
            processor.process(input, output, input.size)
            if (output.contentEquals(input)) {
                return
            }
            Thread.sleep(2)
        }
        assertArrayEquals(input, output, 1.0e-6f)
    }

    private fun assertCountersAtLeast(actual: Map<Double, Int>, minimum: Map<Double, Int>) {
        for ((bucket, count) in minimum) {
            assertTrue((actual[bucket] ?: 0) >= count)
        }
    }

    private fun triggerLikelyUnderrun(processor: Processor) {
        val input = FloatArray(chunkFrames) { 0.5f }
        val largeInput = FloatArray(chunkFrames * 2) { 0.5f }
        val output = FloatArray(largeInput.size)
        processor.process(input, FloatArray(input.size), input.size)
        Thread.sleep(5)
        processor.process(largeInput, output, largeInput.size)
    }

    private fun testContext() = InstrumentationRegistry.getInstrumentation().context
}
