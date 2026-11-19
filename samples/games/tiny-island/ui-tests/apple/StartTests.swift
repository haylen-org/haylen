import XCTest

#if !os(tvOS)
// The start of a run from taps: the menu, the choice of a survivor and the island with the heads-up display.
class StartTests: HaylenUITestCase {
    func testPlayStartsARunOnTheIsland() throws {
        tap("Play")
        tap("Start")
        waitFor(element(matching: "\\d+ / \\d+"))
        try saveScreenshot("island")
    }
}
#endif
