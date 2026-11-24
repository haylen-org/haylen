import XCTest

// The start of a run: the menu, the choice of a survivor and the island with the heads-up display, from taps, or on the Apple TV from the select button of the remote on the buttons that each screen focuses.
class StartTests: HaylenUITestCase {
    func testPlayStartsARunOnTheIsland() throws {
        activate("Play")
        activate("Start")
        waitFor(element(matching: "\\d+ / \\d+"))
        try saveScreenshot("island")
    }
}
