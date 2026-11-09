#pragma once

namespace haylen::platform {

// Reports the time zone of the system to the engine, as its IANA name.
class AppleTimeZone final {
  public:
    // Reports the time zone now and after every change that the system announces.
    static void observe();

  private:
    static void report();
};

} // namespace haylen::platform
