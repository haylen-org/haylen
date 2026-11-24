import Foundation

#if os(iOS) && !targetEnvironment(macCatalyst)
import GameController
import UIKit
#endif

// The virtual controller of GameController on an iPhone or an iPad, with a left stick and the south button, which the app reads as one more gamepad. The plugin draws the stick and the button over the app and hands their touches to the controller, as GameController asks of an app that draws its own controls, so the UI tests of the test project find them by their identifiers and drive a gamepad on a simulator. The system lets an app connect one virtual controller at a time, and Mac Catalyst, tvOS, visionOS and the Mac have none.
@MainActor
final class NativeDemoGamepad {
    struct Shown: Encodable {
        let name: String
        let elements: [String]
        let language: String
    }

    static let unsupported = "Only iPhones and iPads with iOS 17 or later connect the virtual controller of GameController."

    #if os(iOS) && !targetEnvironment(macCatalyst)
    private var controller: GCVirtualController?
    private var items: [HaylenOverlay.Item] = []
    #endif

    func register(_ context: HaylenPluginContext, language: String) {
        context.register("showVirtualController") { [unowned self] (_: NativeDemoPlugin.Empty) async throws -> Shown in
            try await self.show(overlay: context.overlay, language: language)
        }

        context.register("hideVirtualController") { [unowned self] (_: NativeDemoPlugin.Empty) async throws -> NativeDemoPlugin.Empty in
            self.hide()
            return NativeDemoPlugin.Empty()
        }
    }

    func hide() {
        #if os(iOS) && !targetEnvironment(macCatalyst)
        for item in items {
            item.remove()
        }
        items = []
        controller?.disconnect()
        controller = nil
        #endif
    }

    private func show(overlay: HaylenOverlay, language: String) async throws -> Shown {
        #if os(iOS) && !targetEnvironment(macCatalyst)
        guard #available(iOS 17.0, *) else {
            throw HaylenFailure(Self.unsupported, code: "unsupported")
        }

        hide()
        let elements = [GCInputLeftThumbstick, GCInputButtonA]
        let configuration = GCVirtualController.Configuration()
        configuration.elements = Set(elements)
        configuration.isHidden = true
        let virtual = GCVirtualController(configuration: configuration)
        try await virtual.connect()
        controller = virtual

        let stick = Stick { position in virtual.setPosition(position, forDirectionPadElement: GCInputLeftThumbstick) }
        let button = Button { value in virtual.setValue(value, forButtonElement: GCInputButtonA) }
        items = [overlay.add(stick, placement: Self.placement(.left, size: Stick.size)), overlay.add(button, placement: Self.placement(.right, size: Button.size))]
        return Shown(name: virtual.controller?.vendorName ?? "Virtual controller", elements: elements, language: language)
        #else
        throw HaylenFailure(Self.unsupported, code: "unsupported")
        #endif
    }

    #if os(iOS) && !targetEnvironment(macCatalyst)
    private static func placement(_ anchor: HaylenPlacement.Anchor, size: CGFloat) -> HaylenPlacement {
        let placement = HaylenPlacement(anchor: anchor)
        placement.margin = 24
        placement.width = size
        placement.height = size
        return placement
    }

    // The stick: a finger that presses it leans the stick toward the finger, as far as the finger is from the middle over the radius of the stick, and the stick rests once the finger lifts. GameController counts up as positive.
    private final class Stick: UIView {
        static let size: CGFloat = 180

        private let lean: (CGPoint) -> Void

        init(lean: @escaping (CGPoint) -> Void) {
            self.lean = lean
            super.init(frame: .zero)
            backgroundColor = UIColor(white: 1, alpha: 0.18)
            layer.cornerRadius = Self.size / 2
            isAccessibilityElement = true
            accessibilityIdentifier = "virtual-stick"
            accessibilityLabel = "Virtual stick"
        }

        required init?(coder: NSCoder) {
            fatalError("The stick of the demo is made in code alone.")
        }

        override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
            follow(touches)
        }

        override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
            follow(touches)
        }

        override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
            lean(.zero)
        }

        override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
            lean(.zero)
        }

        private func follow(_ touches: Set<UITouch>) {
            guard let touch = touches.first else {
                return
            }

            let point = touch.location(in: self)
            let radius = bounds.width / 2
            let x = (point.x - bounds.midX) / radius
            let y = (bounds.midY - point.y) / radius
            let length = max(1, (x * x + y * y).squareRoot())
            lean(CGPoint(x: x / length, y: y / length))
        }
    }

    // The south button, pressed while a finger holds it.
    private final class Button: UIView {
        static let size: CGFloat = 96

        private let press: (CGFloat) -> Void

        init(press: @escaping (CGFloat) -> Void) {
            self.press = press
            super.init(frame: .zero)
            backgroundColor = UIColor(white: 1, alpha: 0.18)
            layer.cornerRadius = Self.size / 2
            isAccessibilityElement = true
            accessibilityIdentifier = "virtual-south"
            accessibilityLabel = "Virtual south button"
        }

        required init?(coder: NSCoder) {
            fatalError("The button of the demo is made in code alone.")
        }

        override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
            press(1)
        }

        override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
            press(0)
        }

        override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
            press(0)
        }
    }
    #endif
}
