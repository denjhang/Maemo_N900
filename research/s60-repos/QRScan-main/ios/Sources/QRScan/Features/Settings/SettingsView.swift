import SwiftUI

struct SettingsView: View {
    @EnvironmentObject private var settings: SettingsStore

    var body: some View {
        NavigationStack {
            Form {
                Section("Appearance") {
                    Picker("Theme", selection: $settings.themeMode) {
                        ForEach(SettingsStore.ThemeMode.allCases, id: \.self) { mode in
                            Text(mode.label).tag(mode)
                        }
                    }
                }

                Section("Scanning") {
                    Toggle("Haptic feedback", isOn: $settings.haptics)
                    Toggle("Beep on scan", isOn: $settings.beepOnScan)
                    Toggle("Save scan history", isOn: $settings.saveHistory)
                }

                Section("About") {
                    LabeledContent("App", value: "QRScan")
                    LabeledContent("Version", value: "1.0.0")
                    LabeledContent("License", value: "GPL-3.0")
                    LabeledContent("Developer", value: "Hexadecinull")

                    Link(destination: URL(string: "https://github.com/Hexadecinull/QRScan")!) {
                        Label("Source Code on GitHub", systemImage: "chevron.left.slash.chevron.right")
                    }
                }
            }
            .navigationTitle("Settings")
        }
    }
}
