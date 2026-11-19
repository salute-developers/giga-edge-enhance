package com.gigaedge.enhance.demo

import android.Manifest
import android.app.Activity.RESULT_OK
import android.app.Instrumentation.ActivityResult
import android.content.Context
import android.content.Intent
import android.net.Uri
import androidx.test.core.app.ActivityScenario
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.Espresso.onData
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.action.ViewActions.closeSoftKeyboard
import androidx.test.espresso.action.ViewActions.replaceText
import androidx.test.espresso.action.ViewActions.scrollTo
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.intent.Intents
import androidx.test.espresso.intent.Intents.intending
import androidx.test.espresso.intent.matcher.IntentMatchers.hasAction
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.isChecked
import androidx.test.espresso.matcher.ViewMatchers.isEnabled
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withSpinnerText
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.rule.GrantPermissionRule
import org.hamcrest.Matchers.containsString
import org.hamcrest.Matchers.hasToString
import org.hamcrest.Matchers.not
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File

@RunWith(AndroidJUnit4::class)
class MainActivityTest {
    @get:Rule
    val microphonePermissionRule: GrantPermissionRule = GrantPermissionRule.grant(Manifest.permission.RECORD_AUDIO)

    @Before
    fun clearDemoSettings() {
        targetContext().getSharedPreferences(DEMO_SETTINGS_PREFS_NAME, Context.MODE_PRIVATE)
            .edit()
            .clear()
            .commit()
    }

    @Test
    fun appLaunches() {
        val targetContext = targetContext()
        ActivityScenario.launch<MainActivity>(Intent(targetContext, MainActivity::class.java)).use {
            onView(withId(R.id.titleText)).check(matches(isDisplayed()))
            onView(withId(R.id.settingsButton)).check(matches(isDisplayed()))
            onView(withId(R.id.lowPerformanceText)).check(matches(withText(containsString("Low performance"))))
            onView(withId(R.id.statusText)).check(matches(withText("Ready")))
        }
    }

    @Test
    fun processingChainIsShown() {
        val targetContext = targetContext()
        ActivityScenario.launch<MainActivity>(Intent(targetContext, MainActivity::class.java)).use {
            onView(withId(R.id.processingValueText)).check(matches(withText(containsString("GigaEdgeEnhance"))))
        }
    }

    @Test
    fun noiseSuppressionSelectorDefaultsToSortedFirstAndCanSelectOffAndModels() {
        val targetContext = targetContext()
        ActivityScenario.launch<MainActivity>(Intent(targetContext, MainActivity::class.java)).use {
            onView(withId(R.id.noiseSuppressionSpinner)).check(matches(withSpinnerText("hybrid48")))

            onView(withId(R.id.noiseSuppressionSpinner)).perform(click())
            onData(hasToString("Off")).perform(click())
            onView(withId(R.id.noiseSuppressionSpinner)).check(matches(withSpinnerText("Off")))

            onView(withId(R.id.noiseSuppressionSpinner)).perform(click())
            onData(hasToString("quality48")).perform(click())

            onView(withId(R.id.noiseSuppressionSpinner)).check(matches(withSpinnerText("quality48")))

            onView(withId(R.id.noiseSuppressionSpinner)).perform(click())
            onData(hasToString("hybrid48")).perform(click())

            onView(withId(R.id.noiseSuppressionSpinner)).check(matches(withSpinnerText("hybrid48")))
        }
    }

    @Test
    fun normalizationToggleDefaultsEnabledAndCanToggle() {
        val targetContext = targetContext()
        ActivityScenario.launch<MainActivity>(Intent(targetContext, MainActivity::class.java)).use {
            onView(withId(R.id.normalizationCheckbox)).check(matches(isChecked()))

            onView(withId(R.id.normalizationCheckbox)).perform(click())

            onView(withId(R.id.normalizationCheckbox)).check(matches(not(isChecked())))

            onView(withId(R.id.normalizationCheckbox)).perform(click())

            onView(withId(R.id.normalizationCheckbox)).check(matches(isChecked()))
        }
    }

    @Test
    fun settingsButtonOpensSettingsScreen() {
        val targetContext = targetContext()
        ActivityScenario.launch<MainActivity>(Intent(targetContext, MainActivity::class.java)).use {
            onView(withId(R.id.settingsButton)).perform(click())

            onView(withId(R.id.lowPerformanceWindowSecondsEdit)).check(matches(isDisplayed()))
            onView(withId(R.id.saveSettingsButton)).perform(scrollTo())
            onView(withId(R.id.saveSettingsButton)).check(matches(isDisplayed()))
        }
    }

    @Test
    fun settingsValidationShowsErrorAndPersistenceSurvivesReopen() {
        val targetContext = targetContext()
        ActivityScenario.launch<MainActivity>(Intent(targetContext, MainActivity::class.java)).use {
            onView(withId(R.id.settingsButton)).perform(click())

            onView(withId(R.id.telemetryRoundingMillisecondsEdit))
                .perform(scrollTo(), replaceText("0.0"), closeSoftKeyboard())
            onView(withId(R.id.saveSettingsButton)).perform(scrollTo(), click())

            onView(withId(R.id.settingsErrorText))
                .check(matches(withText(containsString("telemetryRoundingMilliseconds"))))

            onView(withId(R.id.lowPerformanceWindowSecondsEdit))
                .perform(scrollTo(), replaceText("2.0"), closeSoftKeyboard())
            onView(withId(R.id.maxProcessingChunkSecondsEdit))
                .perform(scrollTo(), replaceText("0.032"), closeSoftKeyboard())
            onView(withId(R.id.telemetryRoundingMillisecondsEdit))
                .perform(scrollTo(), replaceText("0.25"), closeSoftKeyboard())
            onView(withId(R.id.saveSettingsButton)).perform(scrollTo(), click())

            waitUntilStatus(it, "Settings saved")
            onView(withId(R.id.settingsButton)).perform(click())
            onView(withId(R.id.lowPerformanceWindowSecondsEdit)).check(matches(withText("2.0")))
            onView(withId(R.id.maxProcessingChunkSecondsEdit)).perform(scrollTo())
            onView(withId(R.id.maxProcessingChunkSecondsEdit)).check(matches(withText("0.032")))
        }
    }

    @Test
    fun liveDemoStartStopRestartSmoke() {
        val targetContext = targetContext()
        ActivityScenario.launch<MainActivity>(Intent(targetContext, MainActivity::class.java)).use { scenario ->
            startLiveOrSkipIfUnavailable(scenario)

            onView(withId(R.id.liveToggleButton)).check(matches(withText("Stop live demo")))
            onView(withId(R.id.fileToggleButton)).check(matches(not(isEnabled())))
            onView(withId(R.id.recordLiveDumpsCheckbox)).check(matches(not(isEnabled())))

            onView(withId(R.id.liveToggleButton)).perform(scrollTo(), click())
            waitUntilButtonText(scenario, R.id.liveToggleButton, "Start live demo")
            onView(withId(R.id.fileToggleButton)).check(matches(isEnabled()))

            startLiveOrSkipIfUnavailable(scenario)

            onView(withId(R.id.liveToggleButton)).check(matches(withText("Stop live demo")))
            onView(withId(R.id.liveToggleButton)).perform(scrollTo(), click())
            waitUntilButtonText(scenario, R.id.liveToggleButton, "Start live demo")
        }
    }

    @Test
    fun recordLiveDumpsOptionCanToggleBeforeLiveStarts() {
        val targetContext = targetContext()
        ActivityScenario.launch<MainActivity>(Intent(targetContext, MainActivity::class.java)).use {
            onView(withId(R.id.recordLiveDumpsCheckbox)).perform(scrollTo(), click())
            onView(withId(R.id.recordLiveDumpsCheckbox)).check(matches(isChecked()))

            onView(withId(R.id.recordLiveDumpsCheckbox)).perform(scrollTo(), click())
            onView(withId(R.id.recordLiveDumpsCheckbox)).check(matches(not(isChecked())))
        }
    }

    @Test
    fun fileProcessingFlowCreatesOutput() {
        runFileProcessingFlow(selectedNoiseSuppression = null, inputName = "ui-input.wav", outputName = "ui-output.wav")
    }

    @Test
    fun fileProcessingFlowCreatesOutputWithQuality48() {
        runFileProcessingFlow(selectedNoiseSuppression = "quality48", inputName = "ui-input-48k.wav", outputName = "ui-output-48k.wav")
    }

    @Test
    fun fileProcessingFlowCreatesOutputWithHybrid48() {
        runFileProcessingFlow(selectedNoiseSuppression = "hybrid48", inputName = "ui-input-hybrid48.wav", outputName = "ui-output-hybrid48.wav")
    }

    @Test
    fun fileProcessingFlowCreatesOutputWithNoiseSuppressionOff() {
        runFileProcessingFlow(selectedNoiseSuppression = "Off", inputName = "ui-input-off.wav", outputName = "ui-output-off.wav")
    }

    @Test
    fun fileProcessingFlowCreatesOutputWithNormalizationOffAndNoiseSuppressionOff() {
        runFileProcessingFlow(
            selectedNoiseSuppression = "Off",
            normalizationEnabled = false,
            inputName = "ui-input-normalization-off.wav",
            outputName = "ui-output-normalization-off.wav",
        )
    }

    private fun runFileProcessingFlow(
        selectedNoiseSuppression: String?,
        inputName: String,
        outputName: String,
        normalizationEnabled: Boolean = true,
    ) {
        val targetContext = targetContext()
        val inputFile = File(targetContext.cacheDir, inputName)
        val outputFile = File(targetContext.cacheDir, outputName)
        targetContext.assets.open("input_5s_mono.wav").use { input ->
            inputFile.outputStream().use { output ->
                input.copyTo(output)
            }
        }
        if (outputFile.exists()) {
            outputFile.delete()
        }

        Intents.init()
        try {
            intending(hasAction(Intent.ACTION_OPEN_DOCUMENT))
                .respondWith(ActivityResult(RESULT_OK, Intent().setData(Uri.fromFile(inputFile))))
            intending(hasAction(Intent.ACTION_CREATE_DOCUMENT))
                .respondWith(ActivityResult(RESULT_OK, Intent().setData(Uri.fromFile(outputFile))))

            ActivityScenario.launch<MainActivity>(Intent(targetContext, MainActivity::class.java)).use { scenario ->
                if (!normalizationEnabled) {
                    onView(withId(R.id.normalizationCheckbox)).perform(click())
                    onView(withId(R.id.normalizationCheckbox)).check(matches(not(isChecked())))
                }
                if (selectedNoiseSuppression != null) {
                    onView(withId(R.id.noiseSuppressionSpinner)).perform(click())
                    onData(hasToString(selectedNoiseSuppression)).perform(click())
                    onView(withId(R.id.noiseSuppressionSpinner)).check(matches(withSpinnerText(selectedNoiseSuppression)))
                }
                scenario.onActivity { activity ->
                    activity.findViewById<android.view.View>(R.id.fileToggleButton).performClick()
                }
                onView(withId(R.id.liveToggleButton)).check(matches(not(isEnabled())))
                onView(withId(R.id.noiseSuppressionSpinner)).check(matches(not(isEnabled())))
                onView(withId(R.id.normalizationCheckbox)).check(matches(not(isEnabled())))
                onView(withId(R.id.recordLiveDumpsCheckbox)).check(matches(not(isEnabled())))

                waitUntilCompleted(scenario)

                onView(withId(R.id.statusText)).check(matches(withText("File processing completed")))
                onView(withId(R.id.fileResultText)).check(matches(withText(containsString(outputName))))

                scenario.onActivity {
                    assertTrue(outputFile.exists())
                    assertTrue(outputFile.length() > 44L)
                    assertWavHeader(outputFile)
                }
            }
        } finally {
            Intents.release()
        }
    }

    private fun waitUntilCompleted(scenario: ActivityScenario<MainActivity>) {
        val timeoutMs = 60_000L
        val pollMs = 100L
        val startMs = System.currentTimeMillis()
        var completed = false
        while (System.currentTimeMillis() - startMs < timeoutMs && !completed) {
            InstrumentationRegistry.getInstrumentation().waitForIdleSync()
            scenario.onActivity { activity ->
                completed = activity.findViewById<android.widget.TextView>(R.id.statusText)
                    .text
                    .toString() == "File processing completed"
            }
            if (!completed) {
                Thread.sleep(pollMs)
            }
        }
        assertTrue("Timed out waiting for file processing to complete", completed)
    }

    private fun startLiveOrSkipIfUnavailable(scenario: ActivityScenario<MainActivity>) {
        onView(withId(R.id.liveToggleButton)).perform(scrollTo(), click())

        val status = waitUntilLiveStartedOrFailed(scenario)
        org.junit.Assume.assumeFalse(
            "Live audio devices are unavailable on this test device: $status",
            status.contains("Failed to initialize live audio devices") ||
                status.contains("Microphone capture did not start") ||
                status.contains("Microphone permission is required"),
        )
    }

    private fun waitUntilLiveStartedOrFailed(scenario: ActivityScenario<MainActivity>): String {
        val timeoutMs = 5_000L
        val pollMs = 100L
        val startMs = System.currentTimeMillis()
        var liveStarted = false
        var status = ""
        while (System.currentTimeMillis() - startMs < timeoutMs && !liveStarted && status.isBlank()) {
            InstrumentationRegistry.getInstrumentation().waitForIdleSync()
            scenario.onActivity { activity ->
                liveStarted = activity.findViewById<android.widget.Button>(R.id.liveToggleButton)
                    .text
                    .toString() == "Stop live demo"
                status = activity.findViewById<android.widget.TextView>(R.id.statusText)
                    .text
                    .toString()
                    .takeIf { text ->
                        text.contains("Failed to initialize live audio devices") ||
                            text.contains("Microphone capture did not start") ||
                            text.contains("Microphone permission is required")
                    }
                    .orEmpty()
            }
            if (!liveStarted && status.isBlank()) {
                Thread.sleep(pollMs)
            }
        }
        assertTrue("Timed out waiting for live demo to start or fail", liveStarted || status.isNotBlank())
        return status
    }

    private fun waitUntilButtonText(scenario: ActivityScenario<MainActivity>, buttonId: Int, expected: String) {
        val timeoutMs = 5_000L
        val pollMs = 100L
        val startMs = System.currentTimeMillis()
        var matched = false
        while (System.currentTimeMillis() - startMs < timeoutMs && !matched) {
            InstrumentationRegistry.getInstrumentation().waitForIdleSync()
            scenario.onActivity { activity ->
                matched = activity.findViewById<android.widget.Button>(buttonId).text.toString() == expected
            }
            if (!matched) {
                Thread.sleep(pollMs)
            }
        }
        assertTrue("Timed out waiting for button text $expected", matched)
    }

    private fun waitUntilStatus(scenario: ActivityScenario<MainActivity>, expected: String) {
        val timeoutMs = 5_000L
        val pollMs = 100L
        val startMs = System.currentTimeMillis()
        var matched = false
        while (System.currentTimeMillis() - startMs < timeoutMs && !matched) {
            InstrumentationRegistry.getInstrumentation().waitForIdleSync()
            scenario.onActivity { activity ->
                matched = activity.findViewById<android.widget.TextView>(R.id.statusText).text.toString() == expected
            }
            if (!matched) {
                Thread.sleep(pollMs)
            }
        }
        assertTrue("Timed out waiting for status $expected", matched)
    }

    private fun assertWavHeader(file: File) {
        val bytes = file.readBytes()
        assertEquals("RIFF", String(bytes.copyOfRange(0, 4), Charsets.US_ASCII))
        assertEquals("WAVE", String(bytes.copyOfRange(8, 12), Charsets.US_ASCII))
        assertEquals("fmt ", String(bytes.copyOfRange(12, 16), Charsets.US_ASCII))
        assertEquals("data", String(bytes.copyOfRange(36, 40), Charsets.US_ASCII))
    }

    private fun targetContext(): Context = InstrumentationRegistry.getInstrumentation().targetContext

    private companion object {
        const val DEMO_SETTINGS_PREFS_NAME = "gigaedge-enhance-demo-settings"
    }
}
