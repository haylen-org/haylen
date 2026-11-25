#if os(iOS) || os(macOS)
import XCTest

// The keys that the test presses reach the test "Keyboard" while its play area has the focus, on an iPhone or an iPad and on a Mac as a macOS and as a Mac Catalyst app. The keys of a UI test come from no keyboard of the hardware, as the keys of Voice Control, Switch Control and remote control do, so they reach the app only through the keys that the system hands its view.
class KeyboardTests: TestProjectCase {
    override var code: String {
        "INP-001"
    }

    func testTheKeysOfATestReachTheApp() throws {
        app.typeKey("a", modifierFlags: [])
        expectPresses("The keyboard saw 1 press, with no modifier held")
        app.typeKey("b", modifierFlags: [])
        expectPresses("The keyboard saw 2 presses, with no modifier held")
        try saveScreenshot("keyboard-keys")
    }

    // The test shows how many keys went down and which modifiers are held on its status line, followed by the characters typed, which only some platforms type without a text field.
    func expectPresses(_ text: String, file: StaticString = #filePath, line: UInt = #line) {
        let status = app.descendants(matching: .any).matching(NSPredicate(format: "label BEGINSWITH %@", text)).firstMatch
        waitFor(status, timeout: 10, file: file, line: line)
    }
}
#endif
