# Building QRScan

This document covers build prerequisites and step-by-step instructions for every supported platform.

---

## Android (API 21+, `android/`)

### Prerequisites

| Tool | Minimum version |
|---|---|
| JDK | 17 (Temurin recommended) |
| Android Studio | Hedgehog (2023.1.1) or newer |
| Android SDK | API 34 |
| Android Build Tools | 34.0.0 |
| Gradle | 8.5 (wrapper included) |

### Debug build

```bash
cd android
./gradlew assembleDebug
# Output: app/build/outputs/apk/debug/app-debug.apk
```

### Release build

Create `android/keystore.properties`:

```properties
storeFile=release.keystore
storePassword=YOUR_STORE_PASSWORD
keyAlias=YOUR_KEY_ALIAS
keyPassword=YOUR_KEY_PASSWORD
```

Place your keystore at `android/app/release.keystore`, then:

```bash
cd android
./gradlew assembleRelease
# Output: app/build/outputs/apk/release/app-release.apk

./gradlew bundleRelease
# Output: app/build/outputs/bundle/release/app-release.aab
```

### Lint and code style checks

```bash
cd android
./gradlew lint
./gradlew ktlintCheck
```

---

## Android Legacy (API 7–20, `android-legacy/`)

This workspace targets devices running Android 2.1 (Eclair) through Android 4.4 (KitKat) and some early API 20 devices. It uses Camera1 API and the pure-Java ZXing 3.3.3 core which supports Java 1.4 source compatibility.

### Prerequisites

| Tool | Minimum version |
|---|---|
| JDK | 11 |
| Android SDK | API 17 (target) |
| Android Build Tools | 30.0.3 |

### Debug build

```bash
cd android-legacy
./gradlew assembleDebug
# Output: app/build/outputs/apk/debug/app-debug.apk
```

### Notes on compatibility

- `minSdkVersion` is 7 (Android 2.1 Eclair). The Java sources are compiled with `-source 1.7 -target 1.7`.
- No Kotlin, no Compose, no ViewBinding — pure XML layouts with classic View inflation.
- Camera flash on very early devices (API 7–8) may not support `FLASH_MODE_TORCH`; the code guards this with a null-check on supported flash modes.
- ZXing 3.3.3 is the last version compatible with Android 2.x's limited Dalvik VM runtime.

---

## iOS / iPadOS (iOS 14+, `ios/`)

### Prerequisites

| Tool | Minimum version |
|---|---|
| Xcode | 15.0 |
| macOS | Ventura (13.0) |
| Swift | 5.9 |
| iOS Deployment Target | 14.0 |

### Build from command line

```bash
cd ios

# Resolve packages
xcodebuild -resolvePackageDependencies -scheme QRScan

# Build for simulator
xcodebuild \
  -scheme QRScan \
  -destination 'platform=iOS Simulator,name=iPhone 15' \
  -configuration Debug \
  CODE_SIGNING_ALLOWED=NO \
  build

# Build for device (requires provisioning profile)
xcodebuild \
  -scheme QRScan \
  -destination 'generic/platform=iOS' \
  -configuration Release \
  archive \
  -archivePath build/QRScan.xcarchive
```

### SwiftLint

```bash
cd ios
brew install swiftlint
swiftlint lint
```

### Camera and photo library permissions

The following `Info.plist` keys must be present before submitting to App Store Connect:

```xml
<key>NSCameraUsageDescription</key>
<string>Camera access is required to scan QR codes.</string>
<key>NSPhotoLibraryUsageDescription</key>
<string>Photo library access is required to scan QR codes from images.</string>
```

---

## iPhoneOS Classic (iPhoneOS 2.0–3.x, `iphoneos/`)

This workspace targets the original iPhoneOS SDK from 2008–2009. It uses manual reference counting (MRC, no ARC), `UIAlertView`, `presentModalViewController:`, and the classic UIKit layout engine. Live camera scanning is available from iOS 4 onward via `AVCaptureSession`; on iPhoneOS 2–3 only gallery-based decode via `CIDetector` is available.

### Prerequisites

| Tool | Note |
|---|---|
| Xcode (any modern version) | The project uses compatibility stubs that allow modern Xcode to build the ObjC codebase. Set deployment target to iOS 4.0 minimum to allow simulator builds. |
| macOS Ventura or newer | — |

### Build

```bash
cd iphoneos
xcodebuild \
  -scheme QRScanClassic \
  -destination 'platform=iOS Simulator,name=iPhone 15' \
  -configuration Debug \
  CODE_SIGNING_ALLOWED=NO \
  build
```

### Notes on MRC

All classes in `iphoneos/` use manual retain/release. Do not enable `-fobjc-arc` for any of these files. The `dealloc` methods explicitly call `[super dealloc]`. Do not mix ARC and MRC files without a bridging mechanism.

---

## BlackBerry OS (`blackberry/`)

### Prerequisites

| Tool | Version |
|---|---|
| BlackBerry JDE (Java Development Environment) | 7.1 or 5.0 for older targets |
| JDK | 8 (the BB JDE compiler targets Java 1.4 bytecode) |
| BlackBerry Smartphone Simulator | Optional for testing |

The CI pipeline performs a stub compile using OpenJDK 8 with `-source 1.4 -target 1.4` to verify source compatibility. Full packaging into a `.cod` file requires the proprietary BlackBerry JDE.

### Compile sources (stub, no RIM runtime required)

```bash
cd blackberry
mkdir -p build/classes
find src -name "*.java" -print0 | xargs -0 javac \
  -source 1.4 -target 1.4 \
  -d build/classes
```

### Full build with BlackBerry JDE

1. Open BlackBerry JDE.
2. Import the `blackberry/` workspace.
3. Add `zxing-core-3.3.3.jar` to the project's library path.
4. Build → Create ALX/COD.

---

## Samsung Bada (`bada/`)

### Prerequisites

| Tool | Version |
|---|---|
| Bada IDE | 2.0.4 (Eclipse-based, Windows only) |
| Bada SDK | 2.0 |

Bada development requires the proprietary Samsung IDE running on Windows. The C++ sources in `bada/src/` use the `Osp::` namespace which is the Bada OS API layer.

### Build

1. Open the Bada IDE.
2. Import `bada/` as an existing Bada project.
3. Select target device or simulator.
4. Build → Build Project.

---

## Symbian S60 (`symbian/`)

### Prerequisites

| Tool | Version |
|---|---|
| Carbide.c++ IDE | 3.x |
| Symbian SDK | S60 3rd Edition, FP2 |
| OpenC/C++ plug-in | Required for standard library headers |

### Build

1. Install Carbide.c++ on a Windows machine.
2. Install the S60 3rd Edition FP2 SDK.
3. Open Carbide.c++.
4. Import the `symbian/` project.
5. Build for WINSCW emulator or ARMV5 device target.

---

## postmarketOS / Linux Mobile (`postmarketos/`)

### Prerequisites

```bash
# Alpine Linux / postmarketOS (apk)
sudo apk add \
  build-base meson ninja pkgconf \
  gtk4-dev gstreamer-dev gst-plugins-base-dev \
  gst-plugins-good-dev gst-plugin-gtk \
  zbar-dev sqlite-dev gdk-pixbuf-dev

# Debian / Ubuntu (apt)
sudo apt-get install -y \
  build-essential meson ninja-build pkg-config \
  libgtk-4-dev libgstreamer1.0-dev \
  libgstreamer-plugins-base1.0-dev \
  libzbar-dev libsqlite3-dev libgdk-pixbuf-2.0-dev
```

### Build

```bash
cd postmarketos
meson setup builddir --buildtype=release
ninja -C builddir
sudo ninja -C builddir install
```

### Run

```bash
./builddir/qrscan
```

### cppcheck (static analysis)

```bash
cppcheck --enable=all --suppress=missingIncludeSystem postmarketos/src/
```

---

## CI / CD

All CI workflows run automatically on push and pull request. See `.github/workflows/` for full configurations:

| Workflow | Trigger | Description |
|---|---|---|
| `build-debug.yml` | push / PR | Debug builds for all platforms |
| `build-release.yml` | version tag `v*.*.*` | Signed release builds + GitHub Release |
| `codeql.yml` | push / PR / weekly | CodeQL security analysis |
| `lint.yml` | push / PR | ktlint, Android Lint, SwiftLint, clang-tidy, cppcheck |
| `jules.yml` | PR | Google Labs Jules AI code review |

### Required GitHub Secrets for release builds

| Secret | Description |
|---|---|
| `ANDROID_KEYSTORE_BASE64` | Base64-encoded Android release keystore |
| `ANDROID_KEYSTORE_PASSWORD` | Keystore password |
| `ANDROID_KEY_ALIAS` | Key alias |
| `ANDROID_KEY_PASSWORD` | Key password |
| `IOS_P12_BASE64` | Base64-encoded iOS distribution certificate (.p12) |
| `IOS_P12_PASSWORD` | P12 certificate password |
| `APPSTORE_ISSUER_ID` | App Store Connect API issuer ID |
| `APPSTORE_API_KEY_ID` | App Store Connect API key ID |
| `APPSTORE_API_PRIVATE_KEY` | App Store Connect API private key |
| `JULES_API_KEY` | Google Labs Jules API key (optional) |
