import XCTest

#if os(iOS)
// The list of the test "Long lists" holds a hundred thousand entries, so it goes past its area on every screen, and a finger that drags it from outside the number field scrolls it and leaves the field editing with the keyboard.
class LongListTests: TestProjectCase {
    override var code: String {
        "GUI-022"
    }

    func testAFingerThatScrollsTheListKeepsTheFieldEditing() throws {
        let field = element("Entry to scroll to")
        waitFor(field).tap()
        waitFor(app.keyboards.firstMatch)
        let entry = waitFor(element("Island 1"))
        let top = entry.frame.minY
        let start = entry.coordinate(withNormalizedOffset: CGVector(dx: 0.5, dy: 0.5))
        start.press(forDuration: 0.1, thenDragTo: start.withOffset(CGVector(dx: 0, dy: -120)))
        RunLoop.current.run(until: Date().addingTimeInterval(1))
        XCTAssertTrue(app.keyboards.firstMatch.exists, "The keyboard closes when a finger scrolls the list from outside the field.")
        XCTAssertTrue(!entry.exists || entry.frame.minY < top - 1, "The list does not scroll under the finger.")
        try saveScreenshot("list-scrolled-while-editing")
    }
}
#endif
