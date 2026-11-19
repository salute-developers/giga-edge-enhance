import Foundation

/// Pipeline-stage switches applied to a GigaEdgeEnhance processor or audio unit.
///
/// Unlike ``EnhanceSettings``, features can be changed at runtime through
/// ``EnhanceAVAudioUnit/setFeatures(_:)`` or
/// ``EnhanceProcessor/setFeatures(_:)``. The pipeline is rebuilt lazily on
/// the next worker chunk, so callers do not need to coordinate with the
/// render thread.
public struct EnhanceFeatures: Hashable, Sendable {
    /// Normalization-only pipeline (no denoiser). Suitable for low-latency
    /// playback on devices that cannot afford the denoiser cost.
    public static let standard = EnhanceFeatures(normalization: true, noiseSuppression: nil)

    /// Enables the built-in normalization pipeline.
    public var normalization: Bool
    /// Identifier of the bundled denoiser model to use, or `nil` to skip
    /// denoising. Use static descriptors such as
    /// ``EnhanceModelDescriptor/quality48`` or build a custom descriptor.
    public var noiseSuppression: EnhanceModelDescriptor?

    /// Creates a feature set.
    ///
    /// - Parameters:
    ///   - normalization: Whether to run the normalization stage.
    ///   - noiseSuppression: Denoiser model identifier, or `nil` to skip.
    public init(normalization: Bool, noiseSuppression: EnhanceModelDescriptor?) {
        self.normalization = normalization
        self.noiseSuppression = noiseSuppression
    }
}
