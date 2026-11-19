import XCTest

#if os(iOS)
// PLG-011: the prompts of the camera, the microphone, the location and notifications answer the app, the camera of the plugin follows the answer, and the tap on a notification reaches the app while it shows, from the Home Screen and after the system ended it. A run installs the app anew, so every prompt shows.
class PermissionTests: TestProjectCase {
    static let allow = ["Allow"]
    static let deny = ["Don’t Allow", "Don't Allow"]
    static let allowWhileUsing = ["Allow While Using App"]
    static let notification = "Tap to open Haylen Tests."
    static let tapName = "The tap on the notification reaches the app"
    // The words that the search of the menu finds this test alone by, typed on the keyboard of the system, since the field that edits is hidden.
    static let search = "plg permissions"

    override var code: String {
        "PLG-011"
    }

    override var resetResources: [XCUIProtectedResource] {
        [.camera, .microphone, .location]
    }

    func testThePromptsAnswerTheAppAndTheNotificationTapsReachIt() throws {
        ask("camera", "Ask for the camera", Self.allow, "granted")
        ask("microphone", "Ask for the microphone", Self.deny, "denied")
        ask("location", "Ask for the location", Self.allowWhileUsing, "granted")
        ask("notifications", "Ask for notifications", Self.allow, "granted")
        tap("Start the camera")
#if targetEnvironment(simulator)
        expectCheck("The camera of the plugin follows the permission", "N/A", containing: "This device has no camera.")
#else
        expectCheck("The camera of the plugin follows the permission", containing: "The camera started at")
#endif
        try saveScreenshot("permissions")

        notify()
        tapNotification(containing: Self.notification)
        expectCheck(Self.tapName, containing: "received 1 taps")

        notify()
        goHome()
        tapNotification(containing: Self.notification)
        expectCheck(Self.tapName, containing: "received 2 taps")

        // The system starts the app without launch arguments, so the test opens again through the search of the menu.
        notify()
        app.terminate()
        tapNotification(containing: Self.notification)
        waitFor(element(matching: "Search by code or title.*")).tap()
        waitFor(app.keyboards.firstMatch)
        for character in Self.search {
            let key = character == " " ? "space" : String(character)
            waitFor(app.keyboards.keys.matching(NSPredicate(format: "label ==[c] %@", key)).firstMatch).tap()
        }
        waitFor(element(matching: "PLG-011\\s+Permissions")).tap()
        expectCheck(Self.tapName, containing: "received 1 taps")
        try saveScreenshot("notification-launched")
    }

    // Asks for a permission and answers its prompt, which shows while the system still asks. The system answers the notifications of an app that installs again within a day the way the person answered them before, so their prompt shows only on the first run of a day.
    func ask(_ kind: String, _ button: String, _ answer: [String], _ state: String) {
        let card = element(startingWith: "\(kind.prefix(1).uppercased() + kind.dropFirst()): ")
        waitUntil("The card of the \(kind) permission never tells its state.") {
            card.exists && !card.label.hasSuffix(": Checking")
        }
        let prompts = card.label.hasSuffix(": Not asked yet")
        tap(button)
        if prompts {
            answerAlert(answer)
        }
        expectCheck("The \(kind) permission reaches the app", containing: "The request answered \"\(state)\".")
    }

    // Schedules a notification and waits until the plugin answers with its identifier, which is new for every notification.
    func notify() {
        let scheduled = check("A local notification is scheduled")
        let before = scheduled.exists ? scheduled.label : ""
        tap("Notify in 5 seconds")
        waitUntil("The plugin does not schedule another notification.") {
            scheduled.exists && scheduled.label != before
        }
        expectCheck("A local notification is scheduled")
    }
}
#endif
