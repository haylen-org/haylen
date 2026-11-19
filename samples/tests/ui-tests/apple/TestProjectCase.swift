import XCTest

// The base of the UI tests that open one test of the test project, which the launch arguments "--test <code>" name, and read its checks. Each check is a card whose label is the line of its row, such as "Pass: The camera starts. The camera started at 640 by 480 pixels.".
class TestProjectCase: HaylenUITestCase {
    // The button of the prompts of the system that allows what the plugin asks for, in the words of a device in English.
    static let allow = ["Allow"]

    // The code of the test that the app opens, such as "PLG-016".
    var code: String {
        preconditionFailure("A test class of the test project names the code of its test.")
    }

    override var launchArguments: [String] {
        ["--test", code]
    }

    override func setUp() async throws {
        try await super.setUp()
        waitFor(element(code))
    }

    // The card of the check with the name, in whatever state it is.
    func check(_ name: String) -> XCUIElement {
        let pattern = "(?s)(Waiting|Pass|Fail|Info|N/A): \(NSRegularExpression.escapedPattern(for: name))\\..*"
        return app.descendants(matching: .any).matching(NSPredicate(format: "label MATCHES %@", pattern)).firstMatch
    }

    // Waits until the check with the name reaches the state with a line that holds the text, and fails at once when the check fails instead. It returns the line of the check.
    @discardableResult
    func expectCheck(_ name: String, _ state: String = "Pass", containing text: String = "", timeout: TimeInterval = HaylenUITestCase.timeout, file: StaticString = #filePath, line: UInt = #line) -> String {
        let card = check(name)
        let expected = "\(state): \(name)."
        waitUntil("The check \"\(name)\" does not reach \"\(state)\" with \"\(text)\". It reads \"\(card.exists ? card.label : "nothing")\".", timeout: timeout, file: file, line: line) {
            guard card.exists else {
                return false
            }

            let label = card.label
            return label.hasPrefix("Fail: ") || (label.hasPrefix(expected) && (text.isEmpty || label.contains(text)))
        }

        let settled = card.exists ? card.label : ""
        XCTAssertTrue(settled.hasPrefix(expected), "The check reads \"\(settled)\" instead of \"\(state)\".", file: file, line: line)
        return settled
    }
}
