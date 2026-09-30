#pragma once

namespace haylen::platform {

// The back button of Android, which reaches the app through the back callback of the activity while the app captures back or edits a text field, and leaves the app with the back animation of the system otherwise. Every other key arrives through the key table of `sokol_app`, where the play and pause key of TV remotes is the pause key.
class AndroidKeys final {
  public:
    // Presses and releases escape, which the UI reads as `uiCancel`, for the back callback of the activity, which Android calls from its UI thread once the back gesture completes.
    static void receiveBack();
};

} // namespace haylen::platform
