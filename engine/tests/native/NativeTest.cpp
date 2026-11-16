// The C entry points of the test library that the native interop tests and the test project load on every platform, whose names and C types the tests declare. Each one forwards to `NativeTestValues`, which exercises values, structs, buffers, text and callbacks, or to `NativeTestLibrary`, which works through the `HaylenNativeApi` of the engine.

#include "NativeTestLibrary.hpp"
#include "NativeTestValues.hpp"

#if defined(_WIN32)
#define NATIVE_TEST_EXPORT __declspec(dllexport)
#else
#define NATIVE_TEST_EXPORT __attribute__((visibility("default")))
#endif

extern "C" NATIVE_TEST_EXPORT std::int32_t native_test_add(std::int32_t a, std::int32_t b) {
    return NativeTestValues::add(a, b);
}

extern "C" NATIVE_TEST_EXPORT double native_test_scale(double value, double factor) {
    return NativeTestValues::scale(value, factor);
}

extern "C" NATIVE_TEST_EXPORT const char* native_test_origin() {
    return NativeTestLibrary::kOrigin;
}

extern "C" NATIVE_TEST_EXPORT NativeTestValues::Point native_test_point_add(NativeTestValues::Point a, NativeTestValues::Point b) {
    return NativeTestValues::addPoints(a, b);
}

extern "C" NATIVE_TEST_EXPORT void native_test_rect_grow(NativeTestValues::Rect* rect, float amount) {
    NativeTestValues::growRect(*rect, amount);
}

extern "C" NATIVE_TEST_EXPORT void native_test_fill(std::uint8_t* buffer, std::size_t size, std::uint8_t seed) {
    NativeTestValues::fill(std::span(buffer, size), seed);
}

extern "C" NATIVE_TEST_EXPORT std::uint32_t native_test_checksum(const std::uint8_t* buffer, std::size_t size) {
    return NativeTestValues::checksum(std::span(buffer, size));
}

extern "C" NATIVE_TEST_EXPORT std::int32_t native_test_visit(std::int32_t count, NativeTestValues::Visitor visitor) {
    return NativeTestValues::visit(count, visitor);
}

extern "C" NATIVE_TEST_EXPORT void native_test_report_later(NativeTestValues::Reporter reporter, std::int32_t value) {
    NativeTestValues::reportLater(reporter, value);
}

extern "C" NATIVE_TEST_EXPORT void native_test_push_frame_later(std::int32_t width, std::int32_t height, std::int32_t seed) {
    NativeTestLibrary::get().pushFrameLater(width, height, seed);
}

extern "C" NATIVE_TEST_EXPORT void native_test_push_tone_later(std::int32_t count, std::int32_t value) {
    NativeTestLibrary::get().pushToneLater(count, value);
}

// Returns 1 when the engine refuses to open the streams of `native-test` again with another format.
extern "C" NATIVE_TEST_EXPORT std::int32_t native_test_reopen_refused() {
    return NativeTestLibrary::get().isReopenRefused() ? 1 : 0;
}

extern "C" NATIVE_TEST_EXPORT std::int32_t native_test_error_count() {
    return NativeTestLibrary::get().getErrorCount();
}

extern "C" NATIVE_TEST_EXPORT const char* native_test_last_error() {
    return NativeTestLibrary::get().getLastError();
}

extern "C" NATIVE_TEST_EXPORT void native_test_close_screen_later() {
    NativeTestLibrary::get().closeScreenLater();
}

extern "C" NATIVE_TEST_EXPORT std::intptr_t native_test_window() {
    return NativeTestLibrary::get().getWindow();
}

extern "C" NATIVE_TEST_EXPORT void native_test_cover(std::int32_t covered) {
    NativeTestLibrary::get().setCovered(covered != 0);
}

extern "C" NATIVE_TEST_EXPORT int native_test_haylen_init(const HaylenNativeApi* api) {
    return NativeTestLibrary::initialize(api);
}

extern "C" NATIVE_TEST_EXPORT int native_test_failing_init(const HaylenNativeApi*) {
    return NativeTestLibrary::failInitialization();
}
