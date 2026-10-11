# QRScan

**Free. Open source. Ad-less. Universal. Lightweight. Powerful.**

QRScan is a fully featured QR code scanner and creator that runs on virtually every mobile platform ever made — from Android 2.1 to Android 16, iPhoneOS 2.0 to iOS 26.1, Samsung Bada, Symbian S60, BlackBerry OS, and postmarketOS.

[![Build Debug](https://github.com/Hexadecinull/QRScan/actions/workflows/build-debug.yml/badge.svg)](https://github.com/Hexadecinull/QRScan/actions/workflows/build-debug.yml)
[![Build Release](https://github.com/Hexadecinull/QRScan/actions/workflows/build-release.yml/badge.svg)](https://github.com/Hexadecinull/QRScan/actions/workflows/build-release.yml)
[![CodeQL](https://github.com/Hexadecinull/QRScan/actions/workflows/codeql.yml/badge.svg)](https://github.com/Hexadecinull/QRScan/actions/workflows/codeql.yml)
[![Lint](https://github.com/Hexadecinull/QRScan/actions/workflows/lint.yml/badge.svg)](https://github.com/Hexadecinull/QRScan/actions/workflows/lint.yml)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)

---

## Features

| Feature | Android 21+ | Android Legacy | iOS 14+ | iPhoneOS | BlackBerry | Bada | Symbian | postmarketOS |
|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| Live camera scan | ✓ | ✓ | ✓ | ✓ (iOS 4+) | ✓ | ✓ | ✓ | ✓ |
| Pick image from gallery | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | — | ✓ |
| Flash / torch toggle | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Camera flip (front/rear) | ✓ | — | ✓ | — | — | — | — | — |
| Pinch to zoom | ✓ | — | ✓ | — | — | — | — | ✓ |
| QR Code creation | ✓ | ✓ | ✓ | — | — | — | — | — |
| 13 barcode formats | ✓ | ✓ | ✓ | — | — | — | — | ✓ |
| Copy to clipboard | ✓ | ✓ | ✓ | ✓ | ✓ | — | — | ✓ |
| Share result | ✓ | ✓ | ✓ | — | — | — | — | — |
| Open URL directly | ✓ | ✓ | ✓ | ✓ | ✓ | — | — | ✓ |
| Scan history | ✓ | ✓ | ✓ | — | ✓ | — | — | ✓ |
| Favorites | ✓ | — | ✓ | — | — | — | — | — |
| Edit result | ✓ | — | ✓ | — | — | — | — | — |
| Export result | ✓ | — | ✓ | — | — | — | — | — |
| Material You (Android 12+) | ✓ | — | — | — | — | — | — | — |
| Dark mode | ✓ | — | ✓ | — | — | — | — | ✓ |
| No ads | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Completely free | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |

### Supported barcode formats

QR Code, Data Matrix, Aztec, PDF 417, Code 128, Code 39, Code 93, EAN-13, EAN-8, UPC-A, UPC-E, ITF, Codabar

### Supported content types (smart detection)

URL, Wi-Fi credentials, vCard, MECARD, Geographic location, Email, Phone, SMS, Calendar event, Bitcoin address, Plain text

---

## Repository structure

```
QRScan/
├── android/              # Modern Android (API 21+) — Kotlin + Jetpack Compose + CameraX
├── android-legacy/       # Legacy Android (API 7–20) — Java + Camera1 API + ZXing
├── ios/                  # iOS / iPadOS (14+) — Swift + SwiftUI + AVFoundation
├── iphoneos/             # Classic iPhoneOS (2.0+) — Objective-C, MRC, UIKit
├── bada/                 # Samsung Bada (1.x / 2.x) — C++ Bada SDK
├── symbian/              # Symbian S60 3rd Ed — C++ ECam + ZXing
├── blackberry/           # BlackBerry OS (5–7.1) — Java + RIM APIs + ZXing
├── postmarketos/         # postmarketOS / Linux mobile — C + GTK4 + GStreamer + ZBar
├── docs/                 # Documentation
└── .github/              # CI workflows, Dependabot, issue templates
```

---

## Building

See [docs/BUILDING.md](docs/BUILDING.md) for full platform-by-platform build instructions.

Quick start for the most common targets:

```bash
# Android (API 21+)
cd android && ./gradlew assembleDebug

# Android Legacy (API 7–20)
cd android-legacy && ./gradlew assembleDebug

# iOS / iPadOS
cd ios && xcodebuild -scheme QRScan -destination 'platform=iOS Simulator,name=iPhone 15' build

# postmarketOS
cd postmarketos && meson setup builddir && ninja -C builddir
```

---

## Contributing

Contributions are very welcome. Please read [CONTRIBUTING.md](docs/CONTRIBUTING.md) before opening a pull request.

**Branch model:**
- `main` — stable, protected. All code here must pass CI.
- `develop` — integration branch for features.
- `feature/<name>` — individual feature work.
- `fix/<name>` — bug fix branches.

Pull requests always target `develop`, never `main` directly unless it is a hotfix.

---

## License

QRScan is licensed under the [GNU General Public License v3.0](LICENSE).

Copyright © 2024 Hexadecinull

---

## Acknowledgements

- [ZXing](https://github.com/zxing/zxing) — Java/Kotlin multi-format 1D/2D barcode decoder
- [ZBar](https://github.com/mchehab/zbar) — C barcode decoding library used on postmarketOS
- [Google ML Kit](https://developers.google.com/ml-kit/vision/barcode-scanning) — on-device barcode scanning for modern Android
- [CameraX](https://developer.android.com/training/camerax) — Jetpack camera library
- [Jetpack Compose](https://developer.android.com/jetpack/compose) — Android UI toolkit
- [AVFoundation](https://developer.apple.com/av-foundation/) — Apple's camera and media framework
- [GTK4](https://www.gtk.org/) — UI toolkit for postmarketOS / Linux mobile
- [GStreamer](https://gstreamer.freedesktop.org/) — multimedia pipeline for postmarketOS camera
