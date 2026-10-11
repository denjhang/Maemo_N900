import SwiftUI

struct CreateView: View {
    @State private var content      = ""
    @State private var format       = QRCreatorFormat.qrCode
    @State private var foreground   = Color.black
    @State private var background   = Color.white
    @State private var generated: UIImage?
    @State private var errorMessage: String?
    @State private var showShareSheet = false
    @State private var isGenerating  = false

    private let creator = QRCreatorService()

    var body: some View {
        NavigationStack {
            Form {
                Section("Content") {
                    TextEditor(text: $content)
                        .frame(minHeight: 80)
                        .onChange(of: content) { _ in generated = nil; errorMessage = nil }
                }

                Section("Format") {
                    Picker("Format", selection: $format) {
                        ForEach(QRCreatorFormat.allCases) { f in
                            Text(f.rawValue).tag(f)
                        }
                    }
                    .pickerStyle(.menu)
                }

                Section("Colors") {
                    ColorPicker("Foreground", selection: $foreground)
                    ColorPicker("Background", selection: $background)
                }

                Section {
                    Button {
                        generate()
                    } label: {
                        HStack {
                            Spacer()
                            if isGenerating {
                                ProgressView()
                            } else {
                                Text("Generate")
                                    .bold()
                            }
                            Spacer()
                        }
                    }
                    .disabled(content.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty || isGenerating)
                }

                if let msg = errorMessage {
                    Section {
                        Text(msg)
                            .foregroundStyle(.red)
                            .font(.caption)
                    }
                }

                if let image = generated {
                    Section("Result") {
                        Image(uiImage: image)
                            .resizable()
                            .interpolation(.none)
                            .scaledToFit()
                            .frame(maxWidth: .infinity)
                            .padding(.vertical, 8)

                        HStack(spacing: 12) {
                            Button {
                                saveToPhotos(image)
                            } label: {
                                Label("Save", systemImage: "square.and.arrow.down")
                                    .frame(maxWidth: .infinity)
                            }
                            .buttonStyle(.borderedProminent)

                            Button {
                                showShareSheet = true
                            } label: {
                                Label("Share", systemImage: "square.and.arrow.up")
                                    .frame(maxWidth: .infinity)
                            }
                            .buttonStyle(.bordered)
                        }
                    }
                }
            }
            .navigationTitle("Create QR Code")
            .sheet(isPresented: $showShareSheet) {
                if let image = generated {
                    ShareSheet(items: [image])
                }
            }
        }
    }

    private func generate() {
        let trimmed = content.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return }
        isGenerating = true
        errorMessage = nil

        Task.detached(priority: .userInitiated) {
            do {
                let fg = await UIColor(foreground)
                let bg = await UIColor(background)
                let image = try QRCreatorService().generate(
                    content: trimmed,
                    format: format,
                    size: 1024,
                    foreground: fg,
                    background: bg
                )
                await MainActor.run {
                    generated    = image
                    isGenerating = false
                }
            } catch {
                await MainActor.run {
                    errorMessage = error.localizedDescription
                    isGenerating = false
                }
            }
        }
    }

    private func saveToPhotos(_ image: UIImage) {
        UIImageWriteToSavedPhotosAlbum(image, nil, nil, nil)
    }
}

struct ShareSheet: UIViewControllerRepresentable {
    let items: [Any]

    func makeUIViewController(context: Context) -> UIActivityViewController {
        UIActivityViewController(activityItems: items, applicationActivities: nil)
    }

    func updateUIViewController(_ uiViewController: UIActivityViewController, context: Context) {}
}
