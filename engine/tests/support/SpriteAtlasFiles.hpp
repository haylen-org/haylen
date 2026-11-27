#pragma once

namespace haylen::test {

// The document of a sprite atlas with frames, tags and slices, which the tests of atlases in C++ and in Lua load.
class SpriteAtlasFiles final {
  public:
    // clang-format off
    static constexpr const char* kTaggedAtlas = R"({
        "frames": [
            {"filename": "hero 0", "frame": {"x": 0, "y": 0, "w": 8, "h": 16}, "rotated": false, "trimmed": true, "spriteSourceSize": {"x": 4, "y": 0, "w": 8, "h": 16}, "sourceSize": {"w": 16, "h": 16}, "duration": 100},
            {"filename": "hero 1", "frame": {"x": 8, "y": 0, "w": 16, "h": 16}, "duration": 200},
            {"filename": "hero 2", "frame": {"x": 24, "y": 0, "w": 16, "h": 16}, "duration": 100},
            {"filename": "panel", "frame": {"x": 0, "y": 16, "w": 32, "h": 16}, "duration": 100}
        ],
        "meta": {
            "image": "hero.png",
            "frameTags": [
                {"name": "walk", "from": 0, "to": 2, "direction": "forward"},
                {"name": "back", "from": 0, "to": 2, "direction": "reverse"},
                {"name": "bounce", "from": 0, "to": 2, "direction": "pingpong"},
                {"name": "hit", "from": 1, "to": 2, "direction": "forward", "repeat": "1"}
            ],
            "slices": [
                {"name": "panel", "keys": [{"frame": 3, "bounds": {"x": 0, "y": 0, "w": 32, "h": 16}, "center": {"x": 4, "y": 4, "w": 24, "h": 8}}]},
                {"name": "marker", "keys": [{"frame": 0, "bounds": {"x": 0, "y": 0, "w": 2, "h": 2}}]}
            ]
        }
    })";
    // clang-format on
};

} // namespace haylen::test
