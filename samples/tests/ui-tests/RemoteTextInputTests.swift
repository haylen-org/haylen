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
        waitFor(app.keyboards.firstMatch)
        app.typeText("text fields")
        RunLoop.current.run(until: Date().addingTimeInterval(1))
        try saveScreenshot("keyboard")
        remote.press(.menu)
        waitFor(element(matching: "GUI-005\\s+Text fields.*"))
        try saveScreenshot("search-results")
    }
}
#endif
