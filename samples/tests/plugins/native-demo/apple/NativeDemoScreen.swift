import Foundation

#if os(macOS)
import AppKit

// A native screen over the whole window of the app, with a title, a line of text and a button, shown as a sheet of the window. Return presses the button. The demo shows it as its covering native screen and, through a screen of the plugin, as its fake full-screen ad.
@MainActor
final class NativeDemoScreen: NSViewController {
    private let heading: String
    private let text: String
    private let color: CGColor
    private let action: String
    private let acted: () -> Void

    private init(title: String, detail: String, color: CGColor, action: String, acted: @escaping () -> Void) {
        heading = title
        text = detail
        self.color = color
        self.action = action
        self.acted = acted
        super.init(nibName: nil, bundle: nil)
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("NativeDemoScreen is built in code only.")
    }

    // The controller for a screen of the plugin, which calls `acted` when the person presses the button and leaves the dismissal to the screen.
    static func controller(title: String, detail: String, color: CGColor, action: String, acted: @escaping () -> Void) -> NSViewController {
        NativeDemoScreen(title: title, detail: detail, color: color, action: action, acted: acted)
    }

    // Shows the screen as a sheet of the window and returns once Close ended it.
    static func show(title: String, color: CGColor, over window: NSWindow) async {
        let sheet = NSWindow(contentRect: NSRect(origin: .zero, size: window.contentLayoutRect.size), styleMask: [.titled], backing: .buffered, defer: false)
        let detail = "This AppKit sheet covers the app, which stands still and stays silent until Close ends the cover."
        sheet.contentViewController = NativeDemoScreen(title: title, detail: detail, color: color, action: "Close") { [unowned window, unowned sheet] in
            window.endSheet(sheet)
        }
        sheet.setContentSize(window.contentLayoutRect.size)
        await withCheckedContinuation { continuation in
            window.beginSheet(sheet) { _ in
                continuation.resume()
            }
        }
    }

    override func loadView() {
        let content = NSView(frame: NSRect(x: 0, y: 0, width: 640, height: 400))
        content.wantsLayer = true
        content.layer?.backgroundColor = color
        let title = NSTextField(labelWithString: heading)
        title.font = .systemFont(ofSize: 34, weight: .bold)
        title.textColor = .white
        let detail = NSTextField(wrappingLabelWithString: text)
        detail.textColor = .white
        detail.alignment = .center
        let button = NSButton(title: action, target: self, action: #selector(press))
        button.keyEquivalent = "\r"
        let column = NSStackView(views: [title, detail, button])
        column.orientation = .vertical
        column.spacing = 24
        column.translatesAutoresizingMaskIntoConstraints = false
        content.addSubview(column)
        NSLayoutConstraint.activate([column.centerXAnchor.constraint(equalTo: content.centerXAnchor), column.centerYAnchor.constraint(equalTo: content.centerYAnchor), column.widthAnchor.constraint(lessThanOrEqualTo: content.widthAnchor, constant: -64)])
        view = content
    }

    @objc private func press() {
        acted()
    }
}
#else
import UIKit

// A native screen over the whole app, with a title, a line of text and a button, presented full screen over the view controller of the app. The demo shows it as its covering native screen and, through a screen of the plugin, as its fake full-screen ad. On tvOS the focus starts on the button, and the Menu button of the remote closes the screen too.
final class NativeDemoScreen: UIViewController {
    private let heading: String
    private let text: String
    private let color: CGColor
    private let button: UIButton
    private let acted: (NativeDemoScreen) -> Void
    private var closed: (() -> Void)?

    private init(title: String, detail: String, color: CGColor, action: String, acted: @escaping (NativeDemoScreen) -> Void, closed: (() -> Void)?) {
        heading = title
        text = detail
        self.color = color
        self.acted = acted
        self.closed = closed
        button = UIButton(configuration: .filled())
        button.configuration?.title = action
        super.init(nibName: nil, bundle: nil)
        modalPresentationStyle = .fullScreen
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("NativeDemoScreen is built in code only.")
    }

    // The controller for a screen of the plugin, which calls `acted` when the person presses the button and leaves the dismissal to the screen.
    static func controller(title: String, detail: String, color: CGColor, action: String, acted: @escaping () -> Void) -> UIViewController {
        NativeDemoScreen(title: title, detail: detail, color: color, action: action, acted: { _ in acted() }, closed: nil)
    }

    // Presents the screen over the view controller and returns once it closed.
    static func show(title: String, color: CGColor, over presenter: UIViewController) async {
        let detail = "This UIKit screen covers the app, which stands still and stays silent until Close ends the cover."
        await withCheckedContinuation { continuation in
            presenter.present(NativeDemoScreen(title: title, detail: detail, color: color, action: "Close", acted: { $0.dismiss(animated: true) }, closed: { continuation.resume() }), animated: true)
        }
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = UIColor(cgColor: color)
        let title = UILabel()
        title.text = heading
        title.font = .preferredFont(forTextStyle: .title1)
        title.textColor = .white
        let detail = UILabel()
        detail.text = text
        detail.textColor = .white
        detail.numberOfLines = 0
        detail.textAlignment = .center
        button.addTarget(self, action: #selector(press), for: .primaryActionTriggered)
        let column = UIStackView(arrangedSubviews: [title, detail, button])
        column.axis = .vertical
        column.alignment = .center
        column.spacing = 24
        column.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(column)
        NSLayoutConstraint.activate([column.centerXAnchor.constraint(equalTo: view.safeAreaLayoutGuide.centerXAnchor), column.centerYAnchor.constraint(equalTo: view.safeAreaLayoutGuide.centerYAnchor), column.widthAnchor.constraint(lessThanOrEqualTo: view.safeAreaLayoutGuide.widthAnchor, constant: -64)])
    }

    override var preferredFocusEnvironments: [UIFocusEnvironment] {
        [button]
    }

    // The screen closes through its button or, on tvOS, through the Menu button, and either way reports once.
    override func viewDidDisappear(_ animated: Bool) {
        super.viewDidDisappear(animated)
        if isBeingDismissed {
            closed?()
            closed = nil
        }
    }

    @objc private func press() {
        acted(self)
    }
}
#endif
