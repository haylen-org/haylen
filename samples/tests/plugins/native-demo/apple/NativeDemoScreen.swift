import Foundation

#if os(macOS)
import AppKit

// A native screen over the whole window of the app, with a title, a line of text and a Close button, shown as a sheet of the window. Return presses Close.
@MainActor
final class NativeDemoScreen: NSObject {
    private let parent: NSWindow
    private let sheet: NSWindow

    private init(title: String, color: CGColor, over window: NSWindow) {
        parent = window
        sheet = NSWindow(contentRect: NSRect(origin: .zero, size: window.contentLayoutRect.size), styleMask: [.titled], backing: .buffered, defer: false)
        super.init()
        let content = NSView()
        content.wantsLayer = true
        content.layer?.backgroundColor = color
        let heading = NSTextField(labelWithString: title)
        heading.font = .systemFont(ofSize: 34, weight: .bold)
        heading.textColor = .white
        let detail = NSTextField(wrappingLabelWithString: "This AppKit sheet covers the app, which stands still and stays silent until Close ends the cover.")
        detail.textColor = .white
        detail.alignment = .center
        let close = NSButton(title: "Close", target: self, action: #selector(closeSheet))
        close.keyEquivalent = "\r"
        let column = NSStackView(views: [heading, detail, close])
        column.orientation = .vertical
        column.spacing = 24
        column.translatesAutoresizingMaskIntoConstraints = false
        content.addSubview(column)
        NSLayoutConstraint.activate([column.centerXAnchor.constraint(equalTo: content.centerXAnchor), column.centerYAnchor.constraint(equalTo: content.centerYAnchor), column.widthAnchor.constraint(lessThanOrEqualTo: content.widthAnchor, constant: -64)])
        sheet.contentView = content
    }

    // Shows the screen as a sheet of the window and returns once Close ended it.
    static func show(title: String, color: CGColor, over window: NSWindow) async {
        let screen = NativeDemoScreen(title: title, color: color, over: window)
        await withCheckedContinuation { continuation in
            window.beginSheet(screen.sheet) { _ in
                continuation.resume()
            }
        }
    }

    @objc private func closeSheet() {
        parent.endSheet(sheet)
    }
}
#else
import UIKit

// A native screen over the whole app, with a title, a line of text and a Close button, presented full screen over the view controller of the app. On tvOS the focus starts on Close, and the Menu button of the remote closes the screen too.
final class NativeDemoScreen: UIViewController {
    private let heading: String
    private let color: CGColor
    private let close: UIButton
    private var closed: (() -> Void)?

    private init(title: String, color: CGColor, closed: @escaping () -> Void) {
        heading = title
        self.color = color
        self.closed = closed
        close = UIButton(configuration: .filled())
        super.init(nibName: nil, bundle: nil)
        modalPresentationStyle = .fullScreen
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("NativeDemoScreen is built in code only.")
    }

    // Presents the screen over the view controller and returns once it closed.
    static func show(title: String, color: CGColor, over presenter: UIViewController) async {
        await withCheckedContinuation { continuation in
            presenter.present(NativeDemoScreen(title: title, color: color) { continuation.resume() }, animated: true)
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
        detail.text = "This UIKit screen covers the app, which stands still and stays silent until Close ends the cover."
        detail.textColor = .white
        detail.numberOfLines = 0
        detail.textAlignment = .center
        close.configuration?.title = "Close"
        close.addTarget(self, action: #selector(dismissScreen), for: .primaryActionTriggered)
        let column = UIStackView(arrangedSubviews: [title, detail, close])
        column.axis = .vertical
        column.alignment = .center
        column.spacing = 24
        column.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(column)
        NSLayoutConstraint.activate([column.centerXAnchor.constraint(equalTo: view.safeAreaLayoutGuide.centerXAnchor), column.centerYAnchor.constraint(equalTo: view.safeAreaLayoutGuide.centerYAnchor), column.widthAnchor.constraint(lessThanOrEqualTo: view.safeAreaLayoutGuide.widthAnchor, constant: -64)])
    }

    override var preferredFocusEnvironments: [UIFocusEnvironment] {
        [close]
    }

    // The screen closes through its button or, on tvOS, through the Menu button, and either way reports once.
    override func viewDidDisappear(_ animated: Bool) {
        super.viewDidDisappear(animated)
        if isBeingDismissed {
            closed?()
            closed = nil
        }
    }

    @objc private func dismissScreen() {
        dismiss(animated: true)
    }
}
#endif
