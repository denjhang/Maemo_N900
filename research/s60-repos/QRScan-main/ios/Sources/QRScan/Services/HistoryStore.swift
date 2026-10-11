import Foundation
import Combine

final class HistoryStore: ObservableObject {
    @Published private(set) var scans: [ScanResult] = []
    private let key = "qrscan_history"

    init() { load() }

    func add(_ result: ScanResult) {
        scans.insert(result, at: 0)
        save()
    }

    func remove(_ result: ScanResult) {
        scans.removeAll { $0.id == result.id }
        save()
    }

    func toggleFavorite(_ result: ScanResult) {
        guard let idx = scans.firstIndex(where: { $0.id == result.id }) else { return }
        scans[idx].isFavorite.toggle()
        save()
    }

    func updateLabel(_ result: ScanResult, label: String) {
        guard let idx = scans.firstIndex(where: { $0.id == result.id }) else { return }
        scans[idx].label = label
        save()
    }

    func clearNonFavorites() {
        scans.removeAll { !$0.isFavorite }
        save()
    }

    func clearAll() {
        scans.removeAll()
        save()
    }

    var favorites: [ScanResult] { scans.filter { $0.isFavorite } }

    private func save() {
        guard let data = try? JSONEncoder().encode(scans) else { return }
        UserDefaults.standard.set(data, forKey: key)
    }

    private func load() {
        guard let data = UserDefaults.standard.data(forKey: key),
              let decoded = try? JSONDecoder().decode([ScanResult].self, from: data)
        else { return }
        scans = decoded
    }
}
