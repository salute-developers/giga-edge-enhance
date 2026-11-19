import Foundation

/// Errors thrown by EnhanceAudioKit's Swift API and bridged to `NSError`
/// with domain ``EnhanceError/errorDomain``.
///
/// The raw error codes match the C `EnhanceErrorCode` enum surfaced from the
/// underlying ObjC++ bridge, so a `NSError` produced anywhere inside the
/// framework can be inspected by either ``EnhanceError`` pattern matching or
/// by reading `(error as NSError).code` against ``EnhanceError/Code``.
public enum EnhanceError: Error {
    /// One of the supplied arguments failed validation.
    case invalidArgument(String)
    /// A value was outside the supported range.
    case outOfRange(String)
    /// An unexpected failure occurred while invoking the underlying pipeline.
    case unknown(String)

    /// Numeric codes shared with the C `EnhanceErrorCode` enum.
    public enum Code: Int, Sendable {
        case invalidArgument = 1
        case outOfRange = 2
        case unknown = 999
    }

    /// Human-readable description suitable for logging or surfacing to users.
    public var message: String {
        switch self {
        case .invalidArgument(let message),
             .outOfRange(let message),
             .unknown(let message):
            return message
        }
    }

    /// Code that matches the bridged `NSError.code`.
    public var code: Code {
        switch self {
        case .invalidArgument: return .invalidArgument
        case .outOfRange: return .outOfRange
        case .unknown: return .unknown
        }
    }
}

extension EnhanceError: CustomNSError {
    /// Domain shared with the ObjC++ bridge's `EnhanceErrorDomain`.
    public static let errorDomain = "com.gigaedge.EnhanceAudioKit.EnhanceError"

    public var errorCode: Int { code.rawValue }

    public var errorUserInfo: [String: Any] {
        [NSLocalizedDescriptionKey: message]
    }
}

extension EnhanceError: LocalizedError {
    public var errorDescription: String? { message }
}
