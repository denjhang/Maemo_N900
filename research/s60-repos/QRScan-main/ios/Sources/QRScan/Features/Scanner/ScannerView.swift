import SwiftUI
import AVFoundation
import PhotosUI

struct ScannerView: View {
    @StateObject private var model = ScannerViewModel()
    @State private var showResult = false
    @State private var selectedPhoto: PhotosPickerItem?

    var body: some View {
        NavigationStack {
            ZStack {
                CameraPreviewRepresentable(session: model.session)
                    .ignoresSafeArea()

                ScannerOverlayView()

                VStack {
                    Spacer()

                    HStack(spacing: 24) {
                        controlButton(
                            systemImage: model.isTorchOn ? "bolt.fill" : "bolt.slash",
                            tint: model.isTorchOn ? .yellow : .white
                        ) {
                            model.toggleTorch()
                        }

                        controlButton(
                            systemImage: model.isFrontCamera ? "camera.rotate.fill" : "camera.rotate",
                            tint: .white
                        ) {
                            model.flipCamera()
                        }

                        PhotosPicker(selection: $selectedPhoto, matching: .images) {
                            Image(systemName: "photo.on.rectangle")
                                .font(.system(size: 22, weight: .medium))
                                .foregroundStyle(.white)
                                .frame(width: 52, height: 52)
                                .background(.black.opacity(0.45))
                                .clipShape(Circle())
                        }
                    }
                    .padding(.bottom, 32)
                }
            }
            .navigationTitle("QRScan")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    if model.isScanning {
                        ProgressView()
                    }
                }
            }
            .onChange(of: selectedPhoto) { item in
                guard let item else { return }
                Task {
                    if let data = try? await item.loadTransferable(type: Data.self),
                       let uiImage = UIImage(data: data) {
                        model.decodeFromImage(uiImage)
                    }
                }
            }
            .onReceive(model.$lastResult) { result in
                guard result != nil else { return }
                showResult = true
            }
            .navigationDestination(isPresented: $showResult) {
                if let result = model.lastResult {
                    ResultView(result: result)
                        .onDisappear { model.resumeScanning() }
                }
            }
            .onAppear { model.startSession() }
            .onDisappear { model.stopSession() }
        }
    }

    @ViewBuilder
    private func controlButton(
        systemImage: String,
        tint: Color,
        action: @escaping () -> Void
    ) -> some View {
        Button(action: action) {
            Image(systemName: systemImage)
                .font(.system(size: 22, weight: .medium))
                .foregroundStyle(tint)
                .frame(width: 52, height: 52)
                .background(.black.opacity(0.45))
                .clipShape(Circle())
        }
    }
}

struct ScannerOverlayView: View {
    var body: some View {
        GeometryReader { geo in
            let side = min(geo.size.width, geo.size.height) * 0.65
            ZStack {
                Color.black.opacity(0.4)
                    .ignoresSafeArea()
                    .reverseMask {
                        RoundedRectangle(cornerRadius: 16)
                            .frame(width: side, height: side)
                    }

                RoundedRectangle(cornerRadius: 16)
                    .stroke(Color.white, lineWidth: 2)
                    .frame(width: side, height: side)
            }
        }
    }
}

extension View {
    @ViewBuilder
    func reverseMask<Mask: View>(@ViewBuilder _ mask: () -> Mask) -> some View {
        self.mask(
            Rectangle()
                .ignoresSafeArea()
                .overlay(mask().blendMode(.destinationOut))
                .compositingGroup()
        )
    }
}
