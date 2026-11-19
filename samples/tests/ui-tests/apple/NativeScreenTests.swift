import XCTest

// PLG-008: a native screen of the plugin covers the whole app, which halts until its Close button ends the cover.
class CoverTests: TestProjectCase {
    override var code: String {
        "PLG-008"
    }

    func testTheNativeScreenHaltsTheAppUntilItCloses() throws {
        activate("Show the native screen")
        let close = app.buttons["Close"]
        waitFor(close)
        try saveScreenshot("native-screen")
        activate(close)
        expectCheck("The native screen closes", containing: "answered once Close closed it")
        expectCheck("The call \"haylen.appCovered()\" returns \"true\" while it shows")
        expectCheck("No update runs while covered")
        expectCheck("The events \"appInactive\" and \"appActive\" arrive")
    }
}

// PLG-009: a screen of the plugin answers the call, covers the app, refuses a second screen, closes on a cancel, answers the app that starts again under it as "screenRestored", and shows in SwiftUI too. The launch arguments open the test again after the restart, as the person would.
class ScreenTests: TestProjectCase {
    // The second after which the test restarts the app under the screen, with the time of a few frames.
    static let restartDelay: TimeInterval = 2

    override var code: String {
        "PLG-009"
    }

    func testTheScreenAnswersTheCallAndTheNextApp() throws {
        activate("Open the screen")
        activate(app.buttons["Confirm"])
        expectCheck("The screen answers the call", containing: "The person confirmed")
        expectCheck("The app is covered while the screen shows")
        expectCheck("A second screen fails with \"busy\"")

        activate("Cancel after a second", after: [.down])
        expectCheck("A cancel closes the screen", containing: "The call failed with \"cancelled\"")

        // The app restarts a second after the screen covers it, from a timer that nothing outside the app shows, so the test answers the screen once that time passed.
        activate("Restart under it", after: [.down])
        waitFor(app.buttons["Decline"])
        Thread.sleep(forTimeInterval: Self.restartDelay)
        activate(app.buttons["Decline"])
        expectCheck("The next app hears \"screenRestored\"", containing: "declined")

        // The app that starts again under the screen focuses "Open the screen", and the focus skips "Open by redirect", which only the web offers.
        activate("Open in SwiftUI", after: [.down, .down, .down])
        activate(app.buttons["Confirm"])
        expectCheck("The SwiftUI screen answers the call", containing: "The person confirmed")
        try saveScreenshot("screen-answers")
    }
}

// PLG-019: the fake store lists its products, a purchase answers once the native sheet buys or cancels it, and the kept product comes back as a restored purchase.
class StoreTests: TestProjectCase {
    override var code: String {
        "PLG-019"
    }

    func testPurchasesAnswerAndTheKeptOneComesBack() throws {
        expectCheck("The store lists its products", containing: "holds 3 products")

        // The remote of the TV starts on "Restore the purchases", and the products lie left of it, one under the other. The purchase sheet is an alert, whose buttons tvOS nests in buttons of the same label.
        activate("0.99 USD", after: [.left])
        activate(app.alerts.buttons["Buy for 0.99 USD"].firstMatch)
        expectCheck("The purchase of \"coins.small\" answers", containing: "bought it in the transaction")

        activate("9.99 USD", after: [.down])
        activate(app.alerts.buttons["Cancel"].firstMatch)
        expectCheck("The purchase of \"coins.large\" answers", containing: "The person cancelled")

        activate("2.99 USD", after: [.down])
        activate(app.alerts.buttons["Buy for 2.99 USD"].firstMatch)
        expectCheck("The purchase of \"ads.remove\" answers", containing: "bought it in the transaction")
        activate("Restore the purchases", after: [.right, .up])
        expectCheck("The kept purchases come back", containing: "The device keeps \"ads.remove\".")
        expectCheck("The event \"purchaseUpdated\" reports each transaction")
        try saveScreenshot("store")
    }
}
