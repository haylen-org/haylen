import XCTest

// The base of the UI tests of a Haylen app. The command "haylen test --ui" installs the app on a simulator and names it to the runner, and each test starts it again. The GUIs of the app expose their components to the tests through the accessibility tree, by the text they show.
class HaylenUITestCase: XCTestCase {
    static let environment = ProcessInfo.processInfo.environment
    static let timeout: TimeInterval = 60

    var identifier: String!
    var app: XCUIApplication!

    override func setUpWithError() throws {
        continueAfterFailure = false
        identifier = try XCTUnwrap(Self.environment["HAYLEN_UI_APP"], "Run the UI tests with \"haylen test --ui\", which names the app they drive.")
        app = XCUIApplication(bundleIdentifier: identifier)
        app.launch()
    }

    override func tearDown() {
        app.terminate()
    }

    // The first element of the app whose label or identifier is the text.
    func element(_ label: String) -> XCUIElement {
        app.descendants(matching: .any).matching(NSPredicate(format: "label == %@ OR identifier == %@", label, label)).firstMatch
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

    func tap(_ label: String, file: StaticString = #filePath, line: UInt = #line) {
        waitFor(element(label), file: file, line: line).tap()
    }

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

    // Saves a picture of the window of the app as a PNG named after the text in the folder of the screenshots of the run.
    func saveScreenshot(_ name: String) throws {
        let folder = try XCTUnwrap(Self.environment["HAYLEN_UI_OUTPUT"], "Run the UI tests with \"haylen test --ui\", which names the folder of their screenshots.")
        let window = app.windows.firstMatch
        let screenshot = window.exists ? window.screenshot() : app.screenshot()
        try screenshot.pngRepresentation.write(to: URL(fileURLWithPath: folder).appendingPathComponent("\(name).png"))
    }

#if os(iOS)
    // The ways iPadOS shows the windows of apps, as the multitasking settings of the system name them.
    enum Multitasking: String, CaseIterable {
        case fullScreenApps = "Full Screen Apps"
        case windowedApps = "Windowed Apps"
        case stageManager = "Stage Manager"
    }

    static let springboard = XCUIApplication(bundleIdentifier: "com.apple.springboard")
    static let settings = XCUIApplication(bundleIdentifier: "com.apple.Preferences")

    var isPad: Bool {
        UIDevice.current.userInterfaceIdiom == .pad
    }

    var windowFrame: CGRect {
        app.windows.firstMatch.frame
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
        let springboard = Self.springboard
        waitFor(springboard.descendants(matching: .any)["window-controls:\(identifier!)"].firstMatch).tap()
        waitFor(springboard.buttons["Zoom-button"]).press(forDuration: 1)
        waitFor(springboard.buttons[arrangement]).tap()
    }

    // Drags the bottom right corner of the window of the app by the distance in points, which resizes the window.
    func resizeWindow(by distance: CGVector) {
        let frame = windowFrame
        let screen = Self.springboard.coordinate(withNormalizedOffset: .zero)
        let corner = screen.withOffset(CGVector(dx: frame.maxX - 4, dy: frame.maxY - 4))
        corner.press(forDuration: 0.5, thenDragTo: corner.withOffset(distance))
    }
#endif
}
