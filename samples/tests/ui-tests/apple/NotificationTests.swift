import XCTest

#if os(iOS)
// PLG-025: the plugin asks the notification center for notifications and schedules one, and the tap on the notification reaches the app while it shows, from the Home Screen and after the system ended it. The system answers the notifications of an app that installs again within a day the way the person answered them before, so their prompt shows only on the first run of a day.
class NotificationTests: TestProjectCase {
    static let notification = "Tap to open Haylen Tests."
    static let scheduledName = "A local notification is scheduled"
    static let tapName = "The tap on the notification reaches the app"
    // The words that the search of the menu finds this test alone by, typed on the keyboard of the system, since the field that edits is hidden.
    static let search = "plg notifications"

    override var code: String {
        "PLG-025"
    }

    override func tearDown() async throws {
        XCUIDevice.shared.orientation = .portrait
        try await super.tearDown()
    }

    func testTheNotificationTapsReachTheApp() throws {
        notify()
        tapNotification(containing: Self.notification)
        expectCheck(Self.tapName, containing: "received 1 taps")
        try saveScreenshot("notification")

        notify()
        goHome()
        tapNotification(containing: Self.notification)
        expectCheck(Self.tapName, containing: "received 2 taps")

        // The system starts the app without launch arguments, so the test opens again through the search of the menu. The device turns to the landscape of the app first, so the system opens the app in its orientation, since a window that turns after the launch drops the touches that reach it while it turns.
        notify()
        app.terminate()
        XCUIDevice.shared.orientation = .landscapeLeft
        tapNotification(containing: Self.notification)
        waitFor(element(matching: "Search by code or title.*")).tap()
        typeOnKeyboard(Self.search)
        waitFor(element(matching: "PLG-025\\s+Local notifications")).tap()
        expectCheck(Self.tapName, containing: "received 1 taps")
        try saveScreenshot("notification-launched")
    }

    // Schedules a notification, answers the prompt of the notification center where it shows, and waits until the plugin answers with the identifier, which is new for every notification.
    func notify() {
        let scheduled = check(Self.scheduledName)
        let before = scheduled.exists ? scheduled.label : ""
        tap("Notify in 5 seconds")
        let alert = Self.system.alerts.firstMatch
        let answered = { scheduled.exists && scheduled.label != before && !scheduled.label.hasPrefix("Waiting: ") }
        waitUntil("The plugin neither shows the prompt of the notifications nor schedules another notification.") {
            alert.exists || answered()
        }
        if alert.exists {
            answerAlert(.allow)
        }
        waitUntil("The plugin does not schedule another notification.", answered)
        expectCheck(Self.scheduledName)
    }
}
#endif
