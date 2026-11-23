import XCTest

#if os(iOS)
// The search of the menu of the test project, which the tests that start from the menu type into and open a test from, as a person does.
extension HaylenUITestCase {
    var menuSearch: XCUIElement {
        element(matching: "Search by code or title.*")
    }

    // Ends the editing of the search with a tap on the caption of the menu, which is no control, waits until the keyboard is gone and only then opens the found test whose button matches the pattern. XCUITest never sees the keyboard's animations go idle when a field takes the keyboard back while it is still hiding.
    func openFoundTest(_ pattern: String, file: StaticString = #filePath, line: UInt = #line) {
        let found = waitFor(element(matching: pattern), file: file, line: line)
        let caption = app.descendants(matching: .any).matching(NSPredicate(format: "label CONTAINS %@", "Pick a category, or search a test")).firstMatch
        waitFor(caption, file: file, line: line).tap()
        waitUntil("The keyboard stays open after a tap outside the search of the menu.", file: file, line: line) { !app.keyboards.firstMatch.exists }
        found.tap()
    }
}
#endif
