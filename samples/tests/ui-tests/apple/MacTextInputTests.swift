#if os(macOS)
import AppKit
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

    // The Japanese input method composes the kana of the Latin letters while they are typed, and Return commits them into the field. The input source is selected before the field edits, as a person picks it before typing: macOS hands an input source of Chinese, Japanese, Korean or Vietnamese that a process selects to a field only once the field takes the focus, so a field that already edits keeps typing Latin letters.
    func testAnInputMethodComposesTheTextOfTheField() throws {
        useInputSource(Self.japanese)
        startTyping("Island name")
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
}
#endif
