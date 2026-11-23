#include "graphics/ImageBinding.hpp"

#include <lua.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/ImagePalette.hpp"
#include "haylen/graphics/ImagePlaceholder.hpp"
#include "haylen/graphics/RenderTarget.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/ScriptPlugin.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "varn/async/Promise.h"

namespace haylen::lua::graphics {

void ImageBinding::push(lua_State* L, haylen::graphics::Image image) {
    lua::Userdata::emplace<haylen::graphics::Image>(L, std::make_shared<haylen::graphics::Image>(std::move(image)));
}

haylen::graphics::Image& ImageBinding::checkWritable(lua_State* L, int index) {
    auto& storage = *static_cast<lua::Type<haylen::graphics::Image>::Storage*>(luaL_checkudata(L, index, lua::Type<haylen::graphics::Image>::name));
    if (storage.use_count() > 1) {
        storage = std::make_shared<haylen::graphics::Image>(*storage);
    }

    return *storage;
}

// Reads a rectangle into whole pixels, or the whole image for `nil`. A corner or a size past the pixels that whole numbers reach is an error, since no image reaches it.
haylen::graphics::Image::Area ImageBinding::readArea(lua_State* L, int index, const haylen::graphics::Image& image) {
    if (lua_isnoneornil(L, index)) {
        return image.getArea();
    }

    const auto area = lua::Stack::read<haylen::math::Rect>(L, index);
    const std::array<float, 4> values{area.x, area.y, area.width, area.height};
    if (std::ranges::any_of(values, [](float value) { return !std::isfinite(value) || std::fabs(value) > kMaxAreaPixels; })) {
        throw std::invalid_argument("The corner and the size of an area of an image are numbers of pixels from -1073741824 to 1073741824.");
    }

    return {static_cast<int>(std::lround(area.x)), static_cast<int>(std::lround(area.y)), static_cast<int>(std::lround(area.width)), static_cast<int>(std::lround(area.height))};
}

int ImageBinding::create(lua_State* L) {
    const auto columns = lua::Stack::read<int>(L, 1);
    const auto rows = lua::Stack::read<int>(L, 2);
    if (columns < 1 || rows < 1) {
        throw std::invalid_argument("An image needs a width and a height of one pixel or more.");
    }

    push(L, haylen::graphics::Image(columns, rows, lua_isnoneornil(L, 3) ? haylen::math::Color::transparent() : lua::Stack::read<haylen::math::Color>(L, 3)));
    return 1;
}

int ImageBinding::maxSize(lua_State* L) {
    lua::Stack::push(L, haylen::graphics::Image::kMaxSide);
    return 1;
}

int ImageBinding::readMaxSide(lua_State* L, int index) {
    int maxSide = haylen::graphics::Image::kMaxSide;
    if (!lua_isnoneornil(L, index)) {
        luaL_checktype(L, index, LUA_TTABLE);
        lua::Table::checkFields(L, index, {kDecodeFields});
        lua::Table::readField(L, index, "maxSize", maxSide);
    }

    if (maxSide < 1 || maxSide > haylen::graphics::Image::kMaxSide) {
        throw std::invalid_argument("The option \"maxSize\" of a decode is from 1 to " + std::to_string(haylen::graphics::Image::kMaxSide) + " pixels.");
    }

    return maxSide;
}

int ImageBinding::decode(lua_State* L) {
    const auto bytes = lua::Stack::read<std::string_view>(L, 1);
    push(L, haylen::graphics::Image::decode({reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()}, readMaxSide(L, 2)));
    return 1;
}

int ImageBinding::decodeAsync(lua_State* L) {
    std::string bytes(lua::Stack::read<std::string_view>(L, 1));
    const int maxSide = readMaxSide(L, 2);
    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    auto promise = std::make_shared<varn::async::Promise>(engine.getPlugin<lua::ScriptPlugin>().getRuntime());
    // clang-format off
    engine.getJobs().run([bytes = std::move(bytes), maxSide] {
        return haylen::graphics::Image::decode({reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()}, maxSide);
    }, [promise](haylen::core::JobSystem::Result<haylen::graphics::Image> result) {
        if (!result.isOk()) {
            promise->reject(result.error);
            return;
        }

        auto image = std::make_shared<haylen::graphics::Image>(std::move(*result.value));
        promise->resolveCustom([image](lua_State* state) { lua::Userdata::emplace<haylen::graphics::Image>(state, image); });
    });
    // clang-format on
    varn::async::Promise::push(L, promise);
    return 1;
}

int ImageBinding::pixel(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics::Image>(L, 1).getPixel(lua::Stack::read<int>(L, 2), lua::Stack::read<int>(L, 3)));
    return 1;
}

int ImageBinding::setPixel(lua_State* L) {
    checkWritable(L, 1).setPixel(lua::Stack::read<int>(L, 2), lua::Stack::read<int>(L, 3), lua::Stack::read<haylen::math::Color>(L, 4));
    return 0;
}

// Fills with `fill(color, rect)`, the whole image without a rectangle.
int ImageBinding::fill(lua_State* L) {
    haylen::graphics::Image& image = checkWritable(L, 1);
    image.fill(readArea(L, 3, image), lua::Stack::read<haylen::math::Color>(L, 2));
    return 0;
}

// Paints a gradient over an area with `fillGradient({kind, stops, colors, angle, center, radius, dither}, rect)`, the whole image without a rectangle.
int ImageBinding::fillGradient(lua_State* L) {
    const haylen::graphics::Gradient gradient = readGradient(L, 2);
    haylen::graphics::Image& image = checkWritable(L, 1);
    gradient.paint(image, readArea(L, 3, image));
    return 0;
}

// Reads the stops as a list of offset and color pairs, or the colors spread evenly, but never both.
haylen::graphics::Gradient ImageBinding::readGradient(lua_State* L, int index) {
    luaL_checktype(L, index, LUA_TTABLE);
    haylen::graphics::Gradient::Kind kind = haylen::graphics::Gradient::Kind::Linear;
    std::optional<std::vector<haylen::graphics::Gradient::Stop>> stops;
    std::optional<std::vector<haylen::math::Color>> colors;
    float angle = 0.0F;
    haylen::math::Vec2 center{0.5F, 0.5F};
    std::optional<haylen::math::Vec2> radius;
    bool dither = true;
    // clang-format off
    lua::Table::readFields(L, index, kGradientFields, {}, [&](std::string_view key) {
        if (key == "kind") {
            lua::Table::readValue(L, key, kind);
        } else if (key == "stops") {
            readStops(L, stops.emplace());
        } else if (key == "colors") {
            lua::Table::readValue(L, key, colors.emplace());
        } else if (key == "angle") {
            lua::Table::readValue(L, key, angle);
        } else if (key == "center") {
            lua::Table::readValue(L, key, center);
        } else if (key == "radius") {
            if (lua_type(L, -1) == LUA_TNUMBER) {
                const auto both = lua::Converter<float>::fromNumber(L, lua_tonumber(L, -1));
                radius = haylen::math::Vec2{both, both};
            } else {
                lua::Table::readValue(L, key, radius.emplace());
            }
        } else if (key == "dither") {
            lua::Table::readValue(L, key, dither);
        }
    });
    // clang-format on
    if (stops.has_value() == colors.has_value()) {
        throw std::invalid_argument("A gradient takes its colors from either \"stops\" or \"colors\".");
    }

    haylen::graphics::Gradient gradient = stops ? haylen::graphics::Gradient(std::move(*stops), kind) : haylen::graphics::Gradient::fromColors(*colors, kind);
    gradient.setAngle(angle);
    gradient.setCenter(center);
    if (radius) {
        gradient.setRadius(*radius);
    }

    gradient.setDithered(dither);
    return gradient;
}

void ImageBinding::readStops(lua_State* L, std::vector<haylen::graphics::Gradient::Stop>& stops) {
    if (!lua_istable(L, -1)) {
        throw std::invalid_argument("The stops of a gradient are a list of offset and color pairs, such as {{0, '#FF000000'}, {1, '#FFFFFFFF'}}.");
    }

    const auto count = static_cast<lua_Integer>(lua_rawlen(L, -1));
    for (lua_Integer index = 1; index <= count; ++index) {
        lua_rawgeti(L, -1, index);
        if (!lua_istable(L, -1) || lua_rawlen(L, -1) != 2) {
            throw std::invalid_argument("The stops of a gradient are a list of offset and color pairs, such as {{0, '#FF000000'}, {1, '#FFFFFFFF'}}.");
        }

        haylen::graphics::Gradient::Stop stop;
        lua_rawgeti(L, -1, 1);
        lua::Table::readValue(L, "stops", stop.offset);
        lua_rawgeti(L, -2, 2);
        lua::Table::readValue(L, "stops", stop.color);
        lua_pop(L, 3);
        stops.push_back(stop);
    }
}

// Draws another image into this one with `blit(source, x, y, {source = rect, blend = false})`.
int ImageBinding::blit(lua_State* L) {
    const std::shared_ptr<haylen::graphics::Image> source = lua::Userdata::checkShared<haylen::graphics::Image>(L, 2);
    haylen::graphics::Image& image = checkWritable(L, 1);
    haylen::graphics::Image::Area from = source->getArea();
    bool blend = false;
    if (!lua_isnoneornil(L, 5)) {
        luaL_checktype(L, 5, LUA_TTABLE);
        lua::Table::checkFields(L, 5, {kBlitFields});
        lua_getfield(L, 5, "source");
        from = readArea(L, -1, *source);
        lua_pop(L, 1);
        lua::Table::readField(L, 5, "blend", blend);
    }

    image.blit(*source, from, lua::Stack::read<int>(L, 3), lua::Stack::read<int>(L, 4), blend ? haylen::graphics::Image::BlitMode::Blend : haylen::graphics::Image::BlitMode::Replace);
    return 0;
}

// Returns a promise of a copy of this image with the source drawn into it, both made on the task pool, while this image stays as it is.
int ImageBinding::blitAsync(lua_State* L) {
    const std::shared_ptr<const haylen::graphics::Image> target = lua::Userdata::checkShared<haylen::graphics::Image>(L, 1);
    const std::shared_ptr<const haylen::graphics::Image> source = lua::Userdata::checkShared<haylen::graphics::Image>(L, 2);
    haylen::graphics::Image::Area from = source->getArea();
    bool blend = false;
    if (!lua_isnoneornil(L, 5)) {
        luaL_checktype(L, 5, LUA_TTABLE);
        lua::Table::checkFields(L, 5, {kBlitFields});
        lua_getfield(L, 5, "source");
        from = readArea(L, -1, *source);
        lua_pop(L, 1);
        lua::Table::readField(L, 5, "blend", blend);
    }

    const auto x = lua::Stack::read<int>(L, 3);
    const auto y = lua::Stack::read<int>(L, 4);
    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    auto promise = std::make_shared<varn::async::Promise>(engine.getPlugin<lua::ScriptPlugin>().getRuntime());
    // clang-format off
    engine.getJobs().run([target, source, from, x, y, blend] {
        haylen::graphics::Image result = *target;
        result.blit(*source, from, x, y, blend ? haylen::graphics::Image::BlitMode::Blend : haylen::graphics::Image::BlitMode::Replace);
        return result;
    }, [promise](haylen::core::JobSystem::Result<haylen::graphics::Image> result) {
        if (!result.isOk()) {
            promise->reject(result.error);
            return;
        }

        auto image = std::make_shared<haylen::graphics::Image>(std::move(*result.value));
        promise->resolveCustom([image](lua_State* state) { lua::Userdata::emplace<haylen::graphics::Image>(state, image); });
    });
    // clang-format on
    varn::async::Promise::push(L, promise);
    return 1;
}

int ImageBinding::crop(lua_State* L) {
    const haylen::graphics::Image& image = lua::Userdata::check<haylen::graphics::Image>(L, 1);
    push(L, image.crop(readArea(L, 2, image)));
    return 1;
}

int ImageBinding::bleed(lua_State* L) {
    checkWritable(L, 1).bleed();
    return 0;
}

int ImageBinding::visibleArea(lua_State* L) {
    const haylen::graphics::Image::Area area = lua::Userdata::check<haylen::graphics::Image>(L, 1).getVisibleArea();
    if (area.width == 0) {
        lua_pushnil(L);
        return 1;
    }

    lua::Stack::push(L, haylen::math::Rect{static_cast<float>(area.x), static_cast<float>(area.y), static_cast<float>(area.width), static_cast<float>(area.height)});
    return 1;
}

int ImageBinding::clone(lua_State* L) {
    push(L, lua::Userdata::check<haylen::graphics::Image>(L, 1));
    return 1;
}

int ImageBinding::encodePng(lua_State* L) {
    const std::vector<std::uint8_t> encoded = lua::Userdata::check<haylen::graphics::Image>(L, 1).encodePng();
    lua_pushlstring(L, reinterpret_cast<const char*>(encoded.data()), encoded.size());
    return 1;
}

// Encodes on the task pool, from the image as it is now, and returns a promise of the bytes.
int ImageBinding::encodePngAsync(lua_State* L) {
    const std::shared_ptr<const haylen::graphics::Image> image = lua::Userdata::checkShared<haylen::graphics::Image>(L, 1);
    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    auto promise = std::make_shared<varn::async::Promise>(engine.getPlugin<lua::ScriptPlugin>().getRuntime());
    // clang-format off
    engine.getJobs().run([image] { return image->encodePng(); }, [promise](haylen::core::JobSystem::Result<std::vector<std::uint8_t>> result) {
        if (!result.isOk()) {
            promise->reject(result.error);
            return;
        }

        auto bytes = std::make_shared<std::vector<std::uint8_t>>(std::move(*result.value));
        promise->resolveCustom([bytes](lua_State* state) { lua_pushlstring(state, reinterpret_cast<const char*>(bytes->data()), bytes->size()); });
    });
    // clang-format on
    varn::async::Promise::push(L, promise);
    return 1;
}

// Both promises resolve with the image once the GPU copied it and the task pool straightened it.
int ImageBinding::readTarget(lua_State* L) {
    const haylen::graphics::RenderTarget target = lua::Userdata::check<haylen::graphics::RenderTarget>(L, 1);
    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    auto promise = std::make_shared<varn::async::Promise>(engine.getPlugin<lua::ScriptPlugin>().getRuntime());
    engine.getGraphics().readTarget(target, settle(promise));
    varn::async::Promise::push(L, promise);
    return 1;
}

int ImageBinding::captureScreen(lua_State* L) {
    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    auto promise = std::make_shared<varn::async::Promise>(engine.getPlugin<lua::ScriptPlugin>().getRuntime());
    engine.captureScreen(settle(promise));
    varn::async::Promise::push(L, promise);
    return 1;
}

haylen::graphics::Device::ReadCallback ImageBinding::settle(const std::shared_ptr<varn::async::Promise>& promise) {
    // clang-format off
    return [promise](std::optional<haylen::graphics::Image> image, const std::string& error) {
        if (!image) {
            promise->reject(error);
            return;
        }

        auto shared = std::make_shared<haylen::graphics::Image>(std::move(*image));
        promise->resolveCustom([shared](lua_State* state) { lua::Userdata::emplace<haylen::graphics::Image>(state, shared); });
    };
    // clang-format on
}

int ImageBinding::palette(lua_State* L) {
    const haylen::graphics::Image& image = lua::Userdata::check<haylen::graphics::Image>(L, 1);
    const std::vector<haylen::graphics::ImagePalette::Swatch> colors = haylen::graphics::ImagePalette::extract(image, luaL_optinteger(L, 2, kPaletteColors));
    lua_createtable(L, static_cast<int>(colors.size()), 0);
    for (std::size_t index = 0; index < colors.size(); ++index) {
        lua_createtable(L, 0, 2);
        lua::Stack::push(L, colors[index].color);
        lua_setfield(L, -2, "color");
        lua::Stack::push(L, colors[index].population);
        lua_setfield(L, -2, "population");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    return 1;
}

int ImageBinding::swatches(lua_State* L) {
    const haylen::graphics::Image& image = lua::Userdata::check<haylen::graphics::Image>(L, 1);
    const haylen::graphics::ImagePalette::Swatches found = haylen::graphics::ImagePalette::choose(haylen::graphics::ImagePalette::extract(image, luaL_optinteger(L, 2, kSwatchColors)));
    lua_createtable(L, 0, 7);
    lua::Stack::push(L, found.dominant);
    lua_setfield(L, -2, "dominant");
    for (const auto& [name, color] : {std::pair{"vibrant", found.vibrant}, std::pair{"darkVibrant", found.darkVibrant}, std::pair{"lightVibrant", found.lightVibrant}, std::pair{"muted", found.muted}, std::pair{"darkMuted", found.darkMuted}, std::pair{"lightMuted", found.lightMuted}}) {
        if (color) {
            lua::Stack::push(L, *color);
            lua_setfield(L, -2, name);
        }
    }

    return 1;
}

int ImageBinding::placeholder(lua_State* L) {
    const haylen::graphics::Image& image = lua::Userdata::check<haylen::graphics::Image>(L, 1);
    haylen::graphics::ImagePlaceholder::Options options;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kPlaceholderFields});
        lua::Table::readField(L, 2, "columns", options.columns);
        lua::Table::readField(L, 2, "rows", options.rows);
    }

    lua::Stack::push(L, haylen::graphics::ImagePlaceholder::encode(image, options));
    return 1;
}

int ImageBinding::blur(lua_State* L) {
    haylen::graphics::ImageFilters::blur(checkWritable(L, 1), lua::Stack::read<float>(L, 2));
    return 0;
}

int ImageBinding::sharpen(lua_State* L) {
    haylen::graphics::ImageFilters::sharpen(checkWritable(L, 1), lua::Converter<float>::readOr(L, 2, 1.0));
    return 0;
}

haylen::graphics::ImageFilters::Adjustment ImageBinding::readAdjustment(lua_State* L, int index) {
    haylen::graphics::ImageFilters::Adjustment adjustment;
    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kAdjustFields});
    lua::Table::readField(L, index, "brightness", adjustment.brightness);
    lua::Table::readField(L, index, "contrast", adjustment.contrast);
    lua::Table::readField(L, index, "saturation", adjustment.saturation);
    lua::Table::readField(L, index, "hue", adjustment.hue);
    return adjustment;
}

int ImageBinding::adjust(lua_State* L) {
    haylen::graphics::Image& image = checkWritable(L, 1);
    haylen::graphics::ImageFilters::adjust(image, readAdjustment(L, 2));
    return 0;
}

int ImageBinding::grayscale(lua_State* L) {
    haylen::graphics::ImageFilters::grayscale(checkWritable(L, 1), lua::Converter<float>::readOr(L, 2, 1.0));
    return 0;
}

int ImageBinding::invert(lua_State* L) {
    haylen::graphics::ImageFilters::invert(checkWritable(L, 1));
    return 0;
}

int ImageBinding::tint(lua_State* L) {
    haylen::graphics::Image& image = checkWritable(L, 1);
    haylen::graphics::ImageFilters::tint(image, lua::Stack::read<haylen::math::Color>(L, 2), lua::Converter<float>::readOr(L, 3, 1.0));
    return 0;
}

int ImageBinding::pixelate(lua_State* L) {
    haylen::graphics::ImageFilters::pixelate(checkWritable(L, 1), lua::Stack::read<int>(L, 2));
    return 0;
}

int ImageBinding::outline(lua_State* L) {
    haylen::graphics::Image& image = checkWritable(L, 1);
    haylen::graphics::ImageFilters::outline(image, lua::Stack::read<haylen::math::Color>(L, 2), lua::Converter<float>::readOr(L, 3, 1.0));
    return 0;
}

// Every effect reads its options here on the frame thread, and its step runs on the task pool, where a value out of range rejects the promise.
std::function<void(haylen::graphics::Image&)> ImageBinding::readEffect(lua_State* L, int index) {
    luaL_checktype(L, index, LUA_TTABLE);
    lua_getfield(L, index, "effect");
    const std::string effect = luaL_checkstring(L, -1);
    lua_pop(L, 1);
    // clang-format off
    const auto number = [L, index](const char* key, float fallback) {
        float value = fallback;
        lua::Table::readField(L, index, key, value);
        return value;
    };
    const auto color = [L, index](const char* key) {
        lua_getfield(L, index, key);
        const haylen::math::Color value = lua::Stack::read<haylen::math::Color>(L, -1);
        lua_pop(L, 1);
        return value;
    };
    // clang-format on
    if (effect == "blur") {
        lua::Table::checkFields(L, index, {kBlurEffectFields});
        return [radius = number("radius", 0.0F)](haylen::graphics::Image& image) { haylen::graphics::ImageFilters::blur(image, radius); };
    }

    if (effect == "sharpen") {
        lua::Table::checkFields(L, index, {kAmountEffectFields});
        return [amount = number("amount", 1.0F)](haylen::graphics::Image& image) { haylen::graphics::ImageFilters::sharpen(image, amount); };
    }

    if (effect == "adjust") {
        lua::Table::checkFields(L, index, {kAdjustEffectFields});
        haylen::graphics::ImageFilters::Adjustment adjustment{.brightness = number("brightness", 0.0F), .contrast = number("contrast", 1.0F), .saturation = number("saturation", 1.0F), .hue = number("hue", 0.0F)};
        return [adjustment](haylen::graphics::Image& image) { haylen::graphics::ImageFilters::adjust(image, adjustment); };
    }

    if (effect == "grayscale") {
        lua::Table::checkFields(L, index, {kAmountEffectFields});
        return [amount = number("amount", 1.0F)](haylen::graphics::Image& image) { haylen::graphics::ImageFilters::grayscale(image, amount); };
    }

    if (effect == "invert") {
        lua::Table::checkFields(L, index, {kInvertEffectFields});
        return [](haylen::graphics::Image& image) { haylen::graphics::ImageFilters::invert(image); };
    }

    if (effect == "tint") {
        lua::Table::checkFields(L, index, {kTintEffectFields});
        return [tinted = color("color"), amount = number("amount", 1.0F)](haylen::graphics::Image& image) { haylen::graphics::ImageFilters::tint(image, tinted, amount); };
    }

    if (effect == "pixelate") {
        lua::Table::checkFields(L, index, {kPixelateEffectFields});
        int size = 1;
        lua::Table::readField(L, index, "size", size);
        return [size](haylen::graphics::Image& image) { haylen::graphics::ImageFilters::pixelate(image, size); };
    }

    if (effect == "outline") {
        lua::Table::checkFields(L, index, {kOutlineEffectFields});
        return [outlined = color("color"), thickness = number("thickness", 1.0F)](haylen::graphics::Image& image) { haylen::graphics::ImageFilters::outline(image, outlined, thickness); };
    }

    luaL_error(L, "The image effect \"%s\" is unknown. It is \"blur\", \"sharpen\", \"adjust\", \"grayscale\", \"invert\", \"tint\", \"pixelate\" or \"outline\".", effect.c_str());
    return {};
}

// Returns a promise of a copy of this image with the effects of the list applied in order on the task pool, while this image stays as it is.
int ImageBinding::applyAsync(lua_State* L) {
    const std::shared_ptr<const haylen::graphics::Image> source = lua::Userdata::checkShared<haylen::graphics::Image>(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    std::vector<std::function<void(haylen::graphics::Image&)>> steps;
    for (lua_Integer index = 1; lua_rawgeti(L, 2, index) != LUA_TNIL; ++index) {
        steps.push_back(readEffect(L, lua_gettop(L)));
        lua_pop(L, 1);
    }

    lua_pop(L, 1);
    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    auto promise = std::make_shared<varn::async::Promise>(engine.getPlugin<lua::ScriptPlugin>().getRuntime());
    // clang-format off
    engine.getJobs().run([source, steps = std::move(steps)] {
        haylen::graphics::Image result = *source;
        for (const std::function<void(haylen::graphics::Image&)>& step : steps) {
            step(result);
        }

        return result;
    }, [promise](haylen::core::JobSystem::Result<haylen::graphics::Image> result) {
        if (!result.isOk()) {
            promise->reject(result.error);
            return;
        }

        auto image = std::make_shared<haylen::graphics::Image>(std::move(*result.value));
        promise->resolveCustom([image](lua_State* state) { lua::Userdata::emplace<haylen::graphics::Image>(state, image); });
    });
    // clang-format on
    varn::async::Promise::push(L, promise);
    return 1;
}

int ImageBinding::decodePlaceholder(lua_State* L) {
    push(L, haylen::graphics::ImagePlaceholder::decode(lua::Stack::read<std::string_view>(L, 1), lua::Stack::read<int>(L, 2), lua::Stack::read<int>(L, 3)));
    return 1;
}

int ImageBinding::width(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics::Image>(L, 1).getWidth());
    return 1;
}

int ImageBinding::height(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics::Image>(L, 1).getHeight());
    return 1;
}

void ImageBinding::install(lua_State* L) {
    // clang-format off
    lua::ClassBuilder<haylen::graphics::Image>(L)
        .function("pixel", &lua::Binding::native<&pixel>)
        .function("setPixel", &lua::Binding::native<&setPixel>)
        .function("fill", &lua::Binding::native<&fill>)
        .function("fillGradient", &lua::Binding::native<&fillGradient>)
        .function("blit", &lua::Binding::native<&blit>)
        .function("blitAsync", &lua::Binding::native<&blitAsync>)
        .function("crop", &lua::Binding::native<&crop>)
        .function("bleed", &lua::Binding::native<&bleed>)
        .function("visibleArea", &lua::Binding::native<&visibleArea>)
        .function("clone", &lua::Binding::native<&clone>)
        .function("encodePng", &lua::Binding::native<&encodePng>)
        .function("encodePngAsync", &lua::Binding::native<&encodePngAsync>)
        .function("palette", &lua::Binding::native<&palette>)
        .function("swatches", &lua::Binding::native<&swatches>)
        .function("placeholder", &lua::Binding::native<&placeholder>)
        .function("blur", &lua::Binding::native<&blur>)
        .function("sharpen", &lua::Binding::native<&sharpen>)
        .function("adjust", &lua::Binding::native<&adjust>)
        .function("grayscale", &lua::Binding::native<&grayscale>)
        .function("invert", &lua::Binding::native<&invert>)
        .function("tint", &lua::Binding::native<&tint>)
        .function("pixelate", &lua::Binding::native<&pixelate>)
        .function("outline", &lua::Binding::native<&outline>)
        .function("applyAsync", &lua::Binding::native<&applyAsync>)
        .property("width", &width)
        .property("height", &height)
        .install();
    // clang-format on
}

} // namespace haylen::lua::graphics
