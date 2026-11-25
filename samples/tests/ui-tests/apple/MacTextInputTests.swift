#if os(macOS)
import AppKit
import XCTest

// The fields of the test "Text fields" type through the keyboard of the Mac, in a macOS and in a Mac Catalyst app: each key types once, Backspace deletes, a held Backspace goes on deleting while it repeats, an input method composes text, a paste replaces the selection, and Tab moves to the next field.
class MacTextInputTests: TestProjectCase {
    // The virtual key code of Backspace on the keyboards of the Mac, which macOS calls "delete".
    static let backspace: UInt16 = 51
    // The virtual key codes of the letters that the input method composes and of Return on the keyboards of the Mac.
    static let n: UInt16 = 45
    static let i: UInt16 = 34
    static let h: UInt16 = 4
    static let o: UInt16 = 31
    static let returnKey: UInt16 = 36
    // The input source of Japanese that composes kana from Latin letters, which every Mac has.
    static let japanese = "com.apple.inputmethod.Kotoeri.RomajiTyping.Japanese"
    // The domain of the settings of the Japanese input method and its live conversion, which turns the kana into a word of its dictionary while they are typed.
    static let japaneseSettings = "com.apple.inputmethod.Kotoeri"
    static let liveConversion = "JIMPrefLiveConversionKey"

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

    // The Japanese input method composes the kana of the Latin letters while they are typed, which the field holds as a composition until Return commits them. The live conversion, which macOS turns on by default, would commit a word of its dictionary instead, so the test turns it off and its text depends on no setting or dictionary of the Mac.
    func testAnInputMethodComposesTheTextOfTheField() throws {
        setInputMethodPreference(Self.liveConversion, of: Self.japaneseSettings, to: false)
        useInputSource(Self.japanese)
        startTyping("Island name")
        // The input method keeps a last "n" open for the kana that may follow it, and a second "n" closes it as "ん".
        pressKeys([Self.n, Self.i, Self.h, Self.o, Self.n, Self.n])
        waitFor(element("The input method composes \"にほん\"."), timeout: 10)
        pressKeys([Self.returnKey])
        waitFor(element("No input method composes text."), timeout: 10)
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
