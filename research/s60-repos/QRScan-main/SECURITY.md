# Security Policy

## Supported versions

| Platform | Version | Supported |
|---|---|---|
| Android (API 21+) | 1.x | ✓ |
| Android Legacy (API 7–20) | 1.x | ✓ |
| iOS / iPadOS | 1.x | ✓ |
| iPhoneOS Classic | 1.x | ✓ |
| BlackBerry OS | 1.x | ✓ |
| postmarketOS | 1.x | ✓ |

## Reporting a vulnerability

Do not open a public GitHub issue for security vulnerabilities.

Report security issues privately by emailing the maintainer via the contact listed on the [Hexadecinull GitHub profile](https://github.com/Hexadecinull). Include as much detail as possible:

- Affected platform and version
- Steps to reproduce
- Potential impact
- Any proof-of-concept if available

You will receive a response within 72 hours. If the vulnerability is confirmed, a fix will be prepared and released as a patch version. Credit will be given in the release notes unless you request otherwise.

## Security considerations

- QRScan does not contact any remote server during normal operation. All scanning and decoding is performed entirely on-device.
- Scan history is stored locally in a SQLite database (Android, postmarketOS) or UserDefaults JSON (iOS) and never transmitted.
- The app requests only the minimum permissions needed: CAMERA, READ_MEDIA_IMAGES (or READ_EXTERNAL_STORAGE on legacy Android), FLASHLIGHT, and VIBRATE.
- QR code content is never automatically executed. URLs require an explicit tap to open. Wi-Fi credentials require explicit confirmation. vCards require an explicit tap to add to contacts.
- The ZXing and ZBar decode libraries are well-established open source projects. Their dependency versions are kept up to date via Dependabot.
