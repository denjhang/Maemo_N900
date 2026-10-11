import SwiftUI

struct FavoritesView: View {
    @EnvironmentObject private var historyStore: HistoryStore

    var body: some View {
        NavigationStack {
            Group {
                if historyStore.favorites.isEmpty {
                    ContentUnavailableView(
                        "No Favorites",
                        systemImage: "heart",
                        description: Text("Tap the heart on any result to save it here.")
                    )
                } else {
                    List {
                        ForEach(historyStore.favorites) { scan in
                            NavigationLink(destination: ResultView(result: scan)) {
                                ScanRowView(scan: scan)
                            }
                            .swipeActions(edge: .leading) {
                                Button {
                                    historyStore.toggleFavorite(scan)
                                } label: {
                                    Label("Unfavorite", systemImage: "heart.slash")
                                }
                                .tint(.pink)
                            }
                        }
                    }
                    .listStyle(.insetGrouped)
                }
            }
            .navigationTitle("Favorites")
        }
    }
}
