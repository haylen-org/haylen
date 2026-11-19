import XCTest

#if os(iOS)
// The panel turns with the device and lays its cards out again in landscape and in portrait.
class OrientationTests: HaylenUITestCase {
    override func tearDown() async throws {
        XCUIDevice.shared.orientation = .portrait
        try await super.tearDown()
    }

    func testThePanelTurnsWithTheDevice() throws {
        waitFor(element("Morning scene"))
        turnDevice(.landscapeLeft)
        try saveScreenshot("landscape")
        turnDevice(.portrait)
        try saveScreenshot("portrait")
    }
}
#endif
