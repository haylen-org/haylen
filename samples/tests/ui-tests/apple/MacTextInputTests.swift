#if os(macOS)
import AppKit
import Carbon
import XCTest

// The fields of the test "Text fields" type through the keyboard of the Mac, in a macOS and in a Mac Catalyst app: each key types once, Backspace deletes, a held Backspace goes on deleting while it repeats, an input method composes text, a paste replaces the selection, and Tab moves to the next field.
class MacTextInputTests: TestProjectCase {
    // The virtual key code of Backspace on the keyboards of the Mac, which macOS calls "delete".
    static let backspace: UInt16 = 51
    // The input source of Japanese that composes kana from Latin letters, which every Mac has.
    static let japanese = "com.apple.inputmethod.Kotoeri.RomajiTyping.Japanese"

    override var code: String {
        "GUI-005"
    }

    // The input source of the Mac before the test, which the test selects again, and the input sources that the test turned on, which it turns off again, the last one first.
    var inputSource: TISInputSource?
    var enabledSources: [TISInputSource] = []

    override func tearDown() async throws {
        if let source = inputSource {
            TISSelectInputSource(source)
        }
        for source in enabledSources.reversed() {
            TISDisableInputSource(source)
        }
        try await super.tearDown()
    }

    func testEachKeyTypesOnceAndBackspaceDeletes() throws {
        startTyping("https://haylen.dev")
        app.typeText("oi")
        expectReport("url", "oi")
        app.typeKey(XCUIKeyboardKey.delete, modifierFlags: [])
        expectReport("url", "o")
        app.typeKey("l", modifierFlags: [])
        app.typeKey("a", modifierFlags: [])
        expectReport("url", "ola")
        try saveScreenshot("mac-text-typed")
    }

    // A key held down reaches the app as one press and then the repeats of the system, each marked as a repeat, for longer than the word takes to delete, so the field ends empty.
    func testHoldingBackspaceGoesOnDeleting() throws {
        startTyping("https://haylen.dev")
        app.typeText("ilhas")
        expectReport("url", "ilhas")
        holdKey(Self.backspace, repeats: 12)
        expectReport("url", "")
        try saveScreenshot("mac-backspace-held")
    }

    // The Japanese input method composes the kana of the Latin letters while they are typed, and Return commits them into the field.
    func testAnInputMethodComposesTheTextOfTheField() throws {
        startTyping("Island name")
        useInputSource(Self.japanese)
        // The input method keeps a last "n" open for the kana that may follow it, and a second "n" closes it as "ん".
        app.typeText("nihonn")
        app.typeKey(XCUIKeyboardKey.return, modifierFlags: [])
        expectReport("text", "にほん")
        try saveScreenshot("mac-composed")
    }

    func testPasteReplacesTheSelection() throws {
        startTyping("https://haylen.dev")
        app.typeText("mar")
        expectReport("url", "mar")
        let pasteboard = NSPasteboard.general
        pasteboard.clearContents()
        pasteboard.setString("ilha", forType: .string)
        app.typeKey("a", modifierFlags: .command)
        app.typeKey("v", modifierFlags: .command)
        expectReport("url", "ilha")
        try saveScreenshot("mac-pasted")
    }

    // The address field comes before the search field, so Tab moves the editing there and the next key types into the search field.
    func testTabMovesToTheNextField() throws {
        startTyping("https://haylen.dev")
        app.typeText("a")
        expectReport("url", "a")
        app.typeKey(XCUIKeyboardKey.tab, modifierFlags: [])
        app.typeText("b")
        expectReport("search", "b")
        try saveScreenshot("mac-next-field")
    }

    // Clicks the field whose placeholder is the text, which starts its editing.
    func startTyping(_ placeholder: String) {
        tap(placeholder)
    }

    // The test reports every change of a field on its status line.
    func expectReport(_ field: String, _ text: String, file: StaticString = #filePath, line: UInt = #line) {
        waitFor(element("The field \"\(field)\" reported \"change\" with \"\(text)\"."), timeout: 10, file: file, line: line)
    }

    // Selects an input mode of the Mac until the test ends, turning on first its input method and the mode when the Mac has them off. A mode can report itself on while its input method is off, as on a Mac where nobody added the language, and then only once the input method is on can the mode be selected.
    func useInputSource(_ identifier: String, file: StaticString = #filePath, line: UInt = #line) {
        guard let mode = Self.inputSource(identifier) else {
            XCTFail("The Mac has no input source \"\(identifier)\".", file: file, line: line)
            return
        }
        guard let method = Self.inputMethod(of: mode) else {
            XCTFail("The Mac has no input method for the input source \"\(identifier)\".", file: file, line: line)
            return
        }

        inputSource = TISCopyCurrentKeyboardInputSource().takeRetainedValue()
        for source in [method, mode] where !Self.isEnabled(source) {
            XCTAssertEqual(TISEnableInputSource(source), noErr, "The input source \"\(Self.property(source, kTISPropertyInputSourceID) as? String ?? identifier)\" does not turn on.", file: file, line: line)
            enabledSources.append(source)
        }
        XCTAssertEqual(TISSelectInputSource(mode), noErr, "The input source \"\(identifier)\" cannot be selected.", file: file, line: line)
    }

    static func inputSource(_ identifier: String) -> TISInputSource? {
        let filter = [kTISPropertyInputSourceID as String: identifier] as CFDictionary
        let sources = TISCreateInputSourceList(filter, true)?.takeRetainedValue() as? [TISInputSource]
        return sources?.first
    }

    // The input method that a mode belongs to, the input source of its bundle that has modes.
    static func inputMethod(of mode: TISInputSource) -> TISInputSource? {
        guard let bundle = property(mode, kTISPropertyBundleID) as? String else {
            return nil
        }

        let filter = [kTISPropertyBundleID as String: bundle, kTISPropertyInputSourceType as String: kTISTypeKeyboardInputMethodModeEnabled as String] as CFDictionary
        let sources = TISCreateInputSourceList(filter, true)?.takeRetainedValue() as? [TISInputSource]
        return sources?.first
    }

    static func isEnabled(_ source: TISInputSource) -> Bool {
        property(source, kTISPropertyInputSourceIsEnabled) as? Bool == true
    }

    static func property(_ source: TISInputSource, _ key: CFString) -> AnyObject? {
        guard let value = TISGetInputSourceProperty(source, key) else {
            return nil
        }

        return Unmanaged<AnyObject>.fromOpaque(value).takeUnretainedValue()
    }
}
#endif
