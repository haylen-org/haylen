#include "platform/native/NativeCallback.hpp"

#include <stdexcept>

namespace haylen::platform {

// A platform without the closures of libffi, such as a page or a port that does not allow `executableMemory`, makes no function pointer that runs Lua.
struct NativeCallback::Closure {};

std::shared_ptr<NativeCallback> NativeCallback::create(NativeSignature, Thread, Link) {
    throw std::runtime_error("This platform makes no native callback, since it writes no code into memory for the function pointer that native code would call.");
}

NativeCallback::~NativeCallback() = default;

} // namespace haylen::platform
