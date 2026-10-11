import Foundation

struct ScanResult: Identifiable, Codable, Hashable {
    let id: UUID
    let text: String
    let formatName: String
    let timestamp: Date
    var isFavorite: Bool
    var label: String

    init(
        id: UUID = UUID(),
        text: String,
        formatName: String,
        timestamp: Date = Date(),
        isFavorite: Bool = false,
        label: String = ""
    ) {
        self.id         = id
        self.text       = text
        self.formatName = formatName
        self.timestamp  = timestamp
        self.isFavorite = isFavorite
        self.label      = label
    }

    var contentType: QRContentType { QRContentTypeResolver.resolve(text) }
    var isURL: Bool { contentType == .url }
    var url: URL? { URL(string: text) }
}

enum QRContentType: String {
    case url, wifi, vcard, mecard, geo, email, phone, sms, calendar, bitcoin, text

    var label: String {
        switch self {
        case .url:      return "URL"
        case .wifi:     return "Wi-Fi Network"
        case .vcard:    return "Contact (vCard)"
        case .mecard:   return "Contact (MECARD)"
        case .geo:      return "Geographic Location"
        case .email:    return "Email Address"
        case .phone:    return "Phone Number"
        case .sms:      return "SMS"
        case .calendar: return "Calendar Event"
        case .bitcoin:  return "Bitcoin Address"
        case .text:     return "Plain Text"
        }
    }

    var systemImage: String {
        switch self {
        case .url:      return "link"
        case .wifi:     return "wifi"
        case .vcard:    return "person.crop.rectangle"
        case .mecard:   return "person.crop.rectangle"
        case .geo:      return "map"
        case .email:    return "envelope"
        case .phone:    return "phone"
        case .sms:      return "message"
        case .calendar: return "calendar"
        case .bitcoin:  return "bitcoinsign.circle"
        case .text:     return "doc.text"
        }
    }
}

enum QRContentTypeResolver {
    static func resolve(_ text: String) -> QRContentType {
        let t = text.trimmingCharacters(in: .whitespacesAndNewlines)
        if t.lowercased().hasPrefix("wifi:")            { return .wifi }
        if t.uppercased().hasPrefix("BEGIN:VCARD")      { return .vcard }
        if t.uppercased().hasPrefix("MECARD:")          { return .mecard }
        if t.uppercased().hasPrefix("BEGIN:VCALENDAR")  { return .calendar }
        if t.lowercased().hasPrefix("geo:")             { return .geo }
        if t.lowercased().hasPrefix("mailto:")          { return .email }
        if t.lowercased().hasPrefix("tel:")             { return .phone }
        if t.lowercased().hasPrefix("sms")              { return .sms }
        if t.lowercased().hasPrefix("bitcoin:")         { return .bitcoin }
        if t.lowercased().hasPrefix("http://") ||
           t.lowercased().hasPrefix("https://")         { return .url }
        return .text
    }
}
