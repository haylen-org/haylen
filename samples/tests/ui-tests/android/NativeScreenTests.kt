package com.haylen.uitests

import java.util.regex.Pattern
import org.junit.Test

// PLG-008: a native screen of the plugin covers the whole app, which halts until its Close button ends the cover.
class CoverTests : TestProjectCase() {
    override val code = "PLG-008"

    @Test
    fun theNativeScreenHaltsTheAppUntilItCloses() {
        tap("Show the native screen")
        tapButton("Close")
        expectCheck("The native screen closes", containing = "answered once Close closed it")
        expectCheck("The call \"haylen.appCovered()\" returns \"true\" while it shows")
        expectCheck("No update runs while covered")
        expectCheck("The events \"appInactive\" and \"appActive\" arrive")
    }
}

// PLG-009: a screen of the plugin answers the call, covers the app, refuses a second screen, closes on a cancel and answers the app that starts again under it as "screenRestored". The launch arguments open the test again after the restart, as the person would.
class ScreenTests : TestProjectCase() {
    companion object {
        // The second after which the test restarts the app under the screen, with the time of a few frames.
        const val RESTART_DELAY = 2_000L
    }

    override val code = "PLG-009"

    @Test
    fun theScreenAnswersTheCallAndTheNextApp() {
        tap("Open the screen")
        tapButton("Confirm")
        expectCheck("The screen answers the call", containing = "The person confirmed")
        expectCheck("The app is covered while the screen shows")
        expectCheck("A second screen fails with \"busy\"")

        tap("Cancel after a second")
        expectCheck("A cancel closes the screen", containing = "The call failed with \"cancelled\"")

        // The app restarts a second after the screen covers it, from a timer that nothing outside the app shows, so the test answers the screen once that time passed.
        tap("Restart under it")
        waitFor(Pattern.compile("Decline", Pattern.CASE_INSENSITIVE))
        Thread.sleep(RESTART_DELAY)
        tapButton("Decline")
        expectCheck("The next app hears \"screenRestored\"", containing = "declined")
        expectCheck("The SwiftUI screen answers the call", "N/A")
        saveScreenshot("screen-answers")
    }
}

// PLG-019: the fake store lists its products, a purchase answers once the native dialog buys or cancels it, and the kept product comes back as a restored purchase.
class StoreTests : TestProjectCase() {
    override val code = "PLG-019"

    @Test
    fun purchasesAnswerAndTheKeptOneComesBack() {
        expectCheck("The store lists its products", containing = "holds 3 products")
        tap("0.99 USD")
        tapButton("Buy for 0.99 USD")
        expectCheck("The purchase of \"coins.small\" answers", containing = "bought it in the transaction")

        tap("9.99 USD")
        tapButton("Cancel")
        expectCheck("The purchase of \"coins.large\" answers", containing = "The person cancelled")

        tap("2.99 USD")
        tapButton("Buy for 2.99 USD")
        expectCheck("The purchase of \"ads.remove\" answers", containing = "bought it in the transaction")
        tap("Restore the purchases")
        expectCheck("The kept purchases come back", containing = "The device keeps \"ads.remove\".")
        expectCheck("The event \"purchaseUpdated\" reports each transaction")
        saveScreenshot("store")
    }
}
