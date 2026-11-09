#pragma once

namespace haylen::platform {

// Reports the languages the person prefers to the engine from `NSLocale.preferredLanguages`, which also holds the language that the settings of the system give the app alone and the `-AppleLanguages` argument of a launch.
class AppleLanguages final {
  public:
    // Reports the languages now and again whenever the system announces that its locale changed.
    static void observe();

  private:
    static void report();
};

} // namespace haylen::platform
