import XCTest

#if os(iOS)
// The address field of the test "Text fields", which capitalizes nothing and sits right above the keyboard, types through the software keyboard: each key types once, Backspace deletes, the screen holds still while the keyboard shows, and the text and the keyboard stay when the device turns.
class TextInputTests: HaylenUITestCase {
    override func setUpWithError() throws {
        try super.setUpWithError()
        // The search of the menu finds the test, and typing its words takes the keyboard too.
        waitFor(element(matching: "Search by code or title.*")).tap()
        waitFor(app.keyboards.firstMatch)
        press("text fields")
        waitFor(element(matching: "GUI-005\\s+Text fields.*")).tap()
        waitFor(element("https://haylen.dev"))
    }

    override func tearDown() {
        XCUIDevice.shared.orientation = .landscapeLeft
        super.tearDown()
    }

    func testEachKeyTypesOnceAndBackspaceDeletes() throws {
        startTyping()
        press("oi")
        expectReport("oi")
        waitFor(app.keyboards.keys["delete"]).tap()
        expectReport("o")
        press("la")
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
        press("mar")
        expectReport("mar")
        XCUIDevice.shared.orientation = .landscapeRight
        RunLoop.current.run(until: Date().addingTimeInterval(1.5))
        XCTAssertTrue(app.keyboards.firstMatch.exists, "The keyboard closes when the device turns.")
        press("e")
        expectReport("mare")
        try saveScreenshot("text-turned")
    }

    func startTyping() {
        tap("https://haylen.dev")
        waitFor(app.keyboards.firstMatch)
    }

    // Taps the keys of the software keyboard one by one, as a person types.
    func press(_ text: String) {
        for character in text {
            waitFor(app.keyboards.keys[character == " " ? "space" : String(character)]).tap()
        }
    }

    // The test reports every change of a field on its status line.
    func expectReport(_ text: String, file: StaticString = #filePath, line: UInt = #line) {
        waitFor(element("The field \"url\" reported \"change\" with \"\(text)\"."), timeout: 10, file: file, line: line)
    }
}
#endif
