from tests.support.engine_sources import EngineSources
from tests.support.namespace_scope import NamespaceScope


class NamespaceScopeTest:
    """Every function of the engine, its tests included, is a method of a class, and nothing lives in an anonymous namespace or as a variable at namespace scope. The only functions outside a class are the entry points whose names the platform or the language dictates, which have C linkage or are `main`, `WinMain`, the JNI entry points or the replacements of the global allocation functions."""

    def test_the_engine_declares_no_function_or_variable_at_namespace_scope(self):
        problems = []
        for name, text in EngineSources.read().items():
            problems += [f"{name} declares {problem} at namespace scope." for problem in NamespaceScope.problems(text)]
        assert problems == []

    def test_the_scan_finds_what_the_rules_forbid(self):
        source = """
            namespace haylen::core {
            namespace {
            constexpr std::array<int, 2> kValues = {1, 2};
            } // namespace
            static bool ready;
            void helper(int value) {}
            auto twice = [](int value) { return value * 2; };
            bool operator==(const Clock&, const Clock&);
            void (*callback)(int) = nullptr;
            Clock::Clock() : ticks{0}, rate(1) {}
            Clock& Clock::operator=(Clock&& other) noexcept { return *this; }
            const int Clock::kLimit = 3;
            class Timer { void tick() {} };
            TEST_F(ClockTest, Ticks) { EXPECT_TRUE(ready); }
            } // namespace haylen::core
            int main() { return 0; }
            void* operator new(std::size_t size) { return std::malloc(size); }
            extern "C" void native_init(void*) {}
        """
        assert NamespaceScope.problems(source) == [
            "an anonymous namespace",
            'the variable "kValues"',
            'the variable "ready"',
            'the function "helper"',
            'the variable "twice"',
            'the function "operator=="',
            'the variable "callback"',
        ]
