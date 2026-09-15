import Foundation

/// Errors thrown by GigaDenoiserAudioKit's Swift API and bridged to `NSError`
/// with domain ``DenoiserError/errorDomain``.
///
/// The raw error codes match the C `DenoiserErrorCode` enum surfaced from the
/// underlying ObjC++ bridge, so a `NSError` produced anywhere inside the
/// framework can be inspected by either ``DenoiserError`` pattern matching or
/// by reading `(error as NSError).code` against ``DenoiserError/Code``.
public enum DenoiserError: Error {
    /// One of the supplied arguments failed validation.
    case invalidArgument(String)
    /// A value was outside the supported range.
    case outOfRange(String)
    /// An unexpected failure occurred while invoking the underlying pipeline.
    case unknown(String)

    /// Numeric codes shared with the C `DenoiserErrorCode` enum.
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

extension DenoiserError: CustomNSError {
    /// Domain shared with the ObjC++ bridge's `DenoiserErrorDomain`.
    public static let errorDomain = "com.gigaedge.DenoiserAudioKit.DenoiserError"

    public var errorCode: Int { code.rawValue }

    public var errorUserInfo: [String: Any] {
        [NSLocalizedDescriptionKey: message]
    }
}

extension DenoiserError: LocalizedError {
    public var errorDescription: String? { message }
}
