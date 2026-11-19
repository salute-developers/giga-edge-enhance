import Foundation

/// Stable identifier for a bundled GigaEdgeEnhance denoiser model.
///
/// Use one of the bundled descriptors (``fast8``, ``quality48``, ``hybrid48``)
/// or build your own with ``init(rawValue:)``. The raw value must match the
/// `EnhanceModelID` key inside the model bundle's `Info.plist`.
public struct EnhanceModelDescriptor: RawRepresentable, Hashable, Comparable, Sendable {
    public let rawValue: String

    /// Creates a descriptor for an arbitrary `EnhanceModelID`.
    public init(rawValue: String) {
        self.rawValue = rawValue
    }

    public static func < (lhs: EnhanceModelDescriptor, rhs: EnhanceModelDescriptor) -> Bool {
        lhs.rawValue < rhs.rawValue
    }

    /// Lightweight 8 kHz model. Lowest CPU cost; recommended for narrow-band
    /// telephony scenarios on older devices.
    public static let fast8 = EnhanceModelDescriptor(rawValue: "fast8")
    /// Full-quality 48 kHz model. Highest fidelity at the cost of CPU; best
    /// for offline rendering or when hardware can sustain it.
    public static let quality48 = EnhanceModelDescriptor(rawValue: "quality48")
    /// Hybrid 48 kHz model.
    public static let hybrid48 = EnhanceModelDescriptor(rawValue: "hybrid48")
}

/// Discovers GigaEdgeEnhance model bundles packaged with an application or framework.
///
/// The catalog scans the main bundle, the framework bundle, and every
/// loaded bundle/framework for `.bundle` resources whose `Info.plist`
/// declares a `EnhanceModelID`. Discovery results are cached for the lifetime
/// of the process; call ``invalidateCache()`` when bundles are dynamically
/// added or removed at runtime.
public enum EnhanceModelCatalog {
    /// Returns all installed GigaEdgeEnhance model identifiers sorted by raw value.
    ///
    /// - Throws: ``EnhanceError/invalidArgument(_:)`` if a bundle declares a
    ///   `EnhanceModelID` but is missing required metadata, or if distinct
    ///   bundle products declare the same `EnhanceModelID`.
    public static func availableModels() throws -> [EnhanceModelDescriptor] {
        try modelEntries()
            .map(\.id)
            .sorted()
    }

    /// Drops the in-memory cache of discovered model bundles.
    ///
    /// Call this only when bundles are dynamically added or removed at
    /// runtime (e.g. on-demand resources). Discovery is otherwise cached
    /// for the lifetime of the process.
    public static func invalidateCache() {
        cacheLock.withLock {
            cachedEntries = nil
        }
    }

    static func modelURL(forID id: EnhanceModelDescriptor) throws -> URL {
        let entries = try modelEntries()

        guard let entry = entries.first(where: { $0.id == id }) else {
            let availableIDs = entries.map(\.id.rawValue).sorted().joined(separator: ", ")
            let description = availableIDs.isEmpty
                ? "No GigaEdgeEnhance model bundles are installed"
                : "Unknown GigaEdgeEnhance model id \(id.rawValue). Available models: \(availableIDs)"
            throw EnhanceError.invalidArgument(description)
        }

        return entry.url
    }

    private static func modelEntries() throws -> [ModelEntry] {
        if let cached = cacheLock.withLock({ cachedEntries }) {
            return cached
        }

        let entries = try modelEntries(in: candidateBundleURLs())
        cacheLock.withLock {
            cachedEntries = entries
        }
        return entries
    }

    static func availableModels(in bundleURLs: [URL]) throws -> [EnhanceModelDescriptor] {
        try modelEntries(in: bundleURLs)
            .map(\.id)
            .sorted()
    }

    private static func modelEntries(in bundleURLs: [URL]) throws -> [ModelEntry] {
        var entriesByID: [EnhanceModelDescriptor: ModelEntry] = [:]

        for bundleURL in bundleURLs.sorted(by: { $0.path < $1.path }) {
            guard let bundle = Bundle(url: bundleURL) else { continue }
            guard let entry = try modelEntry(in: bundle) else { continue }

            if let existingEntry = entriesByID[entry.id] {
                // XCTest can expose copies of one bundle product through multiple discovery roots.
                if let bundleIdentifier = entry.bundleIdentifier,
                   !bundleIdentifier.isEmpty,
                   existingEntry.bundleIdentifier == bundleIdentifier {
                    continue
                }
                let description = "Duplicate GigaEdgeEnhance model id \(entry.id.rawValue) in bundles " +
                    "\(existingEntry.bundleURL.lastPathComponent) and \(entry.bundleURL.lastPathComponent)"
                throw EnhanceError.invalidArgument(description)
            }

            entriesByID[entry.id] = entry
        }

        return Array(entriesByID.values)
    }

    private static func modelEntry(in bundle: Bundle) throws -> ModelEntry? {
        guard let modelIDValue = bundle.object(forInfoDictionaryKey: MetadataKey.id) as? String else {
            return nil
        }
        let modelID = EnhanceModelDescriptor(rawValue: modelIDValue)
        guard !modelID.rawValue.isEmpty else {
            throw invalidBundleError(bundle: bundle, reason: "EnhanceModelID must not be empty")
        }

        guard let fileName = bundle.object(forInfoDictionaryKey: MetadataKey.fileName) as? String,
              !fileName.isEmpty else {
            throw invalidBundleError(bundle: bundle, reason: "Missing \(MetadataKey.fileName)")
        }

        guard let fileExtension = bundle.object(forInfoDictionaryKey: MetadataKey.fileExtension) as? String,
              !fileExtension.isEmpty else {
            throw invalidBundleError(bundle: bundle, reason: "Missing \(MetadataKey.fileExtension)")
        }

        guard let modelURL = bundle.url(forResource: fileName, withExtension: fileExtension) else {
            throw invalidBundleError(bundle: bundle,
                                     reason: "Missing model resource \(fileName).\(fileExtension)")
        }

        return ModelEntry(id: modelID,
                          url: modelURL,
                          bundleURL: bundle.bundleURL,
                          bundleIdentifier: bundle.bundleIdentifier)
    }

    private static func candidateBundleURLs() throws -> [URL] {
        var candidateURLs = Set<URL>()
        try addCandidateURLs(from: Bundle.main, to: &candidateURLs)
        try addCandidateURLs(from: Bundle(for: EnhanceBundleLocator.self), to: &candidateURLs)

        for bundle in Bundle.allBundles {
            try addCandidateURLs(from: bundle, to: &candidateURLs)
        }

        for bundle in Bundle.allFrameworks {
            try addCandidateURLs(from: bundle, to: &candidateURLs)
        }

        return Array(candidateURLs)
    }

    private static func addCandidateURLs(from bundle: Bundle, to candidateURLs: inout Set<URL>) throws {
        try addCandidateURLs(under: bundle.bundleURL, to: &candidateURLs)

        guard let resourceURL = bundle.resourceURL,
              resourceURL != bundle.bundleURL else { return }
        try addCandidateURLs(under: resourceURL, to: &candidateURLs)
    }

    private static func addCandidateURLs(under rootURL: URL, to candidateURLs: inout Set<URL>) throws {
        let rootValues = try rootURL.resourceValues(forKeys: [.isDirectoryKey])
        guard rootValues.isDirectory == true else { return }

        if rootURL.pathExtension == "bundle" {
            candidateURLs.insert(rootURL)
        }

        let enumerator = FileManager.default.enumerator(at: rootURL,
                                                        includingPropertiesForKeys: [.isDirectoryKey],
                                                        options: [.skipsHiddenFiles])
        while let candidateURL = enumerator?.nextObject() as? URL {
            guard candidateURL.pathExtension == "bundle" else { continue }
            candidateURLs.insert(candidateURL)
            enumerator?.skipDescendants()
        }
    }

    private static func invalidBundleError(bundle: Bundle, reason: String) -> EnhanceError {
        .invalidArgument("Invalid GigaEdgeEnhance model bundle \(bundle.bundleURL.lastPathComponent): \(reason)")
    }

    private static let cacheLock = UnfairLock()
    private static var cachedEntries: [ModelEntry]?
}

private enum MetadataKey {
    static let id = "EnhanceModelID"
    static let fileName = "EnhanceModelFileName"
    static let fileExtension = "EnhanceModelFileExtension"
}

private struct ModelEntry {
    let id: EnhanceModelDescriptor
    let url: URL
    let bundleURL: URL
    let bundleIdentifier: String?
}

private final class EnhanceBundleLocator: NSObject {}
