import SwiftUI

struct ResultView: View {
    let result: ScanResult
    @EnvironmentObject private var historyStore: HistoryStore
    @State private var showEdit = false
    @State private var editText = ""
    @State private var copied  = false

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 20) {
                HStack(spacing: 8) {
                    Image(systemName: result.contentType.systemImage)
                        .foregroundStyle(.tint)
                    Text(result.contentType.label)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                    Spacer()
                    Text(result.formatName.replacingOccurrences(of: "_", with: " "))
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                        .padding(.horizontal, 8)
                        .padding(.vertical, 2)
                        .background(.quaternary, in: Capsule())
                }

                GroupBox {
                    Text(result.text)
                        .font(.body)
                        .textSelection(.enabled)
                        .frame(maxWidth: .infinity, alignment: .leading)
                }

                LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible())], spacing: 12) {
                    ActionButton(title: copied ? "Copied!" : "Copy", systemImage: "doc.on.doc") {
                        UIPasteboard.general.string = result.text
                        withAnimation { copied = true }
                        DispatchQueue.main.asyncAfter(deadline: .now() + 2) {
                            withAnimation { copied = false }
                        }
                    }

                    ActionButton(title: "Share", systemImage: "square.and.arrow.up") {
                        let av = UIActivityViewController(
                            activityItems: [result.text], applicationActivities: nil)
                        UIApplication.shared
                            .connectedScenes
                            .compactMap { $0 as? UIWindowScene }
                            .first?.windows.first?
                            .rootViewController?
                            .present(av, animated: true)
                    }

                    if result.isURL {
                        ActionButton(title: "Open URL", systemImage: "safari") {
                            if let url = result.url { UIApplication.shared.open(url) }
                        }
                    }

                    ActionButton(title: "Edit", systemImage: "pencil") {
                        editText = result.text
                        showEdit = true
                    }

                    ActionButton(
                        title: result.isFavorite ? "Unfavorite" : "Favorite",
                        systemImage: result.isFavorite ? "heart.fill" : "heart"
                    ) {
                        historyStore.toggleFavorite(result)
                    }
                }
            }
            .padding()
        }
        .navigationTitle("Result")
        .navigationBarTitleDisplayMode(.inline)
        .sheet(isPresented: $showEdit) {
            EditLabelSheet(text: $editText) { newText in
                historyStore.updateLabel(result, label: newText)
            }
        }
        .onAppear {
            historyStore.add(result)
        }
    }
}

struct ActionButton: View {
    let title: String
    let systemImage: String
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            Label(title, systemImage: systemImage)
                .font(.subheadline.weight(.medium))
                .frame(maxWidth: .infinity)
                .padding(.vertical, 12)
                .background(.tint.opacity(0.12))
                .foregroundStyle(.tint)
                .clipShape(RoundedRectangle(cornerRadius: 12))
        }
    }
}

struct EditLabelSheet: View {
    @Binding var text: String
    @Environment(\.dismiss) private var dismiss
    let onSave: (String) -> Void

    var body: some View {
        NavigationStack {
            Form {
                Section("Edit content") {
                    TextEditor(text: $text)
                        .frame(minHeight: 120)
                }
            }
            .navigationTitle("Edit")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Cancel") { dismiss() }
                }
                ToolbarItem(placement: .confirmationAction) {
                    Button("Save") {
                        onSave(text)
                        dismiss()
                    }
                }
            }
        }
    }
}
