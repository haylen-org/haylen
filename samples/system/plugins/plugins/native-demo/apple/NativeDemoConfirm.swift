import Foundation

#if os(macOS)
import AppKit

// The confirm screen on macOS: a question with Confirm, which Return presses, and Decline, which Escape presses. The screen shows it in a sheet of the window of the app and dismisses it once it answered.
final class NativeDemoConfirm: NSViewController {
    private let question: String
    private let answered: @MainActor (Bool) -> Void

    init(title: String, question: String, answered: @escaping @MainActor (Bool) -> Void) {
        self.question = question
        self.answered = answered
        super.init(nibName: nil, bundle: nil)
        self.title = title
        preferredContentSize = NSSize(width: 480, height: 220)
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("NativeDemoConfirm is built in code only.")
    }

    override func loadView() {
        let heading = NSTextField(labelWithString: title ?? "")
        heading.font = .systemFont(ofSize: 24, weight: .bold)
        let detail = NSTextField(wrappingLabelWithString: question)
        detail.alignment = .center
        let confirm = NSButton(title: "Confirm", target: self, action: #selector(confirmed))
        confirm.keyEquivalent = "\r"
        let decline = NSButton(title: "Decline", target: self, action: #selector(declined))
        decline.keyEquivalent = "\u{1b}"
        let buttons = NSStackView(views: [decline, confirm])
        buttons.spacing = 12
        let column = NSStackView(views: [heading, detail, buttons])
        column.orientation = .vertical
        column.spacing = 20
        column.edgeInsets = NSEdgeInsets(top: 32, left: 32, bottom: 32, right: 32)
        view = column
    }

    @objc private func confirmed() {
        answered(true)
    }

    @objc private func declined() {
        answered(false)
    }
}
#else
import UIKit

// The confirm screen on iOS, iPadOS, Mac Catalyst and tvOS: a question with Confirm and Decline, where the focus of the remote starts on Confirm. The screen presents it over the app and dismisses it once it answered, while a swipe or the Menu button of the remote closes it without an answer.
final class NativeDemoConfirm: UIViewController {
    private let question: String
    private let answered: @MainActor (Bool) -> Void
    private let confirm = UIButton(configuration: .filled())

    init(title: String, question: String, answered: @escaping @MainActor (Bool) -> Void) {
        self.question = question
        self.answered = answered
        super.init(nibName: nil, bundle: nil)
        self.title = title
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("NativeDemoConfirm is built in code only.")
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = UIColor(red: 0.11, green: 0.21, blue: 0.34, alpha: 1)
        let heading = UILabel()
        heading.text = title
        heading.font = .preferredFont(forTextStyle: .title1)
        heading.textColor = .white
        let detail = UILabel()
        detail.text = question
        detail.textColor = .white
        detail.numberOfLines = 0
        detail.textAlignment = .center
        confirm.configuration?.title = "Confirm"
        confirm.addTarget(self, action: #selector(confirmed), for: .primaryActionTriggered)
        let decline = UIButton(configuration: .gray())
        decline.configuration?.title = "Decline"
        decline.configuration?.baseForegroundColor = .white
        decline.addTarget(self, action: #selector(declined), for: .primaryActionTriggered)
        let buttons = UIStackView(arrangedSubviews: [decline, confirm])
        buttons.spacing = 16
        let column = UIStackView(arrangedSubviews: [heading, detail, buttons])
        column.axis = .vertical
        column.alignment = .center
        column.spacing = 24
        column.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(column)
        NSLayoutConstraint.activate([column.centerXAnchor.constraint(equalTo: view.safeAreaLayoutGuide.centerXAnchor), column.centerYAnchor.constraint(equalTo: view.safeAreaLayoutGuide.centerYAnchor), column.widthAnchor.constraint(lessThanOrEqualTo: view.safeAreaLayoutGuide.widthAnchor, constant: -64)])
    }

    override var preferredFocusEnvironments: [UIFocusEnvironment] {
        [confirm]
    }

    @objc private func confirmed() {
        answered(true)
    }

    @objc private func declined() {
        answered(false)
    }
}
#endif
