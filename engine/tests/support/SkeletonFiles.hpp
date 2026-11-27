#pragma once

namespace haylen::test {

// The documents of a sprite atlas and of a skeleton that its skin draws from, which the tests of skeletons in C++ and in Lua load.
class SkeletonFiles final {
  public:
    // clang-format off
    static constexpr const char* kAtlas = R"({
        "frames": {
            "body": {"frame": {"x": 0, "y": 0, "w": 32, "h": 64}},
            "arm": {"frame": {"x": 32, "y": 0, "w": 32, "h": 8}},
            "blade": {"frame": {"x": 32, "y": 8, "w": 32, "h": 8}},
            "cape": {"frame": {"x": 0, "y": 64, "w": 32, "h": 32}}
        },
        "meta": {"image": "hero.png"}
    })";
    // clang-format on

    // A body with an upper arm and a forearm, a sword skin, a cape mesh, an aim constraint and three animations.
    // clang-format off
    static constexpr const char* kSkeleton = R"({
        "atlas": "hero.json",
        "bones": [
            {"name": "root"},
            {"name": "body", "parent": "root", "y": -32, "length": 32},
            {"name": "upper", "parent": "body", "x": 10, "y": -20, "rotation": 90, "length": 30},
            {"name": "lower", "parent": "upper", "x": 30, "length": 30},
            {"name": "aim", "parent": "root", "x": 100}
        ],
        "slots": [
            {"name": "body", "bone": "body", "attachment": "body"},
            {"name": "cape", "bone": "body", "attachment": "cape"},
            {"name": "hand", "bone": "lower", "attachment": "fist"},
            {"name": "tip", "bone": "lower", "attachment": "tip"}
        ],
        "skins": {
            "default": {
                "body": {"body": {}},
                "cape": {"cape": {"type": "mesh", "vertices": [0, 0, 10, 0, 10, 10, 0, 10], "uvs": [0, 0, 1, 0, 1, 1, 0, 1], "triangles": [0, 1, 2, 0, 2, 3]}},
                "hand": {"fist": {"frame": "arm"}},
                "tip": {"tip": {"type": "point", "x": 30}}
            },
            "sword": {"hand": {"fist": {"frame": "blade"}}}
        },
        "ik": [{"name": "reach", "bones": ["upper", "lower"], "target": "aim", "mix": 0}],
        "animations": {
            "wave": {"bones": {"upper": {"rotate": [{"time": 0, "value": 0}, {"time": 1, "value": -90}]}}, "events": [{"time": 0.5, "name": "step", "text": "left", "number": 2}]},
            "lift": {"bones": {"body": {"translate": [{"time": 0, "x": 0, "y": 0}, {"time": 1, "x": 0, "y": -10}]}}},
            "flap": {"deform": {"cape": {"cape": [{"time": 0, "offsets": [0, 0, 0, 0, 0, 0, 0, 0]}, {"time": 1, "offsets": [0, 0, 4, 0, 4, 0, 0, 0]}]}}, "slots": {"hand": {"attachment": [{"time": 0, "name": "fist"}, {"time": 0.5, "name": ""}]}}}
        }
    })";
    // clang-format on
};

} // namespace haylen::test
