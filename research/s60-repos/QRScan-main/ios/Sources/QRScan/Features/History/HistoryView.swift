import SwiftUI

struct HistoryView: View {
    @EnvironmentObject private var historyStore: HistoryStore
    @State private var searchText = ""
    @State private var showClearConfirm = false

    private var filtered: [ScanResult] {
        guard !searchText.isEmpty else { return historyStore.scans }
        return historyStore.scans.filter {
            $0.text.localizedCaseInsensitiveContains(searchText) ||
            $0.formatName.localizedCaseInsensitiveContains(searchText)
        }
    }

    var body: some View {
        NavigationStack {
            Group {
                if historyStore.scans.isEmpty {
                    ContentUnavailableView(
                        "No History",
                        systemImage: "clock",
                        description: Text("Scanned codes will appear here.")
                    )
                } else {
                    List {
                        ForEach(filtered) { scan in
                            NavigationLink(destination: ResultView(result: scan)) {
                                ScanRowView(scan: scan)
                            }
                            .swipeActions(edge: .trailing, allowsFullSwipe: true) {
                                Button(role: .destructive) {
                                    historyStore.remove(scan)
                                } label: {
                                    Label("Delete", systemImage: "trash")
                                }
                            }
                            .swipeActions(edge: .leading) {
                                Button {
                                    historyStore.toggleFavorite(scan)
                                } label: {
                                    Label(
                                        scan.isFavorite ? "Unfavorite" : "Favorite",
                                        systemImage: scan.isFavorite ? "heart.slash" : "heart"
                                    )
                                }
                                .tint(scan.isFavorite ? .gray : .pink)
                            }
                        }
                    }
                    .listStyle(.insetGrouped)
                    .searchable(text: $searchText, prompt: "Search history")
                }
            }
            .navigationTitle("History")
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Menu {
                        Button(role: .destructive) {
                            showClearConfirm = true
                        } label: {
                            Label("Clear non-favorites", systemImage: "trash")
                        }
                        Button(role: .destructive) {
                            historyStore.clearAll()
                        } label: {
                            Label("Clear all", systemImage: "trash.fill")
                        }
                    } label: {
                        Image(systemName: "ellipsis.circle")
                    }
                }
            }
            .confirmationDialog(
                "Clear non-favorite scans?",
                isPresented: $showClearConfirm,
                titleVisibility: .visible
            ) {
                Button("Clear", role: .destructive) {
                    historyStore.clearNonFavorites()
                }
                Button("Cancel", role: .cancel) {}
            }
        }
    }
}

struct ScanRowView: View {
    let scan: ScanResult

    private static let dateFormatter: DateFormatter = {
        let f = DateFormatter()
        f.dateStyle = .short
        f.timeStyle = .short
        return f
    }()

    var body: some View {
        HStack(spacing: 12) {
            Image(systemName: scan.contentType.systemImage)
                .font(.title3)
                .foregroundStyle(.tint)
                .frame(width: 36, height: 36)
                .background(.tint.opacity(0.12))
                .clipShape(RoundedRectangle(cornerRadius: 8))

            VStack(alignment: .leading, spacing: 2) {
                Text(scan.label.isEmpty ? scan.text : scan.label)
                    .lineLimit(1)
                    .font(.body)

                HStack(spacing: 4) {
                    Text(scan.formatName.replacingOccurrences(of: "_", with: " "))
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                    Text("•")
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                    Text(Self.dateFormatter.string(from: scan.timestamp))
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                }
            }

            Spacer()

            if scan.isFavorite {
                Image(systemName: "heart.fill")
                    .font(.caption)
                    .foregroundStyle(.pink)
            }
        }
        .padding(.vertical, 2)
    }
}
