#include "platform/native/NativeCallback.hpp"

#include <cstring>
#include <new>
#include <stdexcept>
#include <utility>

#include "platform/native/NativeCallbacks.hpp"

#if !defined(__EMSCRIPTEN__)
#include <ffi.h>
#endif

namespace haylen::platform {

std::mutex& NativeCallback::retainedMutex = *new std::mutex();
std::unordered_map<const NativeCallback*, std::shared_ptr<NativeCallback>>& NativeCallback::retained = *new std::unordered_map<const NativeCallback*, std::shared_ptr<NativeCallback>>();

#if defined(__EMSCRIPTEN__)
struct NativeCallback::Closure {};

std::shared_ptr<NativeCallback> NativeCallback::create(NativeSignature, Thread, Link) {
    throw std::runtime_error("Native callbacks are not available in the browser. Call JavaScript through haylen.platform instead.");
}

NativeCallback::~NativeCallback() = default;
#else
// The libffi closure behind the function pointer, whose entry point forwards every call to its callback.
struct NativeCallback::Closure {
    ffi_cif cif{};
    std::vector<ffi_type*> types;
    ffi_closure* closure = nullptr;

    static void run(ffi_cif*, void*, void** arguments, void* data) {
        invoke(data, arguments);
    }

    [[nodiscard]] static ffi_type* getType(const NativeSignature::Parameter& parameter) {
        using Kind = NativeSignature::Parameter::Kind;
        if (parameter.kind == Kind::Float) {
            return &ffi_type_float;
        }
        if (parameter.kind == Kind::Double) {
            return &ffi_type_double;
        }
        if (parameter.kind != Kind::Integer && parameter.kind != Kind::Boolean) {
            return &ffi_type_pointer;
        }
        switch (parameter.size) {
        case 1:
            return parameter.isSigned ? &ffi_type_sint8 : &ffi_type_uint8;
        case 2:
            return parameter.isSigned ? &ffi_type_sint16 : &ffi_type_uint16;
        case 4:
            return parameter.isSigned ? &ffi_type_sint32 : &ffi_type_uint32;
        default:
            return parameter.isSigned ? &ffi_type_sint64 : &ffi_type_uint64;
        }
    }
};

std::shared_ptr<NativeCallback> NativeCallback::create(NativeSignature signature, Thread thread, Link link) {
    std::shared_ptr<NativeCallback> callback(new NativeCallback(std::move(signature), thread, std::move(link)));
    const std::scoped_lock lock(retainedMutex);
    retained.emplace(callback.get(), callback);
    return callback;
}

NativeCallback::NativeCallback(NativeSignature value, Thread mode, Link target) : signature(std::move(value)), thread(mode), link(std::move(target)), closure(std::make_unique<Closure>()) {
    for (const NativeSignature::Parameter& parameter : signature.getParameters()) {
        closure->types.push_back(Closure::getType(parameter));
    }

    if (ffi_prep_cif(&closure->cif, FFI_DEFAULT_ABI, static_cast<unsigned>(closure->types.size()), &ffi_type_void, closure->types.data()) != FFI_OK) {
        throw std::runtime_error("libffi could not prepare the native callback.");
    }
    closure->closure = static_cast<ffi_closure*>(ffi_closure_alloc(sizeof(ffi_closure), &address));
    if (closure->closure == nullptr) {
        throw std::bad_alloc();
    }
    if (ffi_prep_closure_loc(closure->closure, &closure->cif, &Closure::run, this, address) != FFI_OK) {
        ffi_closure_free(closure->closure);
        throw std::runtime_error("libffi could not create the native callback.");
    }
}

NativeCallback::~NativeCallback() {
    ffi_closure_free(closure->closure);
}
#endif

void NativeCallback::release(const std::shared_ptr<NativeCallback>& callback) {
    const std::scoped_lock lock(retainedMutex);
    retained.erase(callback.get());
}

void NativeCallback::invoke(void* data, void** arguments) {
    const auto& callback = *static_cast<const NativeCallback*>(data);
    std::string failure;
    std::vector<Value> values = callback.read(arguments, failure);
    callback.deliver(std::move(values), std::move(failure));
}

// Each branch widens on its own, because a conditional of a signed and an unsigned integer would convert both to unsigned.
std::int64_t NativeCallback::readInteger(const void* argument, const NativeSignature::Parameter& parameter) {
    switch (parameter.size) {
    case 1:
        return parameter.isSigned ? std::int64_t{*static_cast<const std::int8_t*>(argument)} : std::int64_t{*static_cast<const std::uint8_t*>(argument)};
    case 2:
        return parameter.isSigned ? std::int64_t{*static_cast<const std::int16_t*>(argument)} : std::int64_t{*static_cast<const std::uint16_t*>(argument)};
    case 4:
        return parameter.isSigned ? std::int64_t{*static_cast<const std::int32_t*>(argument)} : std::int64_t{*static_cast<const std::uint32_t*>(argument)};
    default:
        return *static_cast<const std::int64_t*>(argument);
    }
}

std::vector<NativeCallback::Value> NativeCallback::read(void** arguments, std::string& failure) const {
    using Kind = NativeSignature::Parameter::Kind;
    const std::vector<NativeSignature::Parameter>& parameters = signature.getParameters();
    std::vector<Value> values(parameters.size());
    for (std::size_t index = 0; index < parameters.size(); ++index) {
        const NativeSignature::Parameter& parameter = parameters[index];
        const void* argument = arguments[index];
        switch (parameter.kind) {
        case Kind::Boolean:
            values[index] = *static_cast<const bool*>(argument);
            break;
        case Kind::Integer:
            values[index] = readInteger(argument, parameter);
            break;
        case Kind::Float:
            values[index] = static_cast<double>(*static_cast<const float*>(argument));
            break;
        case Kind::Double:
            values[index] = *static_cast<const double*>(argument);
            break;
        case Kind::Pointer:
            if (void* pointer = *static_cast<void* const*>(argument)) {
                values[index] = pointer;
            }
            break;
        case Kind::Text:
            if (const char* text = *static_cast<const char* const*>(argument)) {
                values[index] = std::string(text);
            }
            break;
        case Kind::Bytes: {
            const std::int64_t count = parameter.lengthParameter ? readInteger(arguments[*parameter.lengthParameter], parameters[*parameter.lengthParameter]) : static_cast<std::int64_t>(parameter.count);
            if (count < 0) {
                failure = "The native callback received the negative length " + std::to_string(count) + " for " + parameter.name + ".";
                return {};
            }
            if (const auto* bytes = *static_cast<const char* const*>(argument)) {
                values[index] = std::string(bytes, static_cast<std::size_t>(count) * parameter.size);
            }
            break;
        }
        }
    }
    return values;
}

// A call on the frame thread of a frame callback runs at once, and any other call waits in the mailbox of the bridge, which drops it once the app is gone.
void NativeCallback::deliver(std::vector<Value> values, std::string failure) const {
    if (thread == Thread::Frame && std::this_thread::get_id() == link.frameThread) {
        const std::shared_ptr<NativeCallbacks> owner = link.owner.lock();
        if (owner && failure.empty()) {
            owner->deliver(link.id, values);
        } else if (owner) {
            owner->fail(link.id, failure);
        }
        return;
    }

    // clang-format off
    link.mailbox.post([owner = link.owner, id = link.id, values = std::move(values), failure = std::move(failure)] {
        const std::shared_ptr<NativeCallbacks> alive = owner.lock();
        if (alive && failure.empty()) {
            alive->deliver(id, values);
        } else if (alive) {
            alive->fail(id, failure);
        }
    });
    // clang-format on
}

} // namespace haylen::platform
