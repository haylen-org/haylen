// The part of the plugin for the test port, which its backend compiles into the app and registers under the id of the plugin, so Lua calls it through "native.load".
class TestPortGreeter final {
  public:
    static const char* getGreeting() noexcept {
        return "Hello from the part of the plugin.";
    }
};

extern "C" const char* test_port_greeter_greeting() {
    return TestPortGreeter::getGreeting();
}
