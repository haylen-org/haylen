#include "platform/native/NativeCallback.hpp"

#include <cstring>
#include <new>
#include <stdexcept>
#include <utility>

#include "haylen/core/Log.hpp"
#include "platform/native/NativeCallbacks.hpp"

namespace haylen::platform {

std::mutex& NativeCallback::retainedMutex = *new std::mutex();
std::unordered_map<const NativeCallback*, std::shared_ptr<NativeCallback>>& NativeCallback::retained = *new std::unordered_map<const NativeCallback*, std::shared_ptr<NativeCallback>>();

void NativeCallback::release(const std::shared_ptr<NativeCallback>& callback) {
    const std::scoped_lock lock(retainedMutex);
    retained.erase(callback.get());
}

// Native code called through C frames, so no exception may leave, and a failure such as running out of memory drops the call with a line in the log.
void NativeCallback::invoke(void* data, void** arguments) noexcept {
    try {
        const auto& callback = *static_cast<const NativeCallback*>(data);
        std::string failure;
        std::vector<Value> values = callback.read(arguments, failure);
        callback.deliver(std::move(values), std::move(failure));
    } catch (const std::exception& error) {
        core::Log::error("A call of a native callback was dropped: {}", error.what());
    }
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
                failure = "The native callback received the negative length " + std::to_string(count) + " for \"" + parameter.name + "\".";
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
