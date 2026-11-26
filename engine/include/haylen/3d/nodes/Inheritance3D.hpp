#pragma once

namespace haylen::nodes3d {

// Which parts of the world transform of its parent a node takes. A part that the node does not take is a part of the world: its local position, rotation or scale is then where it sits, how it turns or how large it is in the world. A node that takes none of them ignores its parent and still belongs to its subtree.
struct Inheritance3D {
    bool position = true;
    bool rotation = true;
    bool scale = true;

    [[nodiscard]] constexpr bool isFull() const noexcept {
        return position && rotation && scale;
    }

    [[nodiscard]] constexpr bool operator==(const Inheritance3D&) const noexcept = default;
};

} // namespace haylen::nodes3d
