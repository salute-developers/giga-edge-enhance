#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/xcframework"
BUNDLE_BUILD_DIR="$BUILD_DIR/bundle-products"
TEST_BUILD_DIR="$BUILD_DIR/test-derived-data"
PROJECT="$ROOT_DIR/GigaEdgeSpenchSDK.xcodeproj"
SCHEME="GigaEdgeSpenchAudioKit"

"$ROOT_DIR/scripts/build_core.sh"
"$ROOT_DIR/scripts/generate_project.sh"

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

xcodebuild archive \
  -project "$PROJECT" \
  -scheme "$SCHEME" \
  -destination "generic/platform=iOS" \
  -archivePath "$BUILD_DIR/ios" \
  -configuration Release \
  SKIP_INSTALL=NO \
  CODE_SIGNING_ALLOWED=NO \
  BUILD_LIBRARY_FOR_DISTRIBUTION=YES

xcodebuild archive \
  -project "$PROJECT" \
  -scheme "$SCHEME" \
  -destination "generic/platform=iOS Simulator" \
  -archivePath "$BUILD_DIR/simulator" \
  -configuration Release \
  SKIP_INSTALL=NO \
  CODE_SIGNING_ALLOWED=NO \
  BUILD_LIBRARY_FOR_DISTRIBUTION=YES

xcodebuild -create-xcframework \
  -framework "$BUILD_DIR/ios.xcarchive/Products/Library/Frameworks/GigaEdgeSpenchAudioKit.framework" \
  -framework "$BUILD_DIR/simulator.xcarchive/Products/Library/Frameworks/GigaEdgeSpenchAudioKit.framework" \
  -output "$BUILD_DIR/GigaEdgeSpenchAudioKit.xcframework"

# Strip internal-only artifacts from the distributed xcframework. The
# `GigaEdgeSpenchAudioKit_Private` module is needed only inside the framework's
# own implementation; consumers must not see the headers or modulemap.
while IFS= read -r -d '' fw; do
  rm -rf "$fw/PrivateHeaders"
  rm -f "$fw/Modules/module.private.modulemap"
done < <(find "$BUILD_DIR/GigaEdgeSpenchAudioKit.xcframework" -type d -name "*.framework" -print0)

xcodebuild build \
  -project "$PROJECT" \
  -scheme "$SCHEME" \
  -configuration Release \
  -destination "generic/platform=iOS Simulator" \
  -derivedDataPath "$BUNDLE_BUILD_DIR" \
  CODE_SIGNING_ALLOWED=NO

cp -R "$BUNDLE_BUILD_DIR/Build/Products/Release-iphonesimulator/GigaEdgeSpenchModelFast8.bundle" "$BUILD_DIR"
cp -R "$BUNDLE_BUILD_DIR/Build/Products/Release-iphonesimulator/GigaEdgeSpenchModelQuality48.bundle" "$BUILD_DIR"
cp -R "$BUNDLE_BUILD_DIR/Build/Products/Release-iphonesimulator/GigaEdgeSpenchModelHybrid48.bundle" "$BUILD_DIR"

for test_scheme in GigaEdgeSpenchAudioKit AudioDemoApp; do
  xcodebuild build-for-testing \
    -project "$PROJECT" \
    -scheme "$test_scheme" \
    -configuration Release \
    -destination "generic/platform=iOS Simulator" \
    -derivedDataPath "$TEST_BUILD_DIR" \
    ENABLE_TESTABILITY=YES \
    CODE_SIGNING_ALLOWED=NO
done

echo "Built XCFramework at $BUILD_DIR/GigaEdgeSpenchAudioKit.xcframework"
