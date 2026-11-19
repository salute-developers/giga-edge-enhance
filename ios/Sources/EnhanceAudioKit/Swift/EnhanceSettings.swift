import Foundation

/// Configuration values that shape how a GigaEdgeEnhance audio unit instance behaves.
///
/// EnhanceSettings are validated when you create a ``EnhanceProcessor``,
/// ``EnhanceAUAudioUnit``, or ``EnhanceAVAudioUnit``. They are fixed for
/// the lifetime of that instance: trying to apply new settings after
/// ``EnhanceAUAudioUnit/allocateRenderResources()`` throws
/// ``EnhanceError/invalidArgument(_:)``.
public struct EnhanceSettings: Hashable, Sendable {
    /// Conservative defaults suitable for typical voice-call workloads.
    public static let `default` = EnhanceSettings()

    /// Sliding window, in seconds, over which repeated render-thread refills
    /// are counted before a ``EnhanceLowPerformanceEvent`` fires.
    public var lowPerformanceWindowSeconds: Double
    /// Number of refills inside ``lowPerformanceWindowSeconds`` that must be
    /// observed before a low-performance signal is emitted.
    public var lowPerformanceMaxRefillTimes: Int
    /// Multiplier (over the worst observed worker chunk time) that gates
    /// reaction speed to recurrent congestion. Higher values trade off
    /// quicker reaction for more spurious events.
    public var lowPerformanceMaxRefillTimeFactor: Double
    /// Minimum target output buffering, in seconds, used when priming the
    /// processed-output ring after start or reset.
    public var minLatency: Double
    /// Increment, in seconds, added to the target latency after each
    /// detected underrun until the worker can keep up again.
    public var latencyIncreaseStep: Double
    /// Maximum duration, in seconds, of one real-time audio-unit worker
    /// processing chunk. The effective duration is capped at ``minLatency``
    /// and 256 ms, quantized down to 16 ms steps, and never falls below 16 ms.
    /// This setting does not change direct ``EnhanceProcessor`` calls.
    public var maxProcessingChunkSeconds: Double
    /// Histogram bucket size, in milliseconds, used by ``EnhanceTelemetry``.
    /// Smaller values capture more detail but use more memory.
    public var telemetryRoundingMilliseconds: Double
    /// Enables VAD-driven normalization parameters in the generated YAML
    /// config. Only meaningful when ``EnhanceFeatures/normalization`` is on.
    public var useVadNormalization: Bool
    /// Runs denoiser before normalizer instead of the default order.
    /// Marked unstable: behaviour may change between releases.
    public var unstableReverseProcessingOrder: Bool
    /// Replaces the generated normalization config with a verbatim YAML
    /// string. Marked unstable; use only for tuning experiments.
    public var unstableCustomNormalizationConfig: String?

    /// Creates settings for a new GigaEdgeEnhance processor or audio unit.
    public init(lowPerformanceWindowSeconds: Double = 1,
                lowPerformanceMaxRefillTimes: Int = 4,
                lowPerformanceMaxRefillTimeFactor: Double = 3,
                minLatency: Double = 0.064,
                latencyIncreaseStep: Double = 0.032,
                telemetryRoundingMilliseconds: Double = 0.25,
                useVadNormalization: Bool = false,
                unstableReverseProcessingOrder: Bool = false,
                unstableCustomNormalizationConfig: String? = nil,
                maxProcessingChunkSeconds: Double = 0.016) {
        self.lowPerformanceWindowSeconds = lowPerformanceWindowSeconds
        self.lowPerformanceMaxRefillTimes = lowPerformanceMaxRefillTimes
        self.lowPerformanceMaxRefillTimeFactor = lowPerformanceMaxRefillTimeFactor
        self.minLatency = minLatency
        self.latencyIncreaseStep = latencyIncreaseStep
        self.maxProcessingChunkSeconds = maxProcessingChunkSeconds
        self.telemetryRoundingMilliseconds = telemetryRoundingMilliseconds
        self.useVadNormalization = useVadNormalization
        self.unstableReverseProcessingOrder = unstableReverseProcessingOrder
        self.unstableCustomNormalizationConfig = unstableCustomNormalizationConfig
    }

    /// Validates the receiver and returns it unchanged when all values are supported.
    ///
    /// Use this before persisting or applying settings from UI input.
    public func validated() throws -> EnhanceSettings {
        guard lowPerformanceWindowSeconds > 0, lowPerformanceWindowSeconds.isFinite else {
            throw EnhanceError.invalidArgument("lowPerformanceWindowSeconds must be greater than zero")
        }
        guard lowPerformanceMaxRefillTimes > 0 else {
            throw EnhanceError.invalidArgument("lowPerformanceMaxRefillTimes must be greater than zero")
        }
        guard lowPerformanceMaxRefillTimeFactor >= 0, lowPerformanceMaxRefillTimeFactor.isFinite else {
            throw EnhanceError.invalidArgument("lowPerformanceMaxRefillTimeFactor must be finite and non-negative")
        }
        guard minLatency >= 0, minLatency.isFinite else {
            throw EnhanceError.invalidArgument("minLatency must be finite and non-negative")
        }
        guard latencyIncreaseStep >= 0, latencyIncreaseStep.isFinite else {
            throw EnhanceError.invalidArgument("latencyIncreaseStep must be finite and non-negative")
        }
        guard maxProcessingChunkSeconds > 0, maxProcessingChunkSeconds.isFinite else {
            throw EnhanceError.invalidArgument("maxProcessingChunkSeconds must be finite and greater than zero")
        }
        guard telemetryRoundingMilliseconds > 0, telemetryRoundingMilliseconds.isFinite else {
            throw EnhanceError.invalidArgument("telemetryRoundingMilliseconds must be greater than zero")
        }

        return self
    }
}

/// Payload delivered when the GigaEdgeEnhance audio unit detects repeated output
/// underruns.
///
/// The event is emitted both as a closure callback
/// (``EnhanceAVAudioUnit/onLowPerformance``) and a `Notification`
/// (``Foundation/NSNotification/Name/gigaEdgeEnhanceLowPerformance``) on the main
/// queue. Use it to surface diagnostic UI, log degradation, or back off
/// optional features.
public struct EnhanceLowPerformanceEvent: Sendable {
    /// Pipeline sample rate active when the event was emitted.
    public let sampleRate: Double
    /// Target output latency, in seconds, after the unit increased the
    /// buffer to compensate for the underrun.
    public let currentLatency: Double
    /// Mean worker processing time, in milliseconds, observed in the
    /// sliding window leading up to the event. `nil` if no chunk has been
    /// processed yet.
    public let meanProcessingTimeMilliseconds: Double?
    /// Number of refill events counted in the active detection window.
    public let refillCount: Int

    /// Creates a low-performance event. Normally constructed by the audio
    /// unit; tests and previews may instantiate it directly.
    public init(sampleRate: Double,
                currentLatency: Double,
                meanProcessingTimeMilliseconds: Double?,
                refillCount: Int) {
        self.sampleRate = sampleRate
        self.currentLatency = currentLatency
        self.meanProcessingTimeMilliseconds = meanProcessingTimeMilliseconds
        self.refillCount = refillCount
    }
}

/// Notification names emitted by the GigaEdgeEnhance audio unit.
public extension Notification.Name {
    /// Posted on the main queue when ``EnhanceAUAudioUnit`` detects repeated
    /// output underruns. The notification's `object` is the emitting
    /// audio unit; its `userInfo` contains the event under
    /// ``NotificationUserInfoKey/lowPerformanceEvent``.
    static let gigaEdgeEnhanceLowPerformance = Notification.Name("EnhanceAudioKit.lowPerformance")
}

/// User-info keys attached to GigaEdgeEnhance notifications.
public enum NotificationUserInfoKey {
    /// Key for ``EnhanceLowPerformanceEvent`` payloads on the
    /// ``Foundation/NSNotification/Name/gigaEdgeEnhanceLowPerformance`` notification.
    public static let lowPerformanceEvent = "EnhanceLowPerformanceEvent"
}
