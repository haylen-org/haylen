import XCTest

#if os(iOS)
// The address field of the test "Text fields", which capitalizes nothing and sits right above the keyboard, types through the software keyboard: each key types once, Backspace deletes, the screen holds still while the keyboard shows, the text and the keyboard stay when the device turns, the end of the page scrolls above the keyboard, and the keyboard follows the field that edits.
class TextInputTests: HaylenUITestCase {
    override func setUp() async throws {
        try await super.setUp()
        // The search of the menu finds the test, and typing its words takes the keyboard too.
        waitFor(element(matching: "Search by code or title.*")).tap()
        waitFor(app.keyboards.firstMatch)
        typeOnKeyboard("text fields")
        waitFor(element(matching: "GUI-005\\s+Text fields.*")).tap()
        waitFor(element("https://haylen.dev"))
    }

    override func tearDown() async throws {
        XCUIDevice.shared.orientation = .landscapeLeft
        try await super.tearDown()
    }

    func testEachKeyTypesOnceAndBackspaceDeletes() throws {
        startTyping()
        typeOnKeyboard("oi")
        expectReport("oi")
        waitFor(app.keyboards.keys["delete"]).tap()
        expectReport("o")
        typeOnKeyboard("la")
        expectReport("ola")
        try saveScreenshot("text-typed")
    }

    // The keyboard lifts the field once and the page then holds still, so the field keeps its place frame after frame.
    func testTheScreenHoldsStillWhileTheKeyboardShows() throws {
        startTyping()
        let field = element("https://haylen.dev")
        RunLoop.current.run(until: Date().addingTimeInterval(1.5))
        var places: [CGRect] = []
        for _ in 0..<12 {
            places.append(field.frame)
            RunLoop.current.run(until: Date().addingTimeInterval(0.1))
        }
        XCTAssertEqual(Set(places.map { NSCoder.string(for: $0) }).count, 1, "The field moves while the keyboard shows: \(places).")
        try saveScreenshot("keyboard-still")
    }

    func testTheTextAndTheKeyboardStayWhenTheDeviceTurns() throws {
        startTyping()
        typeOnKeyboard("mar")
        expectReport("mar")
        XCUIDevice.shared.orientation = .landscapeRight
        RunLoop.current.run(until: Date().addingTimeInterval(1.5))
        XCTAssertTrue(app.keyboards.firstMatch.exists, "The keyboard closes when the device turns.")
        typeOnKeyboard("e")
        expectReport("mare")
        try saveScreenshot("text-turned")
    }

    // The keyboard stays while another field takes over, comes back with the app, closes with a field that submits and closes with the screen of its field.
    func testTheKeyboardFollowsTheFieldThatEdits() throws {
        startTyping()
        tap("Crew size")
        RunLoop.current.run(until: Date().addingTimeInterval(1))
        XCTAssertTrue(app.keyboards.firstMatch.exists, "The keyboard closes when another field takes over.")
        typeOnKeyboard("42")
        waitFor(element("The field \"number\" reported \"change\" with \"42\"."), timeout: 10)

        startTyping()
        goHome()
        app.activate()
        waitFor(app.keyboards.firstMatch)
        typeOnKeyboard("ilha")
        expectReport("ilha")
        try saveScreenshot("keyboard-back")
        waitFor(app.keyboards.descendants(matching: .any).matching(NSPredicate(format: "label ==[c] %@", "go")).firstMatch).tap()
        waitUntil("The keyboard stays open after its field submitted.") { !app.keyboards.firstMatch.exists }

        startTyping()
        tap("Back")
        waitUntil("The keyboard stays open after the screen of its field closed.") { !app.keyboards.firstMatch.exists }
        try saveScreenshot("keyboard-closed")
    }

    // The part of the page that the keyboard covers joins the end of its content, so a finger scrolls the last fields of the page above the keyboard, as native apps do, on every screen size. The drags start on the page where the help line of the field shows before any drag, which the lift keeps above the keyboard and its bar, and away from the text area, which scrolls its own lines.
    func testTheEndOfThePageScrollsAboveTheKeyboard() throws {
        startTyping()
        let keyboard = app.keyboards.firstMatch
        let field = element("https://haylen.dev")
        let start = app.coordinate(withNormalizedOffset: .zero).withOffset(CGVector(dx: field.frame.midX, dy: field.frame.maxY + 14))
        for _ in 0..<4 {
            start.press(forDuration: 0.1, thenDragTo: start.withOffset(CGVector(dx: 0, dy: -240)))
        }
        RunLoop.current.run(until: Date().addingTimeInterval(1.5))
        XCTAssertTrue(keyboard.exists, "The keyboard closes when a finger scrolls the page.")
        for label in ["Password", "Filter recipes"] {
            let last = element(label)
            XCTAssertLessThanOrEqual(last.frame.maxY, keyboard.frame.minY + 1, "The field \"\(label)\" stays under the keyboard at \(last.frame).")
        }
        try saveScreenshot("end-above-keyboard")
    }

    // The plain keyboard of the test "Text input" gives the native input to the message field and takes it back once the field sends, and closes when the test hides it as it exits.
    func testThePlainKeyboardOfTheAppComesBackAfterAField() throws {
        tap("Back")
        waitFor(element(matching: "Search by code or title.*")).tap()
        waitFor(app.keyboards.firstMatch)
        for _ in 0..<"text fields".count {
            waitFor(app.keyboards.keys["delete"]).tap()
        }
        typeOnKeyboard("text input")
        waitFor(element(matching: "GUI-018\\s+Text input.*")).tap()
        tap("Show the plain keyboard")
        waitFor(app.keyboards.firstMatch)

        // The keyboard takes the traits of the field from the plain keyboard, so it capitalizes the first letter of the sentence.
        tap("Say hello to the keeper")
        typeOnKeyboard("oi")
        waitFor(app.keyboards.descendants(matching: .any).matching(NSPredicate(format: "label ==[c] %@", "send")).firstMatch).tap()
        waitFor(app.descendants(matching: .any).matching(NSPredicate(format: "label CONTAINS %@", "Submitted \"Oi\"")).firstMatch, timeout: 10)
        RunLoop.current.run(until: Date().addingTimeInterval(1))
        XCTAssertTrue(app.keyboards.firstMatch.exists, "The keyboard of the app closes after the field sends.")
        try saveScreenshot("plain-keyboard-back")

        // The keyboard covers the button that hides it, and the test hides it as it exits.
        tap("Back")
        waitUntil("The plain keyboard stays open after the app hid it.") { !app.keyboards.firstMatch.exists }
    }

    func startTyping() {
        tap("https://haylen.dev")
        waitFor(app.keyboards.firstMatch)
    }

    // The test reports every change of a field on its status line.
    func expectReport(_ text: String, file: StaticString = #filePath, line: UInt = #line) {
        waitFor(element("The field \"url\" reported \"change\" with \"\(text)\"."), timeout: 10, file: file, line: line)
    }
}
#endif
