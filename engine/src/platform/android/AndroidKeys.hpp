#pragma once

namespace haylen::platform {

// The back button of Android, which reaches the app through the back callback of the activity while the app captures back or edits a text field, and leaves the app with the back animation of the system otherwise. Every other key arrives through `AndroidInputEvents`.
class AndroidKeys final {
  public:
    // Hands a press of back to the runtime for the back callback of the activity, which Android calls from its UI thread once the back gesture completes. The app hears it as escape, which the UI reads as `uiCancel`, and a launched app that does not take back quits to the app that launched it.
    static void receiveBack();
};

} // namespace haylen::platform
