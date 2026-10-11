# Contributing to QRScan

Thank you for your interest in contributing to QRScan. This document explains how to get started, what the coding standards are, and how the review process works.

---

## Table of contents

1. [Code of conduct](#code-of-conduct)
2. [How to contribute](#how-to-contribute)
3. [Branch model](#branch-model)
4. [Code standards](#code-standards)
5. [Commit messages](#commit-messages)
6. [Pull request process](#pull-request-process)
7. [Issue reporting](#issue-reporting)
8. [Platform-specific notes](#platform-specific-notes)

---

## Code of conduct

This project follows a simple rule: be respectful. Harassment, discrimination, and hostile behavior of any kind will result in a ban. Constructive criticism of code is always welcome; personal attacks are not.

---

## How to contribute

1. Fork the repository on GitHub.
2. Clone your fork locally.
3. Create a branch off `develop` using the naming conventions below.
4. Make your changes, following the code standards in this document.
5. Push your branch and open a pull request targeting `develop`.
6. Wait for CI to pass and for a maintainer review.

---

## Branch model

| Branch pattern | Purpose |
|---|---|
| `main` | Stable, released code. Protected. Only hotfixes and release merges land here. |
| `develop` | Integration branch. All feature and fix PRs target this branch. |
| `feature/<short-name>` | New feature work. Branch off `develop`. |
| `fix/<issue-number>-<short-name>` | Bug fixes. Branch off `develop` (or `main` for hotfixes). |
| `docs/<short-name>` | Documentation-only changes. |
| `ci/<short-name>` | CI/CD workflow changes. |
| `refactor/<short-name>` | Refactoring with no functional change. |

Examples:

```
feature/zoom-slider
fix/42-crash-on-galaxy-s3
docs/update-building-instructions
ci/add-sonarqube
```

---

## Code standards

### Universal rules (all platforms)

- **No comments in code.** The code must be self-documenting through precise naming. Variable names, function names, and type names must fully communicate intent.
- **No placeholder code.** Do not commit `TODO`, `FIXME`, `// stub`, or `throw NotImplementedError`. Every method must be fully implemented or the PR must not be merged.
- **No dead code.** Remove unused variables, functions, and imports before opening a PR.
- **Explicit is better than implicit.** Prefer verbose, unambiguous names over short abbreviations.
- Production-quality output only. Code should be in a state that could ship immediately.

### Android (Kotlin, `android/`)

- Kotlin only. No new Java files in the modern Android workspace.
- Jetpack Compose for all UI. No XML layouts except where strictly required (e.g. adaptive icons).
- Follow the [Kotlin coding conventions](https://kotlinlang.org/docs/coding-conventions.html).
- `ktlint` must pass with zero violations (`./gradlew ktlintCheck`).
- Android Lint must pass with zero errors (`./gradlew lint`).
- All `suspend` functions must be called from a `CoroutineScope` or another `suspend` function. Do not use `GlobalScope`.
- Use `StateFlow` and `collectAsState` for reactive state in ViewModels.
- All database operations use Room DAO. No raw SQL in UI layers.

### Android Legacy (Java, `android-legacy/`)

- Java only, source and target compatibility set to Java 7.
- No lambdas (API < 26 without desugaring enabled).
- No try-with-resources (API < 19 without desugaring).
- All camera operations must run off the main thread. Use a `HandlerThread` or `Thread`.
- Respect `minSdkVersion 7`. Do not call any API introduced after API 7 without a version guard.

### iOS / iPadOS (Swift, `ios/`)

- Swift only for the `ios/` workspace.
- SwiftUI for all views. No UIKit unless wrapping a third-party component via `UIViewRepresentable`.
- SwiftLint must pass with zero errors (`swiftlint lint --strict`).
- Use `async/await` for asynchronous code. No callback-based concurrency unless wrapping a legacy API.
- Use `@MainActor` for all UI-bound types.
- Avoid force-unwraps (`!`). Use `guard let`, `if let`, or `??`.

### iPhoneOS Classic (Objective-C, `iphoneos/`)

- Objective-C only.
- Manual reference counting (MRC). ARC is disabled for all files in this workspace.
- All `dealloc` methods must call `[super dealloc]` as the final statement.
- Do not use blocks (closures). The deployment target is iPhoneOS 2.0; blocks require iOS 4.0.
- Use `dispatch_async` only where iOS 4+ is the actual deployment target. On iOS 2–3 use `NSThread`.

### BlackBerry OS (Java, `blackberry/`)

- Java 1.4 source compatibility strictly enforced.
- No generics (Java 1.4 does not support generics).
- Use raw `Vector`, `Hashtable`, and `Enumeration` instead of `ArrayList`, `HashMap`, and `Iterator`.
- All UI operations must run on the event thread via `UiApplication.getUiApplication().invokeLater()`.

### Samsung Bada (C++, `bada/`)

- C++03 compatible. The Bada SDK does not support C++11.
- Use `Osp::Base::String` for all string operations; avoid raw `char*` except for interop.
- Handle all `result` return codes. Do not silently ignore errors.
- Memory: use `new`/`delete` and explicitly handle ownership in destructors.

### Symbian S60 (C++, `symbian/`)

- Use Symbian C++ idioms: two-phase construction (`NewL`/`ConstructL`), Cleanup Stack (`CleanupStack::PushL`/`Pop`), `TRAP`/`TRAPD` for leave handling.
- Never call a leaving function from a destructor.
- Follow Symbian naming conventions: `C` prefix for heap-allocated classes, `T` for POD, `M` for mixins, `R` for resource classes.

### postmarketOS (C, `postmarketos/`)

- C99 standard strictly enforced.
- Use GLib types (`gchar`, `gint`, `gboolean`, etc.) consistently throughout.
- Every `GObject` subclass must implement `finalize` or `dispose` if it holds resources.
- Use `g_signal_connect` for all event wiring. Never use direct function pointers where a signal exists.
- All allocations through GLib (`g_malloc`, `g_new`, `g_strdup`, etc.). Free with the corresponding GLib free function.
- `cppcheck` and `clang-tidy` must report zero errors.

---

## Commit messages

Follow the [Conventional Commits](https://www.conventionalcommits.org/) specification:

```
<type>(<scope>): <short description>

[optional body]

[optional footer(s)]
```

**Types:**

| Type | When to use |
|---|---|
| `feat` | New feature |
| `fix` | Bug fix |
| `refactor` | Code change with no functional difference |
| `docs` | Documentation only |
| `ci` | CI/CD workflow changes |
| `test` | Adding or fixing tests |
| `chore` | Dependency bumps, formatting, tooling |
| `perf` | Performance improvement |
| `revert` | Reverts a previous commit |

**Scopes:** `android`, `android-legacy`, `ios`, `iphoneos`, `blackberry`, `bada`, `symbian`, `postmarketos`, `ci`, `docs`, `root`

**Examples:**

```
feat(android): add pinch-to-zoom gesture on scanner
fix(ios): fix crash when picking image with HEIC format
ci(deps): bump actions/checkout from 3 to 4
docs: add Symbian build prerequisites table
chore(android): update ZXing to 3.5.3
```

---

## Pull request process

1. Ensure all CI checks pass before requesting review.
2. Fill in the pull request template completely.
3. Link the PR to any related issues using `Closes #N` or `Fixes #N` in the description.
4. Keep PRs focused. One feature or fix per PR. Large refactors should be discussed in an issue first.
5. Squash fixup commits before merge (the maintainer may do this on merge with squash-merge).
6. Do not force-push to a PR branch after a review has started unless resolving a specific review comment.

---

## Issue reporting

Use the GitHub issue templates:

- **Bug report** — for crashes, incorrect behavior, or broken builds.
- **Feature request** — for new capabilities or platform support.

When filing a bug, always specify:
- Platform and OS version
- Device model
- QRScan version
- Steps to reproduce
- Expected vs. actual behavior
- Logs or crash output if available

---

## Platform-specific notes

### Adding a new platform

If you want to add support for a platform not currently in the repository (e.g. Windows Mobile, Palm OS, MeeGo):

1. Create a new top-level directory named after the platform (lowercase, hyphen-separated).
2. Add the workspace to the relevant CI workflows in `.github/workflows/`.
3. Update `.gitignore` and `.gitattributes` for any new build artifacts.
4. Add a section to `docs/BUILDING.md` for the new platform.
5. Update the feature comparison table in `README.md`.

### Minimum platform requirements

New code must not raise the minimum supported OS version for any existing platform without a discussion and a clear justification in the issue tracker.
