import CoreImage
import CoreImage.CIFilterBuiltins
import UIKit

enum QRCreatorFormat: String, CaseIterable, Identifiable {
    case qrCode       = "QR Code"
    case aztec        = "Aztec"
    case pdf417       = "PDF 417"
    case code128      = "Code 128"

    var id: String { rawValue }
}

enum QRCreatorError: LocalizedError {
    case emptyContent
    case generationFailed
    case unsupportedFormat

    var errorDescription: String? {
        switch self {
        case .emptyContent:      return "Content cannot be empty."
        case .generationFailed:  return "Failed to generate barcode. Try different content."
        case .unsupportedFormat: return "Format not supported on this device."
        }
    }
}

struct QRCreatorService {

    func generate(
        content: String,
        format: QRCreatorFormat,
        size: CGFloat = 512,
        foreground: UIColor = .black,
        background: UIColor = .white
    ) throws -> UIImage {
        guard !content.isEmpty else { throw QRCreatorError.emptyContent }

        let ciImage: CIImage = switch format {
        case .qrCode:  try generateQR(content: content)
        case .aztec:   try generateAztec(content: content)
        case .pdf417:  try generatePDF417(content: content)
        case .code128: try generateCode128(content: content)
        }

        let colored  = applyColors(to: ciImage, foreground: foreground, background: background)
        let scaled   = scale(image: colored, to: size)
        guard let cgImage = CIContext().createCGImage(scaled, from: scaled.extent) else {
            throw QRCreatorError.generationFailed
        }
        return UIImage(cgImage: cgImage)
    }

    private func generateQR(content: String) throws -> CIImage {
        let filter = CIFilter.qrCodeGenerator()
        filter.message = Data(content.utf8)
        filter.correctionLevel = "M"
        guard let output = filter.outputImage else { throw QRCreatorError.generationFailed }
        return output
    }

    private func generateAztec(content: String) throws -> CIImage {
        let filter = CIFilter.aztecCodeGenerator()
        filter.message = Data(content.utf8)
        filter.correctionLevel = 23
        guard let output = filter.outputImage else { throw QRCreatorError.generationFailed }
        return output
    }

    private func generatePDF417(content: String) throws -> CIImage {
        let filter = CIFilter.pdf417BarcodeGenerator()
        filter.message = Data(content.utf8)
        guard let output = filter.outputImage else { throw QRCreatorError.generationFailed }
        return output
    }

    private func generateCode128(content: String) throws -> CIImage {
        let filter = CIFilter.code128BarcodeGenerator()
        filter.message = Data(content.utf8)
        guard let output = filter.outputImage else { throw QRCreatorError.generationFailed }
        return output
    }

    private func applyColors(to image: CIImage, foreground: UIColor, background: UIColor) -> CIImage {
        let filter = CIFilter(name: "CIFalseColor")!
        filter.setValue(image, forKey: kCIInputImageKey)
        filter.setValue(CIColor(color: foreground), forKey: "inputColor0")
        filter.setValue(CIColor(color: background), forKey: "inputColor1")
        return filter.outputImage ?? image
    }

    private func scale(image: CIImage, to size: CGFloat) -> CIImage {
        let scaleX = size / image.extent.width
        let scaleY = size / image.extent.height
        return image.transformed(by: CGAffineTransform(scaleX: scaleX, y: scaleY))
    }
}
