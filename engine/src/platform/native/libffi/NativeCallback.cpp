#include "haylen/platform/native/NativeCallback.hpp"

#include <ffi.h>

#include <new>
#include <stdexcept>
#include <utility>

namespace haylen::platform {

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
        throw std::runtime_error("The native callback could not be prepared by libffi.");
    }

    closure->closure = static_cast<ffi_closure*>(ffi_closure_alloc(sizeof(ffi_closure), &address));
    if (closure->closure == nullptr) {
        throw std::bad_alloc();
    }

    if (ffi_prep_closure_loc(closure->closure, &closure->cif, &Closure::run, this, address) != FFI_OK) {
        ffi_closure_free(closure->closure);
        throw std::runtime_error("The native callback could not be created by libffi.");
    }
}

NativeCallback::~NativeCallback() {
    ffi_closure_free(closure->closure);
}

} // namespace haylen::platform
