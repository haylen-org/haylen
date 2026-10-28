import XCTest

// The menu of the test project answers a tap, which the visionOS simulator delivers as a look and a pinch.
class MenuTests: HaylenUITestCase {
    func testATapOnACategoryOpensItsTests() throws {
        tap("Sprites")
        waitFor(element(matching: "SPR-001\\s+Sprite basics"))
        try saveScreenshot("sprites-category")
    }
}
