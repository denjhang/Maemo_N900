import Foundation
import Combine

final class FavoritesStore: ObservableObject {
    @Published private(set) var favorites: [ScanResult] = []

    func sync(from store: HistoryStore) {
        favorites = store.favorites
    }
}

final class SettingsStore: ObservableObject {
    enum ThemeMode: String, CaseIterable {
        case system, light, dark
        var label: String {
            switch self {
            case .system: return "Follow system"
            case .light:  return "Light"
            case .dark:   return "Dark"
            }
        }
    }

    @Published var themeMode: ThemeMode {
        didSet { UserDefaults.standard.set(themeMode.rawValue, forKey: "theme_mode") }
    }
    @Published var haptics: Bool {
        didSet { UserDefaults.standard.set(haptics, forKey: "haptics") }
    }
    @Published var saveHistory: Bool {
        didSet { UserDefaults.standard.set(saveHistory, forKey: "save_history") }
    }
    @Published var beepOnScan: Bool {
        didSet { UserDefaults.standard.set(beepOnScan, forKey: "beep_on_scan") }
    }

    init() {
        let raw = UserDefaults.standard.string(forKey: "theme_mode") ?? ""
        themeMode   = ThemeMode(rawValue: raw) ?? .system
        haptics     = UserDefaults.standard.object(forKey: "haptics") as? Bool ?? true
        saveHistory = UserDefaults.standard.object(forKey: "save_history") as? Bool ?? true
        beepOnScan  = UserDefaults.standard.object(forKey: "beep_on_scan") as? Bool ?? false
    }
}
