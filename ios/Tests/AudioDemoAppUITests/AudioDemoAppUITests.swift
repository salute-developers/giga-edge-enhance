import XCTest

final class AudioDemoAppUITests: XCTestCase {
    func testSettingsScreenCanSaveUpdatedValues() throws {
        let app = try launchApp()

        app.navigationBars.buttons["Settings"].tap()

        let windowField = app.textFields["lowPerformanceWindowField"]
        let maxProcessingChunkField = app.textFields["maxProcessingChunkSecondsField"]

        XCTAssertTrue(windowField.waitForExistence(timeout: 5))
        XCTAssertTrue(maxProcessingChunkField.waitForExistence(timeout: 5))
        clearAndType("0.032", into: maxProcessingChunkField)
        clearAndType("2.5", into: windowField)
        app.navigationBars.buttons["Save"].tap()

        XCTAssertTrue(app.buttons["liveButton"].waitForExistence(timeout: 5))
    }

    func testControlsRemainEnabledAndModelCanSwitchWhileProcessing() throws {
        let app = try launchApp()

        let modelButton = app.buttons["modelButton"]
        let processingSwitch = app.switches["audioProcessingSwitch"]
        let fileButton = app.buttons["fileButton"]
        let saveButton = app.buttons["saveButton"]
        let statusLabel = app.staticTexts["statusLabel"]
        let lowPerformanceLabel = app.staticTexts["lowPerformanceLabel"]
        let audioProcessingTelemetryLabel = app.staticTexts["audioProcessingTelemetryLabel"]
        let denoiserTelemetryLabel = app.staticTexts["denoiserTelemetryLabel"]
        let errorView = app.textViews["errorView"]

        XCTAssertTrue(modelButton.waitForExistence(timeout: 5))
        XCTAssertTrue(processingSwitch.waitForExistence(timeout: 5))
        XCTAssertTrue(fileButton.waitForExistence(timeout: 5))
        XCTAssertTrue(audioProcessingTelemetryLabel.waitForExistence(timeout: 5))
        XCTAssertTrue(denoiserTelemetryLabel.waitForExistence(timeout: 5))
        XCTAssertEqual(audioProcessingTelemetryLabel.label, "Audio processing mean: --")
        XCTAssertEqual(denoiserTelemetryLabel.label, "Denoising mean: --")
        XCTAssertTrue(!lowPerformanceLabel.exists || !lowPerformanceLabel.isHittable)

        selectModel(withID: "fast8", in: app)
        XCTAssertEqual(modelButton.label, "fast8")

        fileButton.tap()

        XCTAssertEqual(fileButton.label, "Cancel Processing")
        XCTAssertTrue(modelButton.isEnabled)
        XCTAssertTrue(processingSwitch.isEnabled)

        selectModel(withID: "quality48", in: app)
        XCTAssertEqual(modelButton.label, "quality48")

        XCTAssertTrue(saveButton.waitForExistence(timeout: 15))
        XCTAssertEqual(statusLabel.label, "OK")
        XCTAssertEqual(errorView.value as? String ?? "", "")
    }

    func testFileProcessingAndSaveWithoutErrors() throws {
        try runFileProcessingFlow(selectedModelID: nil)
    }

    func testFileProcessingAndSaveWithoutErrorsForFast8() throws {
        try runFileProcessingFlow(selectedModelID: "fast8")
    }

    func testFileProcessingAndSaveWithoutErrorsForQuality48() throws {
        try runFileProcessingFlow(selectedModelID: "quality48")
    }

    func testFileProcessingAndSaveWithoutErrorsForHybrid48() throws {
        try runFileProcessingFlow(selectedModelID: "hybrid48")
    }

    private func runFileProcessingFlow(selectedModelID: String?) throws {
        let app = try launchApp()

        let modelButton = app.buttons["modelButton"]
        XCTAssertTrue(modelButton.waitForExistence(timeout: 5))
        XCTAssertEqual(modelButton.label, "Off")

        if let selectedModelID {
            selectModel(withID: selectedModelID, in: app)
            XCTAssertEqual(modelButton.label, selectedModelID)
        }

        let fileButton = app.buttons["fileButton"]
        XCTAssertTrue(fileButton.waitForExistence(timeout: 5))
        let audioProcessingTelemetryLabel = app.staticTexts["audioProcessingTelemetryLabel"]
        let denoiserTelemetryLabel = app.staticTexts["denoiserTelemetryLabel"]
        XCTAssertTrue(audioProcessingTelemetryLabel.waitForExistence(timeout: 5))
        XCTAssertTrue(denoiserTelemetryLabel.waitForExistence(timeout: 5))
        let lowPerformanceLabel = app.staticTexts["lowPerformanceLabel"]
        XCTAssertTrue(!lowPerformanceLabel.exists || !lowPerformanceLabel.isHittable)
        fileButton.tap()

        XCTAssertTrue(waitForTelemetryUpdate(in: audioProcessingTelemetryLabel, timeout: 15))

        if selectedModelID != nil {
            XCTAssertTrue(waitForTelemetryUpdate(in: denoiserTelemetryLabel, timeout: 15))
        }

        let saveButton = app.buttons["saveButton"]
        XCTAssertTrue(saveButton.waitForExistence(timeout: 15))
        saveButton.tap()

        let statusLabel = app.staticTexts["statusLabel"]
        XCTAssertEqual(statusLabel.label, "OK")

        let errorView = app.textViews["errorView"]
        XCTAssertEqual(errorView.value as? String ?? "", "")
    }

    @discardableResult
    private func launchApp() throws -> XCUIApplication {
        let app = XCUIApplication()
        app.launchArguments.append("--uitest")

        guard let inputURL = Bundle(for: Self.self).url(forResource: "UITestInput", withExtension: "wav") else {
            XCTFail("Missing UITestInput.wav fixture")
            throw FixtureError.missingInput
        }

        app.launchEnvironment["UITEST_INPUT_FILE"] = inputURL.path
        app.launch()
        return app
    }

    private func selectModel(withID modelID: String, in app: XCUIApplication) {
        let modelButton = app.buttons["modelButton"]
        modelButton.tap()

        let modelAction = app.buttons[modelID].firstMatch
        XCTAssertTrue(modelAction.waitForExistence(timeout: 5))
        modelAction.tap()
    }

    private func clearAndType(_ text: String, into field: XCUIElement) {
        field.tap()

        if let currentValue = field.value as? String, !currentValue.isEmpty {
            field.typeText(String(repeating: XCUIKeyboardKey.delete.rawValue, count: currentValue.count))
        }

        field.typeText(text)
    }

    private func waitForTelemetryUpdate(in label: XCUIElement, timeout: TimeInterval) -> Bool {
        let predicate = NSPredicate { evaluated, _ in
            guard let element = evaluated as? XCUIElement else { return false }
            return !element.label.hasSuffix("--")
        }
        let expectation = XCTNSPredicateExpectation(predicate: predicate, object: label)
        return XCTWaiter().wait(for: [expectation], timeout: timeout) == .completed
    }

    private enum FixtureError: Error {
        case missingInput
    }
}
