import Foundation

/// Stable identifier for a bundled Spench denoiser model.
///
/// Use one of the bundled descriptors (``lite8k``, ``quality48k``)
/// or build your own with ``init(rawValue:)``. The raw value must match the
/// `SpenchModelID` key inside the model bundle's `Info.plist`.
public struct SpenchModelDescriptor: RawRepresentable, Hashable, Comparable, Sendable {
    public let rawValue: String

    /// Creates a descriptor for an arbitrary `SpenchModelID`.
    public init(rawValue: String) {
        self.rawValue = rawValue
    }

    public static func < (lhs: SpenchModelDescriptor, rhs: SpenchModelDescriptor) -> Bool {
        lhs.rawValue < rhs.rawValue
    }

    /// Lightweight 8 kHz model. Lowest CPU cost; recommended for narrow-band
    /// telephony scenarios on older devices.
    public static let lite8k = SpenchModelDescriptor(rawValue: "lite8k")
    /// Full-quality 48 kHz model. Highest fidelity at the cost of CPU; best
    /// for offline rendering or when hardware can sustain it.
    public static let quality48k = SpenchModelDescriptor(rawValue: "quality48k")
}

/// Discovers Spench model bundles packaged with an application or framework.
///
/// The catalog scans the main bundle, the framework bundle, and every
/// loaded bundle/framework for `.bundle` resources whose `Info.plist`
/// declares a `SpenchModelID`. Discovery results are cached for the lifetime
/// of the process; call ``invalidateCache()`` when bundles are dynamically
/// added or removed at runtime.
public enum SpenchModelCatalog {
    /// Returns all installed Spench model identifiers sorted by raw value.
    ///
    /// - Throws: ``SpenchError/invalidArgument(_:)`` if a bundle declares a
    ///   `SpenchModelID` but is missing required metadata, or if two
    ///   bundles declare the same `SpenchModelID`.
    public static func availableModels() throws -> [SpenchModelDescriptor] {
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

    static func modelURL(forID id: SpenchModelDescriptor) throws -> URL {
        let entries = try modelEntries()

        guard let entry = entries.first(where: { $0.id == id }) else {
            let availableIDs = entries.map(\.id.rawValue).sorted().joined(separator: ", ")
            let description = availableIDs.isEmpty
                ? "No Spench model bundles are installed"
                : "Unknown Spench model id \(id.rawValue). Available models: \(availableIDs)"
            throw SpenchError.invalidArgument(description)
        }

        return entry.url
    }

    private static func modelEntries() throws -> [ModelEntry] {
        if let cached = cacheLock.withLock({ cachedEntries }) {
            return cached
        }

        var entriesByID: [SpenchModelDescriptor: ModelEntry] = [:]

        for bundleURL in try candidateBundleURLs() {
            guard let bundle = Bundle(url: bundleURL) else { continue }
            guard let entry = try modelEntry(in: bundle) else { continue }

            if let existingEntry = entriesByID[entry.id] {
                let description = "Duplicate Spench model id \(entry.id.rawValue) in bundles " +
                    "\(existingEntry.bundleURL.lastPathComponent) and \(entry.bundleURL.lastPathComponent)"
                throw SpenchError.invalidArgument(description)
            }

            entriesByID[entry.id] = entry
        }

        let entries = Array(entriesByID.values)
        cacheLock.withLock {
            cachedEntries = entries
        }
        return entries
    }

    private static func modelEntry(in bundle: Bundle) throws -> ModelEntry? {
        guard let modelIDValue = bundle.object(forInfoDictionaryKey: MetadataKey.id) as? String else {
            return nil
        }
        let modelID = SpenchModelDescriptor(rawValue: modelIDValue)
        guard !modelID.rawValue.isEmpty else {
            throw invalidBundleError(bundle: bundle, reason: "SpenchModelID must not be empty")
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

        return ModelEntry(id: modelID, url: modelURL, bundleURL: bundle.bundleURL)
    }

    private static func candidateBundleURLs() throws -> [URL] {
        var candidateURLs = Set<URL>()
        try addCandidateURLs(from: Bundle.main, to: &candidateURLs)
        try addCandidateURLs(from: Bundle(for: SpenchBundleLocator.self), to: &candidateURLs)

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

    private static func invalidBundleError(bundle: Bundle, reason: String) -> SpenchError {
        .invalidArgument("Invalid Spench model bundle \(bundle.bundleURL.lastPathComponent): \(reason)")
    }

    private static let cacheLock = UnfairLock()
    private static var cachedEntries: [ModelEntry]?
}

private enum MetadataKey {
    static let id = "SpenchModelID"
    static let fileName = "SpenchModelFileName"
    static let fileExtension = "SpenchModelFileExtension"
}

private struct ModelEntry {
    let id: SpenchModelDescriptor
    let url: URL
    let bundleURL: URL
}

private final class SpenchBundleLocator: NSObject {}
