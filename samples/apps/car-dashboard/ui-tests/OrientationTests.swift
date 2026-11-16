import XCTest

#if os(iOS)
// The dashboard turns with the device and lays its parts out again in landscape and in portrait.
class OrientationTests: HaylenUITestCase {
    override func tearDown() {
        XCUIDevice.shared.orientation = .portrait
        super.tearDown()
    }

    func testTheDashboardTurnsWithTheDevice() throws {
        waitFor(element("Navigation"))
        turnDevice(.landscapeLeft)
        try saveScreenshot("landscape")
        turnDevice(.portrait)
        try saveScreenshot("portrait")
    }
}
#endif
