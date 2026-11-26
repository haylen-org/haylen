#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "haylen/gpu/Device.hpp"

namespace haylen::graphics {

struct BufferResource;

// Shared handle to a buffer of the GPU, such as the vertices, the indices or the uniform blocks of a renderer of a plugin. The GPU buffer is released on the frame thread after the last handle goes away.
class Buffer final {
  public:
    using Usage = gpu::Device::BufferUsage;
    using Update = gpu::Device::BufferUpdate;

    // What a buffer serves as and how its contents change, with the name that debug tools and captures show.
    struct Options {
        Usage usage{};
        Update update = Update::Immutable;
        std::string_view label{};
    };

    Buffer() = default;
    explicit Buffer(std::shared_ptr<BufferResource> value) noexcept : resource(std::move(value)) {}

    // Resolves the usage names `vertex`, `index`, `uniform`, `storage`, `indirect`, `copySource` and `copyDestination`, and sets that usage.
    [[nodiscard]] static bool addUsage(Usage& usage, std::string_view name) noexcept;

    // The names of the usages that are set, in the order of `kUsageNames`.
    template <typename Visit> static void visitUsages(const Usage& usage, Visit&& visit) {
        for (const auto& [name, member] : kUsageNames) {
            if (usage.*member) {
                visit(name);
            }
        }
    }

    [[nodiscard]] bool isValid() const noexcept {
        return resource != nullptr;
    }
    [[nodiscard]] std::size_t getSize() const noexcept;
    [[nodiscard]] Usage getUsage() const noexcept;
    [[nodiscard]] Update getUpdate() const noexcept;
    [[nodiscard]] std::string_view getLabel() const noexcept;

    // The buffer of the GPU device, which renderers bind, while the handle lives.
    [[nodiscard]] gpu::Device::Buffer getGpuBuffer() const noexcept;
    [[nodiscard]] const std::shared_ptr<BufferResource>& getResource() const noexcept {
        return resource;
    }
    [[nodiscard]] bool operator==(const Buffer& other) const noexcept {
        return resource == other.resource;
    }

  private:
    static constexpr std::array<std::pair<std::string_view, bool Usage::*>, 7> kUsageNames{{
        {"vertex", &Usage::vertex},
        {"index", &Usage::index},
        {"uniform", &Usage::uniform},
        {"storage", &Usage::storage},
        {"indirect", &Usage::indirect},
        {"copySource", &Usage::copySource},
        {"copyDestination", &Usage::copyDestination},
    }};

    std::shared_ptr<BufferResource> resource;
};

} // namespace haylen::graphics
