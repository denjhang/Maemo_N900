import SwiftUI

@main
struct QRScanApp: App {
    @StateObject private var historyStore   = HistoryStore()
    @StateObject private var settingsStore  = SettingsStore()
    @StateObject private var favoritesStore = FavoritesStore()

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(historyStore)
                .environmentObject(settingsStore)
                .environmentObject(favoritesStore)
        }
    }
}
