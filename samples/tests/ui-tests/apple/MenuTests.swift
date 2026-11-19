import XCTest

#if !os(tvOS)
// The menu of the test project answers a tap, which the visionOS simulator delivers as a look and a pinch.
class MenuTests: HaylenUITestCase {
    // The test opened before the menu, whose category is the last one, so the menu comes back scrolled to it and the first categories lie out of sight above the list on a short screen.
    override var launchArguments: [String] {
        ["--test", "VRN-008"]
    }

    // The most drags that bring the first category back into view, and how far each drags the list down.
    static let drags = 8
    static let dragDistance: CGFloat = 200

    // The element of a category covers only what the list shows of it, so it never lies on the search field above the list, and a tap lands on the category once a finger that drags the list down brings it into view.
    func testATapOnACategoryOpensItsTests() throws {
        waitFor(element("VRN-008"))
        tap("Back")
        XCTAssertTrue(element("VRN-008").waitForNonExistence(timeout: Self.timeout), "The test stays after its Back button.")
        tap("Back")

        let search = waitFor(element(matching: "Search by code or title.*"))
        let sprites = waitFor(element("Sprites"))
        XCTAssertFalse(sprites.frame.intersects(search.frame), "The category \"Sprites\" at \(sprites.frame) lies on the search field at \(search.frame).")
        let list = search.coordinate(withNormalizedOffset: CGVector(dx: 0.5, dy: 1)).withOffset(CGVector(dx: 0, dy: 40))
        for _ in 0..<Self.drags where !sprites.isHittable {
            list.press(forDuration: 0.1, thenDragTo: list.withOffset(CGVector(dx: 0, dy: Self.dragDistance)))
        }

        XCTAssertTrue(sprites.isHittable, "The category \"Sprites\" at \(sprites.frame) does not come into view.")
        XCTAssertGreaterThanOrEqual(sprites.frame.minY, search.frame.maxY, "The category \"Sprites\" at \(sprites.frame) lies above the list, under the search field at \(search.frame).")
        sprites.tap()
        waitFor(element(matching: "SPR-001\\s+Sprite basics"))
        try saveScreenshot("sprites-category")
    }
}
#endif
