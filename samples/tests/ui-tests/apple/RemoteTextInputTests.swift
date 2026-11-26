import XCTest

#if os(tvOS)
// The search of the menu types through the keyboard of the Apple TV, which the remote opens on the field, and finds the test it names.
class RemoteTextInputTests: HaylenUITestCase {
    func testTheKeyboardOfTheSystemTypesIntoTheSearch() throws {
        let remote = XCUIRemote.shared
        let search = element(matching: "Search by code or title.*")
        waitFor(search)
        for _ in 0..<4 where !search.hasFocus {
            remote.press(.up)
            RunLoop.current.run(until: Date().addingTimeInterval(0.5))
        }
        try saveScreenshot("search-focused")
        remote.press(.select)
        // The keyboard of the system shows before it takes keys, which reach its text line once that line holds the keyboard focus, and the test goes on once the line shows what it typed.
        let line = waitFor(app.textViews.matching(NSPredicate(format: "hasKeyboardFocus == true")).firstMatch)
        line.typeText("text fields")
        waitUntil("The text line of the keyboard of the system shows \"\(line.value ?? "")\" instead of \"text fields\".") {
            line.value as? String == "text fields"
        }
        try saveScreenshot("keyboard")
        remote.press(.menu)
        waitFor(element(matching: "GUI-005\\s+Text fields.*"))
        try saveScreenshot("search-results")
    }
}
#endif
