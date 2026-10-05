import Foundation

#if os(macOS)
import AppKit
#else
import UIKit
#endif

// The native sheet of the fake store, ads and sign-in, which asks the person to pick one of a few choices the way the sheets of SDKs ask: an alert of UIKit that the screen presents on iOS, iPadOS, Mac Catalyst and tvOS, and an alert sheet of the window of the app on macOS. A screen that the app gives up dismisses the sheet and ends as `cancelled`.
@MainActor
enum NativeDemoSheet {
    struct Choice {
        let id: String
        let title: String
        var cancels = false
    }

    // Shows the sheet in the screen and calls `picked` once with the id of the choice the person picked, or `nil` for the choice that cancels.
    static func ask(_ title: String, message: String, choices: [Choice], in screen: HaylenScreen, picked: @escaping (String?) -> Void) {
        #if os(macOS)
        guard let window = screen.window else {
            screen.fail("The app has no window yet.", code: "noWindow", data: nil)
            return
        }
        let alert = NSAlert()
        alert.messageText = title
        alert.informativeText = message
        for choice in choices {
            alert.addButton(withTitle: choice.title)
        }
        screen.cancelHandler = { [weak window, weak screen] in
            window?.endSheet(alert.window)
            screen?.fail("The app gave the screen up.", code: "cancelled", data: nil)
        }
        alert.beginSheetModal(for: window) { response in
            let index = response.rawValue - NSApplication.ModalResponse.alertFirstButtonReturn.rawValue
            picked(choices.indices.contains(index) && !choices[index].cancels ? choices[index].id : nil)
        }
        #else
        guard let presenter = screen.presenter else {
            screen.fail("The app has no window yet.", code: "noWindow", data: nil)
            return
        }
        let alert = UIAlertController(title: title, message: message, preferredStyle: .alert)
        for choice in choices {
            alert.addAction(UIAlertAction(title: choice.title, style: choice.cancels ? .cancel : .default) { _ in
                picked(choice.cancels ? nil : choice.id)
            })
        }
        screen.cancelHandler = { [weak alert, weak screen] in
            alert?.dismiss(animated: true)
            screen?.fail("The app gave the screen up.", code: "cancelled", data: nil)
        }
        presenter.present(alert, animated: true)
        #endif
    }
}
