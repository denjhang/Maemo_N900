// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "QRScan",
    platforms: [
        .iOS(.v14),
        .macCatalyst(.v14)
    ],
    products: [
        .library(name: "QRScan", targets: ["QRScan"])
    ],
    dependencies: [],
    targets: [
        .target(
            name: "QRScan",
            dependencies: [],
            path: "Sources/QRScan",
            swiftSettings: [
                .enableExperimentalFeature("StrictConcurrency")
            ]
        ),
        .testTarget(
            name: "QRScanTests",
            dependencies: ["QRScan"],
            path: "Tests/QRScanTests"
        )
    ]
)
