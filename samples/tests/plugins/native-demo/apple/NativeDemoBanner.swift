import Foundation

#if os(macOS)
import AppKit
#else
import UIKit
#endif

// The native banner of the demo: a colored bar with the greeting and a Tap button, which the overlay of the plugin places over the app. The overlay passes every touch and click outside the bar to the app.
@MainActor
final class NativeDemoBanner: NSObject {
    static let width: CGFloat = 360
    static let height: CGFloat = 56

    private let tapped: () -> Void
    private var item: HaylenOverlay.Item?

    #if os(macOS)
    init(text: String, color: CGColor, overlay: HaylenOverlay, placement: HaylenPlacement, tapped: @escaping () -> Void) {
        self.tapped = tapped
        super.init()
        let bar = NSView()
        bar.wantsLayer = true
        bar.layer?.backgroundColor = color
        bar.layer?.cornerRadius = 10
        let label = NSTextField(labelWithString: text)
        label.textColor = .white
        label.font = .systemFont(ofSize: 16, weight: .semibold)
        label.lineBreakMode = .byTruncatingTail
        let button = NSButton(title: "Tap", target: self, action: #selector(tap))
        let row = NSStackView(views: [label, button])
        row.orientation = .horizontal
        row.spacing = 12
        row.edgeInsets = NSEdgeInsets(top: 8, left: 16, bottom: 8, right: 12)
        row.translatesAutoresizingMaskIntoConstraints = false
        bar.addSubview(row)
        NSLayoutConstraint.activate([row.leadingAnchor.constraint(equalTo: bar.leadingAnchor), row.trailingAnchor.constraint(equalTo: bar.trailingAnchor), row.centerYAnchor.constraint(equalTo: bar.centerYAnchor)])
        item = overlay.add(bar, placement: placement)
    }
    #else
    init(text: String, color: CGColor, overlay: HaylenOverlay, placement: HaylenPlacement, tapped: @escaping () -> Void) {
        self.tapped = tapped
        super.init()
        let bar = UIView()
        bar.backgroundColor = UIColor(cgColor: color)
        bar.layer.cornerRadius = 10
        let label = UILabel()
        label.text = text
        label.textColor = .white
        label.font = .systemFont(ofSize: 16, weight: .semibold)
        var configuration = UIButton.Configuration.filled()
        configuration.title = "Tap"
        let button = UIButton(configuration: configuration)
        button.addTarget(self, action: #selector(tap), for: .primaryActionTriggered)
        let row = UIStackView(arrangedSubviews: [label, button])
        row.axis = .horizontal
        row.spacing = 12
        row.alignment = .center
        row.translatesAutoresizingMaskIntoConstraints = false
        bar.addSubview(row)
        NSLayoutConstraint.activate([row.leadingAnchor.constraint(equalTo: bar.leadingAnchor, constant: 16), row.trailingAnchor.constraint(equalTo: bar.trailingAnchor, constant: -8), row.centerYAnchor.constraint(equalTo: bar.centerYAnchor)])
        item = overlay.add(bar, placement: placement)
    }
    #endif

    var isVisible: Bool {
        get { item?.isVisible ?? false }
        set { item?.isVisible = newValue }
    }

    func place(_ placement: HaylenPlacement) {
        item?.update(placement)
    }

    func remove() {
        item?.remove()
        item = nil
    }

    @objc private func tap() {
        tapped()
    }
}
