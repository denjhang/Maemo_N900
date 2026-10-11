import AVFoundation
import UIKit
import Combine

@MainActor
final class ScannerViewModel: NSObject, ObservableObject {
    @Published var lastResult: ScanResult?
    @Published var isTorchOn    = false
    @Published var isFrontCamera = false
    @Published var isScanning   = false

    let session = AVCaptureSession()
    private var currentInput: AVCaptureDeviceInput?
    private let output = AVCaptureMetadataOutput()
    private let sessionQueue = DispatchQueue(label: "com.hexadecinull.qrscan.session")

    func startSession() {
        sessionQueue.async { [weak self] in
            guard let self else { return }
            self.configureSession(front: false)
            self.session.startRunning()
        }
    }

    func stopSession() {
        sessionQueue.async { [weak self] in
            self?.session.stopRunning()
        }
    }

    func resumeScanning() {
        lastResult = nil
    }

    func toggleTorch() {
        guard let device = AVCaptureDevice.default(for: .video),
              device.hasTorch else { return }
        try? device.lockForConfiguration()
        isTorchOn.toggle()
        device.torchMode = isTorchOn ? .on : .off
        device.unlockForConfiguration()
    }

    func flipCamera() {
        isFrontCamera.toggle()
        sessionQueue.async { [weak self] in
            guard let self else { return }
            self.session.beginConfiguration()
            if let old = self.currentInput {
                self.session.removeInput(old)
            }
            self.configureInput(front: self.isFrontCamera)
            self.session.commitConfiguration()
        }
    }

    func decodeFromImage(_ image: UIImage) {
        guard let cgImage = image.cgImage else { return }
        let ciImage = CIImage(cgImage: cgImage)
        let context = CIContext()
        let detector = CIDetector(
            ofType: CIDetectorTypeQRCode,
            context: context,
            options: [CIDetectorAccuracy: CIDetectorAccuracyHigh]
        )
        let features = detector?.features(in: ciImage) ?? []
        if let qr = features.first as? CIQRCodeFeature, let msg = qr.messageString {
            lastResult = ScanResult(text: msg, formatName: "QR_CODE")
        }
    }

    private func configureSession(front: Bool) {
        session.beginConfiguration()
        session.sessionPreset = .high
        configureInput(front: front)
        if session.canAddOutput(output) {
            session.addOutput(output)
            output.metadataObjectTypes = AVCaptureMetadataOutput.supportedTypes
            output.setMetadataObjectsDelegate(self, queue: .main)
        }
        session.commitConfiguration()
    }

    private func configureInput(front: Bool) {
        let position: AVCaptureDevice.Position = front ? .front : .back
        guard let device = AVCaptureDevice.default(.builtInWideAngleCamera, for: .video, position: position),
              let input = try? AVCaptureDeviceInput(device: device),
              session.canAddInput(input) else { return }
        session.addInput(input)
        currentInput = input
    }
}

extension ScannerViewModel: AVCaptureMetadataOutputObjectsDelegate {
    nonisolated func metadataOutput(
        _ output: AVCaptureMetadataOutput,
        didOutput metadataObjects: [AVMetadataObject],
        from connection: AVCaptureConnection
    ) {
        guard let obj = metadataObjects.first as? AVMetadataMachineReadableCodeObject,
              let text = obj.stringValue else { return }
        Task { @MainActor in
            self.lastResult = ScanResult(text: text, formatName: obj.type.rawValue.uppercased())
        }
    }
}

extension AVCaptureMetadataOutput {
    static var supportedTypes: [AVMetadataObject.ObjectType] {
        [
            .qr, .ean13, .ean8, .code128, .code39, .code93,
            .upce, .pdf417, .aztec, .dataMatrix, .interleaved2of5,
            .itf14, .code39Mod43
        ]
    }
}
