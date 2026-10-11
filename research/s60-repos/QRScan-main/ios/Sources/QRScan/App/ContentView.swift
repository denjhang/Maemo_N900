import SwiftUI

struct ContentView: View {
    @State private var selectedTab: Tab = .scanner

    enum Tab {
        case scanner, history, favorites, create, settings
    }

    var body: some View {
        TabView(selection: $selectedTab) {
            ScannerView()
                .tabItem {
                    Label("Scan", systemImage: "qrcode.viewfinder")
                }
                .tag(Tab.scanner)

            HistoryView()
                .tabItem {
                    Label("History", systemImage: "clock")
                }
                .tag(Tab.history)

            FavoritesView()
                .tabItem {
                    Label("Favorites", systemImage: "heart")
                }
                .tag(Tab.favorites)

            CreateView()
                .tabItem {
                    Label("Create", systemImage: "qrcode")
                }
                .tag(Tab.create)

            SettingsView()
                .tabItem {
                    Label("Settings", systemImage: "gearshape")
                }
                .tag(Tab.settings)
        }
    }
}
