import AVFoundation

/// `AVAudioUnitEffect` wrapper that exposes GigaEdgeSpench processing through `AVAudioEngine`.
public final class DenoiserAVAudioUnit: AVAudioUnitEffect {
    /// Audio component description used to register and instantiate GigaEdgeSpench locally.
    public static let componentDescription = AudioComponentDescription(
        componentType: kAudioUnitType_Effect,
        componentSubType: 0x7370656E, // 'spen'
        componentManufacturer: 0x53504348, // 'SPCH'
        componentFlags: 0,
        componentFlagsMask: 0
    )

    // Registration must run exactly once per process. Swift `static let` is
    // initialised under a one-shot lock, which gives us thread-safe lazy
    // registration without an explicit lock + flag.
    private static let registrationToken: Void = {
        AUAudioUnit.registerSubclass(
            DenoiserAUAudioUnit.self,
            as: componentDescription,
            name: "Local GigaEdgeSpench Audio Unit",
            version: UInt32.max
        )
    }()

    private var typedAU: DenoiserAUAudioUnit? {
        auAudioUnit as? DenoiserAUAudioUnit
    }

    /// Creates and registers a GigaEdgeSpench audio unit using the provided component description.
    public override init(audioComponentDescription: AudioComponentDescription) {
        Self.register()
        super.init(audioComponentDescription: audioComponentDescription)
    }

    /// Creates and registers a GigaEdgeSpench audio unit using `componentDescription`.
    public override init() {
        Self.register()
        super.init(audioComponentDescription: Self.componentDescription)
    }

    /// Creates a configured GigaEdgeSpench audio unit for `AVAudioEngine` graphs.
    public convenience init(features: Features = .standard,
                            settings: Settings = .default) throws {
        self.init()
        // Settings are fixed for the lifetime of the underlying AU instance.
        try typedAU?.configureInitialSettings(settings)
        try typedAU?.setFeatures(features)
    }

    public required init?(coder: NSCoder) {
        return nil
    }

    /// Registers the local GigaEdgeSpench audio unit subclass exactly once per process.
    public static func register() {
        _ = registrationToken
    }

    /// Active GigaEdgeSpench features.
    public var features: Features {
        typedAU?.features ?? .standard
    }

    /// Validated settings captured at creation time.
    public var settings: Settings {
        typedAU?.settings ?? .default
    }

    /// Current processing sample rate.
    public var sampleRate: Double {
        typedAU?.sampleRate ?? 0
    }

    /// Cumulative processing telemetry.
    public var telemetry: Telemetry {
        typedAU?.telemetry ?? Telemetry(audioProcessingCounters: [:], denoiserCounters: [:])
    }

    /// Low-performance callback delivered on the main queue.
    public var onLowPerformance: ((LowPerformanceEvent) -> Void)? {
        get { typedAU?.onLowPerformance }
        set { typedAU?.onLowPerformance = newValue }
    }

    /// Processing-failure callback delivered on the main queue.
    public var onProcessingFailure: ((String) -> Void)? {
        get { typedAU?.onProcessingFailure }
        set { typedAU?.onProcessingFailure = newValue }
    }

    /// Updates the feature set used by the wrapped audio unit.
    public func setFeatures(_ features: Features) throws {
        try typedAU?.setFeatures(features)
    }

    public override func reset() {
        super.reset()
        typedAU?.reset()
    }
}
