#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "haylen/2d/animation/Animation.hpp"
#include "haylen/2d/animation/SpriteAtlas.hpp"
#include "haylen/2d/animation/SpriteFrame.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/graphics/Texture.hpp"

namespace haylen::animation2d {

// Packs images and the frames of other atlases into one texture at run time, so sprites that drew from many textures draw from one and batch together. Packing is plain work on images, which runs on any thread, and only the texture of the result is made on the frame thread. Frames keep their pivots, boxes, points and events, trimming keeps where they sat in their untrimmed frame, and the animations of an added atlas come along under its prefix.
class AtlasBuilder final {
  public:
    struct Options {
        // Pixels between neighbouring images and around the edges of the texture.
        int padding = 2;

        // Pixels the edges of each image repeat outward into its padding, at most half of it, so filtered sampling at the edge of a frame never reads its neighbour.
        int extrude = 1;
        int maxSize = 4096;

        // Leaves out the transparent borders of every image.
        bool trim = true;
        graphics::Texture::Options texture{};
    };

    // The image and the frames of a packed atlas, which `finish` gives a texture.
    struct Packed {
        graphics::Image image;
        std::vector<std::pair<std::string, SpriteFrame>> frames;
        std::vector<std::pair<std::string, Animation>> animations;
    };

    // Adds an area of an image, or all of it, as a frame. Throws `std::invalid_argument` for a name given twice or an area outside the image.
    void add(std::string name, std::shared_ptr<const graphics::Image> image, std::optional<graphics::Image::Area> area = std::nullopt);

    // Adds every frame of an atlas, cut from the image of its texture, and its animations, each named with the prefix in front.
    void addAtlas(std::string_view prefix, const SpriteAtlas& atlas, std::shared_ptr<const graphics::Image> image);

    [[nodiscard]] std::size_t size() const noexcept {
        return entries.size();
    }

    // Throws `std::invalid_argument` for options that cannot pack and `std::runtime_error` when the images need more room than the largest texture.
    [[nodiscard]] Packed pack(const Options& options) const;
    [[nodiscard]] static SpriteAtlas finish(Packed packed, graphics::Texture texture);

  private:
    struct Entry {
        std::string name;
        std::shared_ptr<const graphics::Image> image;
        graphics::Image::Area area;
        SpriteFrame frame;
    };

    struct Placement {
        graphics::Image::Area area;
        int x = 0;
        int y = 0;
    };

    // One run of the skyline, the top of what is packed below it.
    struct Span {
        int x = 0;
        int y = 0;
        int width = 0;
    };

    using Key = std::tuple<const graphics::Image*, int, int, int, int>;

    [[nodiscard]] static graphics::Image::Area trimmed(const graphics::Image& image, const graphics::Image::Area& area);
    [[nodiscard]] static std::optional<int> skyline(const std::vector<graphics::Image::Area>& cells, const std::vector<std::size_t>& order, int width, std::vector<Placement>& placements);

    std::vector<Entry> entries;
    std::map<std::string, std::size_t, std::less<>> names;
    std::vector<std::pair<std::string, Animation>> animations;

    // The entry of each frame of each animation, by position.
    std::vector<std::vector<std::size_t>> animationEntries;
};

} // namespace haylen::animation2d
