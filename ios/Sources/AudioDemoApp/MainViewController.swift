import UIKit
import UniformTypeIdentifiers
import EnhanceAudioKit

final class MainViewController: UIViewController {
    private let titleLabel = UILabel()
    private let modelLabel = UILabel()
    private let modelButton = UIButton(type: .system)
    private let processingLabel = UILabel()
    private let processingSwitch = UISwitch()
    private let liveButton = UIButton(type: .system)
    private let fileButton = UIButton(type: .system)
    private let saveButton = UIButton(type: .system)
    private let statusLabel = UILabel()
    private let lowPerformanceLabel = UILabel()
    private let audioProcessingTelemetryLabel = UILabel()
    private let denoiserTelemetryLabel = UILabel()
    private let errorView = UITextView()

    private let liveController = LiveAudioController()
    private let fileController = FileProcessingController()

    private var liveRunning = false
    private var fileProcessing = false
    private var processedFileURL: URL?
    private var availableModels: [EnhanceModelDescriptor] = []
    private var selectedModel: EnhanceModelDescriptor?
    private var currentSettings = EnhanceSettings.default
    private var processingFailureMessage: String?
    private var telemetryTimer: Timer?
    private var previousTelemetry = EnhanceTelemetry.empty

    private var currentFeatures: EnhanceFeatures {
        EnhanceFeatures(normalization: processingSwitch.isOn,
                       noiseSuppression: selectedModel)
    }

    private var uiTestInputURL: URL? {
        guard let rawPath = ProcessInfo.processInfo.environment["UITEST_INPUT_FILE"] else { return nil }
        return URL(fileURLWithPath: rawPath)
    }

    private var isUITestMode: Bool {
        ProcessInfo.processInfo.arguments.contains("--uitest")
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .systemBackground
        navigationItem.title = "Audio Demo"
        navigationItem.rightBarButtonItem = UIBarButtonItem(title: "EnhanceSettings",
                                                            style: .plain,
                                                            target: self,
                                                            action: #selector(openSettings))
        configureUI()
        liveController.onProcessingFailure = { [weak self] message in
            self?.showProcessingFailure(message)
        }
        fileController.onProcessingFailure = { [weak self] message in
            self?.showProcessingFailure(message)
        }
        updateStatus(ok: true, message: nil)
        loadModels()
    }

    private func configureUI() {
        titleLabel.translatesAutoresizingMaskIntoConstraints = false
        titleLabel.text = "Realtime GigaEdgeEnhance EnhanceProcessor"
        titleLabel.font = .preferredFont(forTextStyle: .title2)
        titleLabel.textAlignment = .center

        modelLabel.translatesAutoresizingMaskIntoConstraints = false
        modelLabel.text = "Denoise model"
        modelLabel.font = .preferredFont(forTextStyle: .body)

        modelButton.translatesAutoresizingMaskIntoConstraints = false
        modelButton.accessibilityIdentifier = "modelButton"
        modelButton.showsMenuAsPrimaryAction = true

        processingLabel.translatesAutoresizingMaskIntoConstraints = false
        processingLabel.text = "GigaEdgeEnhance audio processing"
        processingLabel.font = .preferredFont(forTextStyle: .body)

        processingSwitch.translatesAutoresizingMaskIntoConstraints = false
        processingSwitch.isOn = true
        processingSwitch.accessibilityIdentifier = "audioProcessingSwitch"
        processingSwitch.addTarget(self, action: #selector(audioProcessingChanged), for: .valueChanged)

        liveButton.translatesAutoresizingMaskIntoConstraints = false
        liveButton.setTitle("Live Demo", for: .normal)
        liveButton.accessibilityIdentifier = "liveButton"
        liveButton.addTarget(self, action: #selector(toggleLive), for: .touchUpInside)

        fileButton.translatesAutoresizingMaskIntoConstraints = false
        fileButton.setTitle("Choose File", for: .normal)
        fileButton.accessibilityIdentifier = "fileButton"
        fileButton.addTarget(self, action: #selector(toggleFileProcessing), for: .touchUpInside)

        saveButton.translatesAutoresizingMaskIntoConstraints = false
        saveButton.setTitle("Save Output", for: .normal)
        saveButton.accessibilityIdentifier = "saveButton"
        saveButton.isHidden = true
        saveButton.addTarget(self, action: #selector(saveOutput), for: .touchUpInside)

        statusLabel.translatesAutoresizingMaskIntoConstraints = false
        statusLabel.text = "OK"
        statusLabel.font = .preferredFont(forTextStyle: .headline)
        statusLabel.accessibilityIdentifier = "statusLabel"

        lowPerformanceLabel.translatesAutoresizingMaskIntoConstraints = false
        lowPerformanceLabel.text = "Low Performance"
        lowPerformanceLabel.font = .preferredFont(forTextStyle: .headline)
        lowPerformanceLabel.textColor = .systemRed
        lowPerformanceLabel.textAlignment = .center
        lowPerformanceLabel.accessibilityIdentifier = "lowPerformanceLabel"
        lowPerformanceLabel.isHidden = true

        audioProcessingTelemetryLabel.translatesAutoresizingMaskIntoConstraints = false
        audioProcessingTelemetryLabel.font = .preferredFont(forTextStyle: .caption1)
        audioProcessingTelemetryLabel.textColor = .secondaryLabel
        audioProcessingTelemetryLabel.numberOfLines = 0
        audioProcessingTelemetryLabel.accessibilityIdentifier = "audioProcessingTelemetryLabel"

        denoiserTelemetryLabel.translatesAutoresizingMaskIntoConstraints = false
        denoiserTelemetryLabel.font = .preferredFont(forTextStyle: .caption1)
        denoiserTelemetryLabel.textColor = .secondaryLabel
        denoiserTelemetryLabel.numberOfLines = 0
        denoiserTelemetryLabel.accessibilityIdentifier = "denoiserTelemetryLabel"
        updateTelemetryLabels(with: .empty)

        errorView.translatesAutoresizingMaskIntoConstraints = false
        errorView.isEditable = false
        errorView.font = .preferredFont(forTextStyle: .footnote)
        errorView.accessibilityIdentifier = "errorView"

        let processingRow = UIStackView(arrangedSubviews: [processingLabel, processingSwitch])
        processingRow.axis = .horizontal
        processingRow.alignment = .center
        processingRow.distribution = .equalSpacing

        let modelRow = UIStackView(arrangedSubviews: [modelLabel, modelButton])
        modelRow.axis = .horizontal
        modelRow.alignment = .center
        modelRow.distribution = .equalSpacing

        let stack = UIStackView(arrangedSubviews: [titleLabel,
                                                   modelRow,
                                                   processingRow,
                                                   liveButton,
                                                   fileButton,
                                                   saveButton,
                                                   statusLabel,
                                                   lowPerformanceLabel,
                                                   audioProcessingTelemetryLabel,
                                                   denoiserTelemetryLabel,
                                                   errorView])
        stack.axis = .vertical
        stack.spacing = 12
        stack.translatesAutoresizingMaskIntoConstraints = false

        view.addSubview(stack)

        NSLayoutConstraint.activate([
            stack.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor, constant: 16),
            stack.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor, constant: -16),
            stack.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 24),
            errorView.heightAnchor.constraint(equalToConstant: 120)
        ])
    }

    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        startTelemetryTimerIfNeeded()
    }

    override func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated)
        stopTelemetryTimer()
    }

    @objc private func toggleLive() {
        if liveRunning {
            liveController.stop()
            liveRunning = false
            liveButton.setTitle("Live Demo", for: .normal)
            resetTelemetryDisplay()
            resetLowPerformanceIndicator()
            processingFailureMessage = nil
            updateStatus(ok: true, message: nil)
            return
        }

        resetLowPerformanceIndicator()
        resetTelemetryDisplay()
        processingFailureMessage = nil
        liveController.start(features: currentFeatures,
                              settings: currentSettings,
                              onLowPerformance: { [weak self] _ in
                                  self?.showLowPerformanceIndicator()
                              }) { [weak self] error in
            guard let self else { return }
            if let error {
                self.updateStatus(ok: false, message: error.localizedDescription)
                return
            }
            self.liveRunning = true
            self.liveButton.setTitle("Stop Live Demo", for: .normal)
            self.resetTelemetryDisplay()
            self.updateStatus(ok: true, message: nil)
        }
    }

    @objc private func audioProcessingChanged() {
        applyCurrentFeatures()
    }

    @objc private func toggleFileProcessing() {
        if fileProcessing {
            fileController.cancel()
            fileProcessing = false
            fileButton.setTitle("Choose File", for: .normal)
            resetTelemetryDisplay()
            resetLowPerformanceIndicator()
            processingFailureMessage = nil
            updateStatus(ok: true, message: nil)
            return
        }

        if let uiTestInputURL, isUITestMode {
            startFileProcessing(url: uiTestInputURL, features: currentFeatures)
            return
        }

        let picker = UIDocumentPickerViewController(forOpeningContentTypes: [UTType.audio])
        picker.delegate = self
        picker.allowsMultipleSelection = false
        present(picker, animated: true)
    }

    @objc private func saveOutput() {
        guard let processedFileURL else { return }

        if isUITestMode {
            let destination = FileManager.default.temporaryDirectory.appendingPathComponent("ui-test-saved.wav")
            do {
                if FileManager.default.fileExists(atPath: destination.path) {
                    try FileManager.default.removeItem(at: destination)
                }
                try FileManager.default.copyItem(at: processedFileURL, to: destination)
                updateStatus(ok: true, message: nil)
            } catch {
                updateStatus(ok: false, message: error.localizedDescription)
            }
            return
        }

        let controller = UIActivityViewController(activityItems: [processedFileURL], applicationActivities: nil)
        present(controller, animated: true)
    }

    @objc private func openSettings() {
        let controller = SettingsViewController(settings: currentSettings) { [weak self] settings in
            self?.currentSettings = settings
            self?.updateStatus(ok: true, message: nil)
        }
        navigationController?.pushViewController(controller, animated: true)
    }

    private func startFileProcessing(url: URL, features: EnhanceFeatures) {
        fileProcessing = true
        fileButton.setTitle("Cancel Processing", for: .normal)
        saveButton.isHidden = true
        resetTelemetryDisplay()
        resetLowPerformanceIndicator()
        processingFailureMessage = nil
        updateStatus(ok: true, message: nil)

        fileController.process(inputURL: url,
                               features: features,
                                settings: currentSettings,
                                onLowPerformance: { [weak self] _ in
                                    self?.showLowPerformanceIndicator()
                                }) { [weak self] result in
            guard let self else { return }
            self.fileProcessing = false
            self.fileButton.setTitle("Choose File", for: .normal)

            switch result {
            case .success(let outputURL):
                self.processedFileURL = outputURL
                self.saveButton.isHidden = false
                self.refreshTelemetrySnapshot()
                if let processingFailureMessage = self.processingFailureMessage {
                    self.updateStatus(ok: false, message: processingFailureMessage)
                } else {
                    self.updateStatus(ok: true, message: nil)
                }
            case .failure(let error):
                self.processedFileURL = nil
                self.saveButton.isHidden = true
                self.resetTelemetryDisplay()
                self.updateStatus(ok: false, message: error.localizedDescription)
            }
        }
    }

    private func updateStatus(ok: Bool, message: String?) {
        statusLabel.text = ok ? "OK" : "FAIL"
        statusLabel.textColor = ok ? .systemGreen : .systemRed
        errorView.text = message ?? ""
    }

    private func resetLowPerformanceIndicator() {
        lowPerformanceLabel.isHidden = true
    }

    private func startTelemetryTimerIfNeeded() {
        guard telemetryTimer == nil else { return }

        telemetryTimer = Timer.scheduledTimer(withTimeInterval: 1, repeats: true) { [weak self] _ in
            self?.refreshTelemetrySnapshot()
        }
        if let telemetryTimer {
            RunLoop.main.add(telemetryTimer, forMode: .common)
        }
    }

    private func stopTelemetryTimer() {
        telemetryTimer?.invalidate()
        telemetryTimer = nil
    }

    private func resetTelemetryDisplay() {
        previousTelemetry = activeTelemetry
        updateTelemetryLabels(with: .empty)
    }

    private func refreshTelemetrySnapshot() {
        let currentTelemetry = activeTelemetry
        let delta = currentTelemetry - previousTelemetry
        previousTelemetry = currentTelemetry
        updateTelemetryLabels(with: delta)
    }

    private var activeTelemetry: EnhanceTelemetry {
        if liveRunning {
            return liveController.telemetry
        }
        if fileProcessing || processedFileURL != nil {
            return fileController.telemetry
        }
        return .empty
    }

    private func updateTelemetryLabels(with telemetry: EnhanceTelemetry) {
        audioProcessingTelemetryLabel.text = makeTelemetryText(title: "Audio processing mean",
                                                               value: telemetry.audioProcessingMeanMilliseconds)
        denoiserTelemetryLabel.text = makeTelemetryText(title: "Denoising mean",
                                                        value: telemetry.denoiserMeanMilliseconds)
    }

    private func makeTelemetryText(title: String, value: Double?) -> String {
        guard let value else { return "\(title): --" }
        return "\(title): \(String(format: "%.2f", value)) ms"
    }

    private func showLowPerformanceIndicator() {
        DispatchQueue.main.async { [weak self] in
            self?.lowPerformanceLabel.isHidden = false
        }
    }

    private func showProcessingFailure(_ message: String) {
        processingFailureMessage = message
        updateStatus(ok: false, message: message)
    }

    private func loadModels() {
        do {
            availableModels = try EnhanceModelCatalog.availableModels()
            refreshModelMenu()
            applyModelSelection(nil)
        } catch {
            availableModels = []
            refreshModelMenu()
            applyModelSelection(nil)
            updateStatus(ok: false, message: error.localizedDescription)
        }
    }

    private func refreshModelMenu() {
        var actions = [UIAction(title: "Off", state: selectedModel == nil ? .on : .off) { [weak self] _ in
            self?.applyModelSelection(nil)
        }]

        actions.append(contentsOf: availableModels.map { model in
            UIAction(title: model.rawValue, state: model == selectedModel ? .on : .off) { [weak self] _ in
                self?.applyModelSelection(model)
            }
        })

        modelButton.menu = UIMenu(title: "", options: .displayInline, children: actions)
        modelButton.setTitle(selectedModel?.rawValue ?? "Off", for: .normal)
    }

    private func applyModelSelection(_ model: EnhanceModelDescriptor?) {
        selectedModel = model
        refreshModelMenu()
        applyCurrentFeatures()
    }

    private func applyCurrentFeatures() {
        do {
            if liveRunning {
                try liveController.setFeatures(currentFeatures)
            }
            if fileProcessing {
                try fileController.setFeatures(currentFeatures)
            }
            updateStatus(ok: true, message: nil)
        } catch {
            updateStatus(ok: false, message: error.localizedDescription)
        }
    }
}

extension MainViewController: UIDocumentPickerDelegate {
    func documentPicker(_ controller: UIDocumentPickerViewController, didPickDocumentsAt urls: [URL]) {
        guard let url = urls.first else { return }
        startFileProcessing(url: url, features: currentFeatures)
    }

    func documentPickerWasCancelled(_ controller: UIDocumentPickerViewController) {
        updateStatus(ok: true, message: nil)
    }
}
