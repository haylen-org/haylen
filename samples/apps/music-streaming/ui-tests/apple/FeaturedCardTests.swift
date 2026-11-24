import XCTest

#if os(iOS)
// The card of the mix of the day on an iPad mini in portrait, whose sidebar leaves the card too little room for the illustration beside its texts: the column of the texts keeps the width it needs to read, and the illustration goes down to a line of its own. The illustration is decoration that screen readers skip, so the test measures the texts around it.
class FeaturedCardTests: HaylenUITestCase {
    // The size of a UI unit of the app, which draws in the scale mode "physical", where a unit is three quarters of a point.
    static let pointsPerUnit: CGFloat = 0.75
    // The width that the column of the texts keeps, "Style.size.featuredText", and the height of the illustration, in UI units.
    static let textWidth: CGFloat = 280
    static let illustrationHeight: CGFloat = 210

    override func tearDown() async throws {
        XCUIDevice.shared.orientation = .portrait
        try await super.tearDown()
    }

    func testTheTextsKeepTheirWidthAndTheIllustrationTakesALineOfItsOwn() throws {
        try XCTSkipUnless(UIDevice.current.name.hasPrefix("iPad mini"), "The card fits its illustration beside its texts on wider screens, so this check runs on an iPad mini.")
        turnDevice(.portrait)

        // The description shows only in the wide layout of the app, and it wraps inside the column of the texts, so its width follows the width of the column.
        let description = waitFor(element(matching: "The .* you play most, and more like it\\."))
        let shelf = waitFor(element("Your mixes"))
        let play = app.buttons.matching(NSPredicate(format: "label == %@", "Play")).allElementsBoundByIndex.first { button in
            button.frame.minY >= description.frame.maxY && button.frame.maxY <= shelf.frame.minY
        }
        let button = try XCTUnwrap(play, "The card shows no Play button under the description of the mix.")

        let textWidth = description.frame.width / Self.pointsPerUnit
        XCTAssertGreaterThanOrEqual(textWidth, Self.textWidth, "The column of the texts is narrower than \(Self.textWidth) units: the description takes \(textWidth) units.")
        let below = (shelf.frame.minY - button.frame.maxY) / Self.pointsPerUnit
        XCTAssertGreaterThanOrEqual(below, Self.illustrationHeight, "The illustration does not take a line of its own under the texts: only \(below) units lie between the Play button and the next shelf.")
        try saveScreenshot("featured-card")
    }
}
#endif
