import XCTest
@testable import EnhanceAudioKit

final class ModelCatalogTests: XCTestCase {
    private var temporaryDirectory: URL!

    override func setUpWithError() throws {
        temporaryDirectory = FileManager.default.temporaryDirectory
            .appendingPathComponent(UUID().uuidString, isDirectory: true)
        try FileManager.default.createDirectory(at: temporaryDirectory,
                                                withIntermediateDirectories: true)
    }

    override func tearDownWithError() throws {
        try FileManager.default.removeItem(at: temporaryDirectory)
    }

    func testRepeatedCopiesOfSameBundleAreDiscoveredOnce() throws {
        let firstBundle = try makeBundle(name: "First", bundleIdentifier: "com.gigaedge.TestModel")
        let secondBundle = try makeBundle(name: "Second", bundleIdentifier: "com.gigaedge.TestModel")

        let models = try EnhanceModelCatalog.availableModels(in: [secondBundle, firstBundle])

        XCTAssertEqual(models, [EnhanceModelDescriptor(rawValue: "test-model")])
    }

    func testDifferentBundlesWithSameModelIDAreRejected() throws {
        let firstBundle = try makeBundle(name: "First", bundleIdentifier: "com.gigaedge.FirstModel")
        let secondBundle = try makeBundle(name: "Second", bundleIdentifier: "com.gigaedge.SecondModel")

        XCTAssertThrowsError(try EnhanceModelCatalog.availableModels(in: [firstBundle, secondBundle])) { error in
            guard case let EnhanceError.invalidArgument(message) = error else {
                return XCTFail("Expected invalidArgument, received \(error)")
            }
            XCTAssertTrue(message.contains("Duplicate GigaEdgeEnhance model id test-model"))
        }
    }

    private func makeBundle(name: String, bundleIdentifier: String) throws -> URL {
        let bundleURL = temporaryDirectory.appendingPathComponent("\(name).bundle", isDirectory: true)
        try FileManager.default.createDirectory(at: bundleURL, withIntermediateDirectories: true)
        try Data("model".utf8).write(to: bundleURL.appendingPathComponent("model.enhance"))

        let metadata: [String: Any] = [
            "CFBundleIdentifier": bundleIdentifier,
            "CFBundlePackageType": "BNDL",
            "EnhanceModelID": "test-model",
            "EnhanceModelFileName": "model",
            "EnhanceModelFileExtension": "ges"
        ]
        let infoPlist = try PropertyListSerialization.data(fromPropertyList: metadata,
                                                           format: .xml,
                                                           options: 0)
        try infoPlist.write(to: bundleURL.appendingPathComponent("Info.plist"))
        return bundleURL
    }
}
