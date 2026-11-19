import AVFoundation

/// `AVAudioUnitEffect` wrapper that exposes GigaEdgeEnhance processing through `AVAudioEngine`.
public final class EnhanceAVAudioUnit: AVAudioUnitEffect {
    /// Audio component description used to register and instantiate GigaEdgeEnhance locally.
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
            EnhanceAUAudioUnit.self,
            as: componentDescription,
            name: "Local GigaEdgeEnhance Audio Unit",
            version: UInt32.max
        )
    }()

    private var typedAU: EnhanceAUAudioUnit? {
        auAudioUnit as? EnhanceAUAudioUnit
    }

    /// Creates and registers a GigaEdgeEnhance audio unit using the provided component description.
    public override init(audioComponentDescription: AudioComponentDescription) {
        Self.register()
        super.init(audioComponentDescription: audioComponentDescription)
    }

    /// Creates and registers a GigaEdgeEnhance audio unit using `componentDescription`.
    public override init() {
        Self.register()
        super.init(audioComponentDescription: Self.componentDescription)
    }

    /// Creates a configured GigaEdgeEnhance audio unit for `AVAudioEngine` graphs.
    public convenience init(features: EnhanceFeatures = .standard,
                            settings: EnhanceSettings = .default) throws {
        self.init()
        // EnhanceSettings are fixed for the lifetime of the underlying AU instance.
        try typedAU?.configureInitialSettings(settings)
        try typedAU?.setFeatures(features)
    }

    public required init?(coder: NSCoder) {
        return nil
    }

    /// Registers the local GigaEdgeEnhance audio unit subclass exactly once per process.
    public static func register() {
        _ = registrationToken
    }

    /// Active GigaEdgeEnhance features.
    public var features: EnhanceFeatures {
        typedAU?.features ?? .standard
    }

    /// Validated settings captured at creation time.
    public var settings: EnhanceSettings {
        typedAU?.settings ?? .default
    }

    /// Current processing sample rate.
    public var sampleRate: Double {
        typedAU?.sampleRate ?? 0
    }

    /// Cumulative processing telemetry.
    public var telemetry: EnhanceTelemetry {
        typedAU?.telemetry ?? EnhanceTelemetry(audioProcessingCounters: [:], denoiserCounters: [:])
    }

    /// Low-performance callback delivered on the main queue.
    public var onLowPerformance: ((EnhanceLowPerformanceEvent) -> Void)? {
        get { typedAU?.onLowPerformance }
        set { typedAU?.onLowPerformance = newValue }
    }

    /// Processing-failure callback delivered on the main queue.
    public var onProcessingFailure: ((String) -> Void)? {
        get { typedAU?.onProcessingFailure }
        set { typedAU?.onProcessingFailure = newValue }
    }

    /// Updates the feature set used by the wrapped audio unit.
    public func setFeatures(_ features: EnhanceFeatures) throws {
        try typedAU?.setFeatures(features)
    }

    public override func reset() {
        super.reset()
        typedAU?.reset()
    }
}
