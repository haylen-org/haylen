import CoreLocation
import XCTest

// The base of the UI tests of a Haylen app. The command "haylen test --ui" installs the app on a simulator or a device and names it to the runner, and each test starts it again with the launch arguments of its class. The GUIs of the app expose their components to the tests through the accessibility tree, by the text they show.
@MainActor
class HaylenUITestCase: XCTestCase {
    // The Info.plist of the runner names the app it drives and the folder of its screenshots, which "haylen test --ui" writes into it.
    static let info = Bundle(for: HaylenUITestCase.self).infoDictionary ?? [:]
    static let timeout: TimeInterval = 60

    var identifier: String!
    var app: XCUIApplication!

    // The launch arguments of the app, which it reads through "haylen.launchArguments". A test class overrides it to start the app somewhere, such as on one screen.
    var launchArguments: [String] {
        []
    }

#if !os(visionOS)
    // The resources whose authorization the app loses before it starts, so their prompts show again whatever a test before answered. The system resets the authorization of notifications only when the app installs again.
    var resetResources: [XCUIProtectedResource] {
        []
    }

    var simulatesLocation = false
#endif

    // The tests drive the app on the main actor, as XCUIApplication requires. XCTest declares the synchronous setUp and tearDown without an actor, so the class overrides their async variants, which run on the main actor.
    override func setUp() async throws {
        continueAfterFailure = false
        identifier = try XCTUnwrap(Self.info["HaylenUIApp"] as? String, "Build the UI tests with \"haylen test --ui\", which names the app they drive.")
        app = XCUIApplication(bundleIdentifier: identifier)
#if !os(visionOS)
        for resource in resetResources {
            app.resetAuthorizationStatus(for: resource)
        }
#endif
        app.launchArguments = launchArguments.isEmpty ? [] : ["--"] + launchArguments
#if os(tvOS)
        // An Apple TV that sleeps launches no app, and a press of its remote wakes it, as the press of a person does. The press is an arrow, which moves only the focus of the screen in front, since tvOS acts on the Home button a moment later, when it would send the app that just launched to the background.
        XCUIRemote.shared.press(.down)
#endif
        app.launch()
        XCTAssertTrue(app.wait(for: .runningForeground, timeout: Self.timeout), "The app does not come to the front after its launch.")
    }

    override func tearDown() async throws {
        app.terminate()
#if !os(visionOS)
        if simulatesLocation {
            XCUIDevice.shared.location = nil
        }
#endif
    }

    // The first element of the app whose label or identifier is the text.
    func element(_ label: String) -> XCUIElement {
        app.descendants(matching: .any).matching(NSPredicate(format: "label == %@ OR identifier == %@", label, label)).firstMatch
    }

    // The first element of the app whose label starts with the text, such as a line of a log that ends with a value the test does not know.
    func element(startingWith prefix: String) -> XCUIElement {
        app.descendants(matching: .any).matching(NSPredicate(format: "label BEGINSWITH %@", prefix)).firstMatch
    }

    // The first element of the app whose label matches the regular expression.
    func element(matching pattern: String) -> XCUIElement {
        app.descendants(matching: .any).matching(NSPredicate(format: "label MATCHES %@", pattern)).firstMatch
    }

    @discardableResult
    func waitFor(_ element: XCUIElement, timeout: TimeInterval = HaylenUITestCase.timeout, file: StaticString = #filePath, line: UInt = #line) -> XCUIElement {
        XCTAssertTrue(element.waitForExistence(timeout: timeout), "The app shows no \(element.description) after \(Int(timeout)) seconds.", file: file, line: line)
        return element
    }

#if !os(tvOS)
    func tap(_ label: String, file: StaticString = #filePath, line: UInt = #line) {
        waitFor(element(label), file: file, line: line).tap()
    }
#endif

    // The arrows of the remote of a TV that move the focus of a GUI.
    enum Arrow {
        case up, down, left, right
    }

    // Presses a component of a GUI of the engine the way the person does: a tap, or on the Apple TV the arrows of the remote and then its select button. The focus of a GUI belongs to the engine, which XCUITest does not read, so a test names the arrows that take the focus from the component that holds it to this one, which the other platforms ignore.
    func activate(_ label: String, after arrows: [Arrow] = [], file: StaticString = #filePath, line: UInt = #line) {
        waitFor(element(label), file: file, line: line)
#if os(tvOS)
        let remote = XCUIRemote.shared
        for arrow in arrows {
            remote.press(Self.button(for: arrow))
            settle()
        }
        remote.press(.select)
#else
        element(label).tap()
#endif
    }

    // Presses a native view the way the person does: a tap, or on the Apple TV the select button of the remote once the focus of UIKit reached the view.
    func activate(_ view: XCUIElement, file: StaticString = #filePath, line: UInt = #line) {
        waitFor(view, file: file, line: line)
#if os(tvOS)
        focus(view, in: app, file: file, line: line)
        XCUIRemote.shared.press(.select)
#else
        view.tap()
#endif
    }

#if os(tvOS)
    // The time a GUI takes to move its focus after a press of the remote, a few of its frames, since nothing that XCUITest reads shows that it moved.
    static let focusMoveTime: TimeInterval = 0.5

    // The most presses of the arrows that the focus of UIKit takes to reach a view, and the longest wait for it to move after one press.
    static let focusMoves = 24
    static let focusMoveTimeout: TimeInterval = 2

    static func button(for arrow: Arrow) -> XCUIRemote.Button {
        switch arrow {
        case .up:
            return .up
        case .down:
            return .down
        case .left:
            return .left
        case .right:
            return .right
        }
    }

    func settle() {
        RunLoop.current.run(until: Date().addingTimeInterval(Self.focusMoveTime))
    }

    // Moves the focus of UIKit onto a native view of the app or of the system UI over it, pressing the arrow that points from the view that holds the focus toward it.
    func focus(_ target: XCUIElement, in root: XCUIApplication, file: StaticString = #filePath, line: UInt = #line) {
        let remote = XCUIRemote.shared
        for _ in 0..<Self.focusMoves where !target.hasFocus {
            let from = focusedFrame(in: root)
            let dx = target.frame.midX - from.midX
            let dy = target.frame.midY - from.midY
            let direction: XCUIRemote.Button = abs(dx) > abs(dy) ? (dx > 0 ? .right : .left) : (dy > 0 ? .down : .up)
            remote.press(direction)
            let moved = XCTNSPredicateExpectation(predicate: NSPredicate { _, _ in target.hasFocus || self.focusedFrame(in: root) != from }, object: nil)
            _ = XCTWaiter.wait(for: [moved], timeout: Self.focusMoveTimeout)
        }
        XCTAssertTrue(target.hasFocus, "The focus does not reach \(target.description).", file: file, line: line)
    }

    // The frame of the view of the app or of the system UI that holds the focus of UIKit, or an empty frame while none does.
    func focusedFrame(in root: XCUIApplication) -> CGRect {
        let focused = root.descendants(matching: .any).matching(NSPredicate(format: "hasFocus == true")).firstMatch
        return focused.exists ? focused.frame : .zero
    }
#endif

#if !os(visionOS)
    // The process of the system UI, which shows the alerts and the notifications of the system over the app.
#if os(tvOS)
    static let system = XCUIApplication(bundleIdentifier: "com.apple.PineBoard")
#else
    static let system = XCUIApplication(bundleIdentifier: "com.apple.springboard")
#endif

    // An answer to the prompt of a permission. The device writes the buttons of a prompt in its own language and always in the same order, so an answer is the place of its button among the choices of its prompt: a prompt of access offers "Don't Allow" and "Allow", the prompt of the location "Allow Once", "Allow While Using App" and "Don't Allow", and the prompt that offers the location always "Keep Only While Using" and "Change to Always Allow".
    enum Answer: String {
        case allow = "Allow"
        case deny = "Don't Allow"
        case allowWhileUsing = "Allow While Using App"

        var choices: Int {
            self == .allowWhileUsing ? 3 : 2
        }

        var place: Int {
            switch self {
            case .allow, .allowWhileUsing:
                return 1
            case .deny:
                return 0
            }
        }
    }

    // Makes the simulator or the device report the place until the test ends. A device keeps a simulated place after the run otherwise, with the time zone that the place sets.
    func simulateLocation(_ place: CLLocation) {
        XCUIDevice.shared.location = XCUILocation(location: place)
        simulatesLocation = true
    }

    // Presses the button of the answer on the alert of the system that shows over the app, such as the prompt of a permission, and fails when no such alert shows.
    func answerAlert(_ answer: Answer, timeout: TimeInterval = HaylenUITestCase.timeout, file: StaticString = #filePath, line: UInt = #line) {
        let alert = Self.system.alerts.firstMatch
        waitFor(alert, timeout: timeout, file: file, line: line)

        // The buttons of the choices come first, and tvOS nests a copy of each one in it and adds a button that reads the prompt after them.
        var labels: [String] = []
        for label in alert.buttons.allElementsBoundByIndex.map(\.label) where !labels.contains(label) {
            labels.append(label)
        }
        XCTAssertGreaterThanOrEqual(labels.count, answer.choices, "The alert \"\(alert.label)\" offers \(labels), fewer than the \(answer.choices) choices of a prompt that offers \"\(answer.rawValue)\".", file: file, line: line)
        let button = alert.buttons[labels[answer.place]].firstMatch
#if os(tvOS)
        focus(button, in: Self.system, file: file, line: line)
        XCUIRemote.shared.press(.select)
#else
        button.tap()
#endif
        XCTAssertTrue(alert.waitForNonExistence(timeout: timeout), "The alert \"\(alert.label)\" stays after the answer.", file: file, line: line)
    }
#endif

    // Waits until the condition holds, checking it twice a second, and fails with the message when it never does.
    func waitUntil(_ message: @autoclosure () -> String, timeout: TimeInterval = HaylenUITestCase.timeout, file: StaticString = #filePath, line: UInt = #line, _ condition: () -> Bool) {
        let deadline = Date().addingTimeInterval(timeout)
        while !condition() {
            if Date() > deadline {
                XCTFail(message(), file: file, line: line)
                return
            }

            RunLoop.current.run(until: Date().addingTimeInterval(0.5))
        }
    }

    // Saves a picture of the window of the app as a PNG named after the text in the folder of the screenshots of the run, which on a device is relative to the container of the runner.
    func saveScreenshot(_ name: String) throws {
        let path = try XCTUnwrap(Self.info["HaylenUIOutput"] as? String, "Build the UI tests with \"haylen test --ui\", which names the folder of their screenshots.")
        let folder = URL(fileURLWithPath: path, relativeTo: URL(fileURLWithPath: NSHomeDirectory(), isDirectory: true))
        try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        let window = app.windows.firstMatch
        let screenshot = window.exists ? window.screenshot() : app.screenshot()
        try screenshot.pngRepresentation.write(to: folder.appendingPathComponent("\(name).png"))
    }

#if os(iOS)
    // Taps the notification banner whose text holds the words, over the app, the Home Screen or another app, which brings the app to the front or launches it.
    func tapNotification(containing text: String, timeout: TimeInterval = HaylenUITestCase.timeout, file: StaticString = #filePath, line: UInt = #line) {
        let banner = Self.system.descendants(matching: .any).matching(NSPredicate(format: "label CONTAINS %@", text)).firstMatch
        waitFor(banner, timeout: timeout, file: file, line: line).tap()
        XCTAssertTrue(app.wait(for: .runningForeground, timeout: timeout), "The tap on the notification does not bring the app to the front.", file: file, line: line)
    }

    // Leaves the app for the Home Screen, which sends it to the background, where it runs or the system suspends it.
    func goHome(file: StaticString = #filePath, line: UInt = #line) {
        XCUIDevice.shared.press(.home)
        waitUntil("The app does not leave the front for the Home Screen.", file: file, line: line) {
            app.state != .runningForeground
        }
    }

    // The ways iPadOS shows the windows of apps, as the multitasking settings of the system name them.
    enum Multitasking: String, CaseIterable {
        case fullScreenApps = "Full Screen Apps"
        case windowedApps = "Windowed Apps"
        case stageManager = "Stage Manager"
    }

    static let settings = XCUIApplication(bundleIdentifier: "com.apple.Preferences")

    var isPad: Bool {
        UIDevice.current.userInterfaceIdiom == .pad
    }

    var windowFrame: CGRect {
        app.windows.firstMatch.frame
    }

    // Turns the device and waits until the window of the app turns with it and its rotation ends.
    func turnDevice(_ orientation: UIDeviceOrientation, file: StaticString = #filePath, line: UInt = #line) {
        XCUIDevice.shared.orientation = orientation
        let landscape = orientation.isLandscape
        waitUntil("The window of the app does not turn to \(landscape ? "landscape" : "portrait").", file: file, line: line) {
            let frame = windowFrame
            return (frame.width > frame.height) == landscape
        }
        RunLoop.current.run(until: Date().addingTimeInterval(1))
    }

    // Chooses how iPadOS shows windows in the settings of the system, returns the way it showed them before and brings the app back to the front.
    @discardableResult
    func setMultitasking(_ mode: Multitasking) -> Multitasking {
        let settings = Self.settings
        settings.launch()
        let section = settings.buttons["com.apple.settings.multitaskingAndGestures"]
        waitFor(section).tap()
        let options = Multitasking.allCases.map { settings.staticTexts[$0.rawValue] }
        waitFor(options[0])
        // The mark of the chosen way sits just before its name.
        let mark = settings.images["checkmark.circle.fill"].frame
        let previous = Multitasking.allCases[options.indices.min { abs(options[$0].frame.minX - mark.maxX) < abs(options[$1].frame.minX - mark.maxX) }!]
        options[Multitasking.allCases.firstIndex(of: mode)!].tap()
        settings.terminate()
        app.activate()
        return previous
    }

    // Picks one of the arrangements that the window controls of iPadOS offer from a long press on their zoom button, such as "Left", "Right" or "Fill".
    func arrangeWindow(_ arrangement: String) {
        let system = Self.system
        waitFor(system.descendants(matching: .any)["window-controls:\(identifier!)"].firstMatch).tap()
        waitFor(system.buttons["Zoom-button"]).press(forDuration: 1)
        waitFor(system.buttons[arrangement]).tap()
    }

    // Drags the bottom right corner of the window of the app by the distance in points, which resizes the window.
    func resizeWindow(by distance: CGVector) {
        let frame = windowFrame
        let screen = Self.system.coordinate(withNormalizedOffset: .zero)
        let corner = screen.withOffset(CGVector(dx: frame.maxX - 4, dy: frame.maxY - 4))
        corner.press(forDuration: 0.5, thenDragTo: corner.withOffset(distance))
    }
#endif
}
