import Foundation

#if os(macOS)
import AppKit
#else
import UIKit
#endif

// The banner of the {{TITLE}} plugin on Apple platforms: a native bar with a text and a button, which the overlay of the plugin places over the app at its top or bottom edge, and whose button sends the event `bannerTapped`. The overlay passes every touch and click outside the bar to the app.
@MainActor
final class {{NAME}}Banner: NSObject {
    struct Request: Decodable {
        let text: String
        let anchor: String
        let reserve: Bool
    }

    struct Empty: Codable {}

    private var context: HaylenPluginContext?
    private var item: HaylenOverlay.Item?
    private var taps = 0

    func register(_ context: HaylenPluginContext) {
        self.context = context
        context.register("showBanner") { [unowned self] (params: Request) async throws -> Empty in
            guard let anchor = ["top": HaylenPlacement.Anchor.top, "bottom": .bottom][params.anchor] else {
                throw HaylenFailure("The anchor of the banner is \"top\" or \"bottom\", not \"\(params.anchor)\".", code: "invalidAnchor")
            }
            let placement = HaylenPlacement(anchor: anchor)
            placement.reserve = params.reserve
            placement.width = 360
            placement.height = 56
            self.item?.remove()
            self.item = context.overlay.add(self.makeBar(text: params.text), placement: placement)
            return Empty()
        }

        context.register("hideBanner") { [unowned self] (_: Empty) async throws -> Empty in
            self.item?.remove()
            self.item = nil
            return Empty()
        }
    }

    #if os(macOS)
    private func makeBar(text: String) -> NSView {
        let bar = NSView()
        bar.wantsLayer = true
        bar.layer?.backgroundColor = NSColor.systemIndigo.cgColor
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
        return bar
    }
    #else
    private func makeBar(text: String) -> UIView {
        let bar = UIView()
        bar.backgroundColor = .systemIndigo
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
        return bar
    }
    #endif

    @objc private func tap() {
        taps += 1
        context?.emit("bannerTapped", payload: ["count": taps])
    }
}
