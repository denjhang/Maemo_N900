# Changelog

All notable changes to QRScan are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versions follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Added
- Initial postmarketOS workspace (GTK4 + GStreamer + ZBar)
- Symbian S60 workspace (ECam + ZXing)
- Samsung Bada workspace (Bada SDK 2.0)

---

## [1.0.0] — 2024-01-01

### Added

**Android (API 21+)**
- Live QR / barcode scanning via CameraX with STRATEGY_KEEP_ONLY_LATEST image analysis
- ZXing MultiFormatReader supporting QR Code, Data Matrix, Aztec, PDF 417, Code 128, Code 39, Code 93, EAN-13, EAN-8, UPC-A, UPC-E, ITF, Codabar
- Google ML Kit barcode scanning as secondary decoder for improved accuracy on modern devices
- Flash / torch toggle with persistent camera state across configuration changes
- Front / rear camera flip
- Pinch-to-zoom gesture with animated slider overlay
- Gallery image import via system photo picker (READ_MEDIA_IMAGES on API 33+, READ_EXTERNAL_STORAGE on API 32 and below)
- Static image decode using ZXing RGBLuminanceSource with EXIF rotation correction and auto-downscale for oversized images
- QR Code creation supporting all 13 ZXing-writable barcode formats with selectable foreground/background colors via MultiFormatWriter
- Copy result to clipboard
- Share result text via Android share sheet
- Share generated QR Code image via FileProvider
- Open URL results directly in default browser
- Smart content-type detection: URL, Wi-Fi, vCard, MECARD, Geo, Email, Phone, SMS, Calendar, Bitcoin, plain text
- Scan history with Room database persistence (SQLite)
- Favorites — mark any scan as favorite with heart toggle on result screen
- Edit scan content inline with AlertDialog
- Save generated QR Code to gallery (MediaStore on API 29+, direct file write on older)
- Full Material You dynamic color support on Android 12+ (API 31+) using dynamicDarkColorScheme / dynamicLightColorScheme
- Static Material 3 color scheme for Android 5–11
- Light / Dark / System theme switching persisted via DataStore preferences
- Haptic feedback toggle
- Save history toggle
- Beep on scan toggle
- Shared image intake via ACTION_SEND intent filter

**Android Legacy (API 7–20)**
- Live QR scanning via Camera1 API with background HandlerThread decode loop
- ZXing 3.3.3 (Java 1.4 compatible) multi-format reader
- Auto-focus with FOCUS_MODE_CONTINUOUS_PICTURE fallback to FOCUS_MODE_AUTO
- Best preview size selection (max 720p to conserve memory on low-RAM devices)
- Flash torch toggle
- Gallery image import and decode via ZXing RGBLuminanceSource
- Copy result to clipboard
- Share result via ACTION_SEND
- Open URL results in default browser
- QR Code creation and gallery save
- History screen backed by SharedPreferences

**iOS / iPadOS (14+)**
- Live scanning via AVCaptureSession with AVCaptureMetadataOutput supporting all AVMetadataObject barcode types
- Flash / torch toggle via AVCaptureDevice.torchMode
- Front / rear camera flip with AVCaptureDevice position switching
- Gallery import via PhotosPicker (SwiftUI, iOS 16+) with CIDetector fallback
- QR Code creation via CIQRCodeGenerator, Aztec via CIAztecCodeGenerator, PDF 417 via CIPDF417BarcodeGenerator, Code 128 via CICode128BarcodeGenerator with foreground/background color customization
- Copy to clipboard via UIPasteboard
- Share via UIActivityViewController
- Open URL via UIApplication.open
- Edit result content in inline sheet
- Favorite / unfavorite toggle
- Scan history persisted via JSON + UserDefaults
- Favorites view
- SwiftUI TabView navigation
- Dark / light / system theme toggle via ColorScheme environment
- Haptic feedback, beep on scan, save history settings

**iPhoneOS Classic (2.0+)**
- Gallery-based decode via CIDetector (requires iOS 5+; on iOS 4 falls back to UIImagePickerController + CIDetector)
- Live camera scanning via AVCaptureSession + AVCaptureMetadataOutput (iOS 4+)
- Flash / torch toggle
- Copy to clipboard via UIPasteboard
- Open URL in Safari via UIApplication openURL:
- Full manual reference counting (MRC) — no ARC dependency

**BlackBerry OS (5–7.1)**
- Live camera scanning via JSR-234 VideoControl with JPEG snapshot decode loop
- ZXing 3.3.3 pure-Java decode over captured JPEG frames
- Gallery image pick via FileDialog + ZXing RGBLuminanceSource decode
- Copy to clipboard via RIM Clipboard API
- Open URLs in BlackBerry Browser via BrowserSession
- Persistent scan history via PersistentStore

**postmarketOS / Linux mobile**
- Live camera scanning via GStreamer v4l2src → tee → (GTK4 paintable sink | appsink) dual pipeline
- ZBar barcode decode of RGB frames from appsink in dedicated background thread
- Flash / torch control via v4l2src extra-controls
- Pinch-to-zoom (GtkScale widget mapped to v4l2src zoom property)
- Gallery image pick via GtkFileDialog with MIME type filtering
- Static image decode via GdkPixbuf + ZBar
- Copy to clipboard via GdkClipboard
- Open URLs via gtk_show_uri
- SQLite-backed scan history via history_store
- meson + ninja build system
- .desktop file for application menu integration
- Adaptive icon (SVG)

**CI / CD**
- CodeQL security analysis for Java/Kotlin (Android), Swift (iOS), Objective-C/C++ (iPhoneOS), Java (BlackBerry)
- Debug build CI for all 6 platform workspaces
- Release build CI producing signed APK, AAB, and IPA on git tags
- ktlint, Android Lint, SwiftLint, clang-tidy, cppcheck, yamllint lint pipeline
- Dependabot for Gradle (Android, Android Legacy), Swift PM, and GitHub Actions
- Google Labs Jules AI code review on pull requests
- Bug report and feature request issue templates

[Unreleased]: https://github.com/Hexadecinull/QRScan/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/Hexadecinull/QRScan/releases/tag/v1.0.0
