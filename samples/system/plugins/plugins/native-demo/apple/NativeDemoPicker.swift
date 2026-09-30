import Foundation

#if os(macOS)
import AppKit

// Lets the person pick a file with an NSOpenPanel shown as a sheet of the window of the app.
@MainActor
enum NativeDemoPicker {
    // Returns the name of the picked file, or nil when the person cancelled.
    static func pick(over window: NSWindow) async -> String? {
        let panel = NSOpenPanel()
        panel.canChooseFiles = true
        panel.canChooseDirectories = false
        panel.allowsMultipleSelection = false
        let response = await panel.beginSheetModal(for: window)
        return response == .OK ? panel.url?.lastPathComponent : nil
    }
}
#elseif os(iOS)
import UIKit
import UniformTypeIdentifiers

// Lets the person pick a file with the document picker of UIKit presented over the app, on iOS, iPadOS and Mac Catalyst. The picker is its own delegate, since a picker keeps its delegate weakly.
final class NativeDemoPicker: UIDocumentPickerViewController, UIDocumentPickerDelegate {
    private var picked: ((String?) -> Void)?

    // Returns the name of the picked file, or nil when the person cancelled.
    static func pick(over presenter: UIViewController) async -> String? {
        await withCheckedContinuation { continuation in
            let picker = NativeDemoPicker(forOpeningContentTypes: [.item], asCopy: true)
            picker.picked = { continuation.resume(returning: $0) }
            picker.delegate = picker
            picker.allowsMultipleSelection = false
            presenter.present(picker, animated: true)
        }
    }

    func documentPicker(_ controller: UIDocumentPickerViewController, didPickDocumentsAt urls: [URL]) {
        finish(urls.first?.lastPathComponent)
    }

    func documentPickerWasCancelled(_ controller: UIDocumentPickerViewController) {
        finish(nil)
    }

    private func finish(_ name: String?) {
        picked?(name)
        picked = nil
    }
}
#endif
