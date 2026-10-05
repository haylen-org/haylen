package dev.haylen.app

import android.app.Application

// The application class of the app, which starts before the first activity. Native platform bridge handlers of the app are registered here, with `HaylenBridge.register` of the `dev.haylen` package, and its native code sends events to the app with `HaylenBridge.emit` at any time.
class App : Application() {
    override fun onCreate() {
        super.onCreate()
    }
}
