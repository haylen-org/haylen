import XCTest

#if os(iOS) && !targetEnvironment(macCatalyst)
// PLT-009: the scoreboard opens on the external display of the device, which UIKit connects as a scene of the app only because "window.externalDisplays" of the test project registers the accessory of external displays. A simulator shows its external display once "xcrun simctl io <device> screenConfig --display=external power on" turns it on, and without one the test is skipped.
class SecondDisplayTests: TestProjectCase {
    override var code: String {
        "PLT-009"
    }

    func testTheScoreboardOpensOnTheExternalDisplay() throws {
        tap("Open on the next display")
        let opening = element(matching: "Opening on .*")
        let alone = element(startingWith: "Only the display of the window is connected.")
        waitUntil("The app neither opens the scoreboard nor tells that no other display is connected.") {
            opening.exists || alone.exists
        }
        try XCTSkipIf(alone.exists, "The device has no external display. Turn the one of a simulator on with \"xcrun simctl io <device> screenConfig --display=external power on\".")

        waitFor(element(matching: "The output is shown, \\d+ by \\d+ pixels at [0-9.]+x\\."))
        try saveScreenshot("second-display")
    }
}
#endif
