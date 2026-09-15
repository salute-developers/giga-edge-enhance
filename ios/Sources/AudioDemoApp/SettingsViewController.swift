import UIKit
import GigaDenoiserAudioKit

final class SettingsViewController: UIViewController {
    private let scrollView = UIScrollView()
    private let stackView = UIStackView()
    private let errorLabel = UILabel()
    private let customConfigView = UITextView()

    private let lowPerformanceWindowField = UITextField()
    private let lowPerformanceMaxRefillTimesField = UITextField()
    private let lowPerformanceMaxRefillTimeFactorField = UITextField()
    private let minLatencyField = UITextField()
    private let latencyIncreaseStepField = UITextField()
    private let maxProcessingChunkSecondsField = UITextField()
    private let telemetryRoundingMillisecondsField = UITextField()
    private let useVadNormalizationSwitch = UISwitch()
    private let reverseProcessingOrderSwitch = UISwitch()

    private let onSave: (Settings) -> Void

    init(settings: Settings, onSave: @escaping (Settings) -> Void) {
        self.onSave = onSave
        super.init(nibName: nil, bundle: nil)
        apply(settings: settings)
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .systemBackground
        navigationItem.title = "Settings"
        navigationItem.rightBarButtonItem = UIBarButtonItem(title: "Save",
                                                            style: .done,
                                                            target: self,
                                                            action: #selector(saveTapped))
        configureUI()
    }

    private func configureUI() {
        scrollView.translatesAutoresizingMaskIntoConstraints = false
        stackView.translatesAutoresizingMaskIntoConstraints = false
        stackView.axis = .vertical
        stackView.spacing = 16

        errorLabel.translatesAutoresizingMaskIntoConstraints = false
        errorLabel.font = .preferredFont(forTextStyle: .footnote)
        errorLabel.textColor = .systemRed
        errorLabel.numberOfLines = 0
        errorLabel.accessibilityIdentifier = "settingsErrorLabel"

        customConfigView.translatesAutoresizingMaskIntoConstraints = false
        customConfigView.font = .preferredFont(forTextStyle: .body)
        customConfigView.layer.borderWidth = 1
        customConfigView.layer.cornerRadius = 8
        customConfigView.layer.borderColor = UIColor.separator.cgColor
        customConfigView.accessibilityIdentifier = "customNormalizationConfigField"
        customConfigView.heightAnchor.constraint(equalToConstant: 120).isActive = true

        view.addSubview(scrollView)
        scrollView.addSubview(stackView)

        stackView.addArrangedSubview(makeTextFieldRow(title: "Window seconds",
                                                      field: lowPerformanceWindowField,
                                                      identifier: "lowPerformanceWindowField",
                                                      keyboardType: .decimalPad))
        stackView.addArrangedSubview(makeTextFieldRow(title: "Max refill times",
                                                      field: lowPerformanceMaxRefillTimesField,
                                                      identifier: "lowPerformanceMaxRefillTimesField",
                                                      keyboardType: .numberPad))
        stackView.addArrangedSubview(makeTextFieldRow(title: "Max refill time factor",
                                                      field: lowPerformanceMaxRefillTimeFactorField,
                                                      identifier: "lowPerformanceMaxRefillTimeFactorField",
                                                      keyboardType: .decimalPad))
        stackView.addArrangedSubview(makeTextFieldRow(title: "Min latency",
                                                      field: minLatencyField,
                                                      identifier: "minLatencyField",
                                                      keyboardType: .decimalPad))
        stackView.addArrangedSubview(makeTextFieldRow(title: "Latency increase step",
                                                       field: latencyIncreaseStepField,
                                                       identifier: "latencyIncreaseStepField",
                                                       keyboardType: .decimalPad))
        stackView.addArrangedSubview(makeTextFieldRow(title: "Max processing chunk seconds",
                                                       field: maxProcessingChunkSecondsField,
                                                       identifier: "maxProcessingChunkSecondsField",
                                                       keyboardType: .decimalPad))
        stackView.addArrangedSubview(makeTextFieldRow(title: "Telemetry rounding (ms)",
                                                      field: telemetryRoundingMillisecondsField,
                                                      identifier: "telemetryRoundingMillisecondsField",
                                                      keyboardType: .decimalPad))
        stackView.addArrangedSubview(makeSwitchRow(title: "Use VAD normalization",
                                                   toggle: useVadNormalizationSwitch,
                                                   identifier: "useVadNormalizationSwitch"))
        stackView.addArrangedSubview(makeSwitchRow(title: "Reverse processing order",
                                                   toggle: reverseProcessingOrderSwitch,
                                                   identifier: "reverseProcessingOrderSwitch"))

        let customConfigLabel = UILabel()
        customConfigLabel.text = "Custom normalization config"
        customConfigLabel.font = .preferredFont(forTextStyle: .headline)

        let hintLabel = UILabel()
        hintLabel.text = "Changes apply to the next live or file processing start. Leave empty to use generated defaults."
        hintLabel.font = .preferredFont(forTextStyle: .footnote)
        hintLabel.textColor = .secondaryLabel
        hintLabel.numberOfLines = 0

        stackView.addArrangedSubview(customConfigLabel)
        stackView.addArrangedSubview(customConfigView)
        stackView.addArrangedSubview(hintLabel)
        stackView.addArrangedSubview(errorLabel)

        NSLayoutConstraint.activate([
            scrollView.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor),
            scrollView.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor),
            scrollView.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor),
            scrollView.bottomAnchor.constraint(equalTo: view.bottomAnchor),

            stackView.leadingAnchor.constraint(equalTo: scrollView.contentLayoutGuide.leadingAnchor, constant: 16),
            stackView.trailingAnchor.constraint(equalTo: scrollView.contentLayoutGuide.trailingAnchor, constant: -16),
            stackView.topAnchor.constraint(equalTo: scrollView.contentLayoutGuide.topAnchor, constant: 24),
            stackView.bottomAnchor.constraint(equalTo: scrollView.contentLayoutGuide.bottomAnchor, constant: -24),
            stackView.widthAnchor.constraint(equalTo: scrollView.frameLayoutGuide.widthAnchor, constant: -32)
        ])
    }

    private func apply(settings: Settings) {
        lowPerformanceWindowField.text = Self.string(from: settings.lowPerformanceWindowSeconds)
        lowPerformanceMaxRefillTimesField.text = String(settings.lowPerformanceMaxRefillTimes)
        lowPerformanceMaxRefillTimeFactorField.text = Self.string(from: settings.lowPerformanceMaxRefillTimeFactor)
        minLatencyField.text = Self.string(from: settings.minLatency)
        latencyIncreaseStepField.text = Self.string(from: settings.latencyIncreaseStep)
        maxProcessingChunkSecondsField.text = Self.string(from: settings.maxProcessingChunkSeconds)
        telemetryRoundingMillisecondsField.text = Self.string(from: settings.telemetryRoundingMilliseconds)
        useVadNormalizationSwitch.isOn = settings.useVadNormalization
        reverseProcessingOrderSwitch.isOn = settings.unstableReverseProcessingOrder
        customConfigView.text = settings.unstableCustomNormalizationConfig ?? ""
    }

    @objc private func saveTapped() {
        do {
            let settings = try buildSettings()
            onSave(settings)
            navigationController?.popViewController(animated: true)
        } catch {
            errorLabel.text = error.localizedDescription
        }
    }

    private func buildSettings() throws -> Settings {
        let customConfig = customConfigView.text.trimmingCharacters(in: .whitespacesAndNewlines)

        let settings = Settings(
            lowPerformanceWindowSeconds: try requiredDouble(from: lowPerformanceWindowField, name: "Window seconds"),
            lowPerformanceMaxRefillTimes: try requiredInt(from: lowPerformanceMaxRefillTimesField, name: "Max refill times"),
            lowPerformanceMaxRefillTimeFactor: try requiredDouble(from: lowPerformanceMaxRefillTimeFactorField,
                                                                   name: "Max refill time factor"),
            minLatency: try requiredDouble(from: minLatencyField, name: "Min latency"),
            latencyIncreaseStep: try requiredDouble(from: latencyIncreaseStepField, name: "Latency increase step"),
            telemetryRoundingMilliseconds: try requiredDouble(from: telemetryRoundingMillisecondsField,
                                                               name: "Telemetry rounding"),
            useVadNormalization: useVadNormalizationSwitch.isOn,
            unstableReverseProcessingOrder: reverseProcessingOrderSwitch.isOn,
            unstableCustomNormalizationConfig: customConfig.isEmpty ? nil : customConfig,
            maxProcessingChunkSeconds: try requiredDouble(from: maxProcessingChunkSecondsField,
                                                           name: "Max processing chunk seconds")
        )

        return try settings.validated()
    }

    private func makeTextFieldRow(title: String,
                                  field: UITextField,
                                  identifier: String,
                                  keyboardType: UIKeyboardType) -> UIStackView {
        let label = UILabel()
        label.text = title
        label.font = .preferredFont(forTextStyle: .body)

        field.borderStyle = .roundedRect
        field.keyboardType = keyboardType
        field.autocorrectionType = .no
        field.autocapitalizationType = .none
        field.accessibilityIdentifier = identifier

        let stack = UIStackView(arrangedSubviews: [label, field])
        stack.axis = .vertical
        stack.spacing = 8
        return stack
    }

    private func makeSwitchRow(title: String,
                               toggle: UISwitch,
                               identifier: String) -> UIStackView {
        let label = UILabel()
        label.text = title
        label.font = .preferredFont(forTextStyle: .body)

        toggle.accessibilityIdentifier = identifier

        let row = UIStackView(arrangedSubviews: [label, toggle])
        row.axis = .horizontal
        row.alignment = .center
        row.distribution = .equalSpacing
        return row
    }

    private func requiredDouble(from field: UITextField, name: String) throws -> Double {
        guard let text = field.text?.trimmingCharacters(in: .whitespacesAndNewlines),
              !text.isEmpty,
              let value = Double(text) else {
            throw NSError(domain: "AudioDemoApp.Settings",
                          code: 1,
                          userInfo: [NSLocalizedDescriptionKey: "\(name) must be a number"])
        }
        return value
    }

    private func requiredInt(from field: UITextField, name: String) throws -> Int {
        guard let text = field.text?.trimmingCharacters(in: .whitespacesAndNewlines),
              !text.isEmpty,
              let value = Int(text) else {
            throw NSError(domain: "AudioDemoApp.Settings",
                          code: 1,
                          userInfo: [NSLocalizedDescriptionKey: "\(name) must be an integer"])
        }
        return value
    }

    private static func string(from value: Double) -> String {
        let formatter = NumberFormatter()
        formatter.locale = Locale(identifier: "en_US_POSIX")
        formatter.numberStyle = .decimal
        formatter.maximumFractionDigits = 6
        formatter.minimumFractionDigits = 0
        return formatter.string(from: NSNumber(value: value)) ?? String(value)
    }
}
