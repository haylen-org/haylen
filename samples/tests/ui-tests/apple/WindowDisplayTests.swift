import XCTest

#if os(visionOS)
// PLT-008: on visionOS the window of the app is its one display, so a size that the app asks for through "window.setFrame" resizes the window and publishes "displayChanged" for it.
class WindowDisplayTests: TestProjectCase {
    override var code: String {
        "PLT-008"
    }

    func testAResizedWindowChangesItsDisplay() throws {
        tap("Resize the window")
        waitFor(element("Last event: displayChanged main"))
        try saveScreenshot("window-display-resized")
    }
}
#endif
