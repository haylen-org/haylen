#include "graphics/ImageLua.hpp"

#include <lua.hpp>

#include <cmath>
#include <cstddef>
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
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "varn/async/Promise.h"

namespace haylen::graphics {

void ImageLua::push(lua_State* L, Image image) {
    lua::Userdata::emplace<Image>(L, std::make_shared<Image>(std::move(image)));
}

Image& ImageLua::checkWritable(lua_State* L, int index) {
    auto& storage = *static_cast<lua::Type<Image>::Storage*>(luaL_checkudata(L, index, lua::Type<Image>::name));
    if (storage.use_count() > 1) {
        storage = std::make_shared<Image>(*storage);
    }
    return *storage;
}

// Reads a rectangle into whole pixels, or the whole image for `nil`.
Image::Area ImageLua::readArea(lua_State* L, int index, const Image& image) {
    if (lua_isnoneornil(L, index)) {
        return image.getArea();
    }
    const auto area = lua::Stack::read<math::Rect>(L, index);
    return {static_cast<int>(std::lround(area.x)), static_cast<int>(std::lround(area.y)), static_cast<int>(std::lround(area.width)), static_cast<int>(std::lround(area.height))};
}

int ImageLua::create(lua_State* L) {
    const auto columns = static_cast<int>(luaL_checkinteger(L, 1));
    const auto rows = static_cast<int>(luaL_checkinteger(L, 2));
    if (columns < 1 || rows < 1) {
        throw std::invalid_argument("An image needs a width and a height of one pixel or more.");
    }
    push(L, Image(columns, rows, lua_isnoneornil(L, 3) ? math::Color::transparent() : lua::Stack::read<math::Color>(L, 3)));
    return 1;
}

int ImageLua::decode(lua_State* L) {
    const auto bytes = lua::Stack::read<std::string_view>(L, 1);
    push(L, Image::decode({reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()}));
    return 1;
}

int ImageLua::decodeAsync(lua_State* L) {
    std::string bytes(lua::Stack::read<std::string_view>(L, 1));
    core::Engine& engine = lua::Runtime::getEngine(L);
    auto promise = std::make_shared<varn::async::Promise>(engine.getScriptRuntime());
    // clang-format off
    engine.getJobs().run([bytes = std::move(bytes)] {
        return Image::decode({reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()});
    }, [promise](core::JobSystem::Result<Image> result) {
        if (!result.isOk()) {
            promise->reject(result.error);
            return;
        }
        auto image = std::make_shared<Image>(std::move(*result.value));
        promise->resolveCustom([image](lua_State* state) { lua::Userdata::emplace<Image>(state, image); });
    });
    // clang-format on
    varn::async::Promise::push(L, promise);
    return 1;
}

int ImageLua::pixel(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Image>(L, 1).getPixel(static_cast<int>(luaL_checkinteger(L, 2)), static_cast<int>(luaL_checkinteger(L, 3))));
    return 1;
}

int ImageLua::setPixel(lua_State* L) {
    checkWritable(L, 1).setPixel(static_cast<int>(luaL_checkinteger(L, 2)), static_cast<int>(luaL_checkinteger(L, 3)), lua::Stack::read<math::Color>(L, 4));
    return 0;
}

// Fills with `fill(color, rect)`, the whole image without a rectangle.
int ImageLua::fill(lua_State* L) {
    Image& image = checkWritable(L, 1);
    image.fill(readArea(L, 3, image), lua::Stack::read<math::Color>(L, 2));
    return 0;
}

// Draws another image into this one with `blit(source, x, y, {source = rect, blend = false})`.
int ImageLua::blit(lua_State* L) {
    const std::shared_ptr<Image> source = lua::Userdata::checkShared<Image>(L, 2);
    Image& image = checkWritable(L, 1);
    Image::Area from = source->getArea();
    bool blend = false;
    if (!lua_isnoneornil(L, 5)) {
        luaL_checktype(L, 5, LUA_TTABLE);
        lua::Table::checkFields(L, 5, {kBlitFields});
        lua_getfield(L, 5, "source");
        from = readArea(L, -1, *source);
        lua_pop(L, 1);
        lua::Table::readField(L, 5, "blend", blend);
    }
    image.blit(*source, from, static_cast<int>(luaL_checkinteger(L, 3)), static_cast<int>(luaL_checkinteger(L, 4)), blend ? Image::BlitMode::Blend : Image::BlitMode::Replace);
    return 0;
}

// Returns a promise of a copy of this image with the source drawn into it, both made on the task pool, while this image stays as it is.
int ImageLua::blitAsync(lua_State* L) {
    const std::shared_ptr<const Image> target = lua::Userdata::checkShared<Image>(L, 1);
    const std::shared_ptr<const Image> source = lua::Userdata::checkShared<Image>(L, 2);
    Image::Area from = source->getArea();
    bool blend = false;
    if (!lua_isnoneornil(L, 5)) {
        luaL_checktype(L, 5, LUA_TTABLE);
        lua::Table::checkFields(L, 5, {kBlitFields});
        lua_getfield(L, 5, "source");
        from = readArea(L, -1, *source);
        lua_pop(L, 1);
        lua::Table::readField(L, 5, "blend", blend);
    }
    const auto x = static_cast<int>(luaL_checkinteger(L, 3));
    const auto y = static_cast<int>(luaL_checkinteger(L, 4));
    core::Engine& engine = lua::Runtime::getEngine(L);
    auto promise = std::make_shared<varn::async::Promise>(engine.getScriptRuntime());
    // clang-format off
    engine.getJobs().run([target, source, from, x, y, blend] {
        Image result = *target;
        result.blit(*source, from, x, y, blend ? Image::BlitMode::Blend : Image::BlitMode::Replace);
        return result;
    }, [promise](core::JobSystem::Result<Image> result) {
        if (!result.isOk()) {
            promise->reject(result.error);
            return;
        }
        auto image = std::make_shared<Image>(std::move(*result.value));
        promise->resolveCustom([image](lua_State* state) { lua::Userdata::emplace<Image>(state, image); });
    });
    // clang-format on
    varn::async::Promise::push(L, promise);
    return 1;
}

int ImageLua::crop(lua_State* L) {
    const Image& image = lua::Userdata::check<Image>(L, 1);
    push(L, image.crop(readArea(L, 2, image)));
    return 1;
}

int ImageLua::bleed(lua_State* L) {
    checkWritable(L, 1).bleed();
    return 0;
}

int ImageLua::visibleArea(lua_State* L) {
    const Image::Area area = lua::Userdata::check<Image>(L, 1).getVisibleArea();
    if (area.width == 0) {
        lua_pushnil(L);
        return 1;
    }
    lua::Stack::push(L, math::Rect{static_cast<float>(area.x), static_cast<float>(area.y), static_cast<float>(area.width), static_cast<float>(area.height)});
    return 1;
}

int ImageLua::clone(lua_State* L) {
    push(L, lua::Userdata::check<Image>(L, 1));
    return 1;
}

int ImageLua::encodePng(lua_State* L) {
    const std::vector<std::uint8_t> encoded = lua::Userdata::check<Image>(L, 1).encodePng();
    lua_pushlstring(L, reinterpret_cast<const char*>(encoded.data()), encoded.size());
    return 1;
}

// Encodes on the task pool, from the image as it is now, and returns a promise of the bytes.
int ImageLua::encodePngAsync(lua_State* L) {
    const std::shared_ptr<const Image> image = lua::Userdata::checkShared<Image>(L, 1);
    core::Engine& engine = lua::Runtime::getEngine(L);
    auto promise = std::make_shared<varn::async::Promise>(engine.getScriptRuntime());
    // clang-format off
    engine.getJobs().run([image] { return image->encodePng(); }, [promise](core::JobSystem::Result<std::vector<std::uint8_t>> result) {
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
int ImageLua::readTarget(lua_State* L) {
    const RenderTarget target = lua::Userdata::check<RenderTarget>(L, 1);
    core::Engine& engine = lua::Runtime::getEngine(L);
    auto promise = std::make_shared<varn::async::Promise>(engine.getScriptRuntime());
    engine.getGraphics().readTarget(target, settle(promise));
    varn::async::Promise::push(L, promise);
    return 1;
}

int ImageLua::captureScreen(lua_State* L) {
    core::Engine& engine = lua::Runtime::getEngine(L);
    auto promise = std::make_shared<varn::async::Promise>(engine.getScriptRuntime());
    engine.captureScreen(settle(promise));
    varn::async::Promise::push(L, promise);
    return 1;
}

Device::ReadCallback ImageLua::settle(const std::shared_ptr<varn::async::Promise>& promise) {
    // clang-format off
    return [promise](std::optional<Image> image, const std::string& error) {
        if (!image) {
            promise->reject(error);
            return;
        }
        auto shared = std::make_shared<Image>(std::move(*image));
        promise->resolveCustom([shared](lua_State* state) { lua::Userdata::emplace<Image>(state, shared); });
    };
    // clang-format on
}

int ImageLua::palette(lua_State* L) {
    const Image& image = lua::Userdata::check<Image>(L, 1);
    const std::vector<ImagePalette::Swatch> colors = ImagePalette::extract(image, static_cast<int>(luaL_optinteger(L, 2, kPaletteColors)));
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

int ImageLua::swatches(lua_State* L) {
    const Image& image = lua::Userdata::check<Image>(L, 1);
    const ImagePalette::Swatches found = ImagePalette::choose(ImagePalette::extract(image, static_cast<int>(luaL_optinteger(L, 2, kSwatchColors))));
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

int ImageLua::placeholder(lua_State* L) {
    const Image& image = lua::Userdata::check<Image>(L, 1);
    ImagePlaceholder::Options options;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kPlaceholderFields});
        lua::Table::readField(L, 2, "columns", options.columns);
        lua::Table::readField(L, 2, "rows", options.rows);
    }
    lua::Stack::push(L, ImagePlaceholder::encode(image, options));
    return 1;
}

int ImageLua::blur(lua_State* L) {
    ImageFilters::blur(checkWritable(L, 1), lua::Stack::read<float>(L, 2));
    return 0;
}

int ImageLua::sharpen(lua_State* L) {
    ImageFilters::sharpen(checkWritable(L, 1), static_cast<float>(luaL_optnumber(L, 2, 1.0)));
    return 0;
}

ImageFilters::Adjustment ImageLua::readAdjustment(lua_State* L, int index) {
    ImageFilters::Adjustment adjustment;
    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kAdjustFields});
    lua::Table::readField(L, index, "brightness", adjustment.brightness);
    lua::Table::readField(L, index, "contrast", adjustment.contrast);
    lua::Table::readField(L, index, "saturation", adjustment.saturation);
    lua::Table::readField(L, index, "hue", adjustment.hue);
    return adjustment;
}

int ImageLua::adjust(lua_State* L) {
    Image& image = checkWritable(L, 1);
    ImageFilters::adjust(image, readAdjustment(L, 2));
    return 0;
}

int ImageLua::grayscale(lua_State* L) {
    ImageFilters::grayscale(checkWritable(L, 1), static_cast<float>(luaL_optnumber(L, 2, 1.0)));
    return 0;
}

int ImageLua::invert(lua_State* L) {
    ImageFilters::invert(checkWritable(L, 1));
    return 0;
}

int ImageLua::tint(lua_State* L) {
    Image& image = checkWritable(L, 1);
    ImageFilters::tint(image, lua::Stack::read<math::Color>(L, 2), static_cast<float>(luaL_optnumber(L, 3, 1.0)));
    return 0;
}

int ImageLua::pixelate(lua_State* L) {
    ImageFilters::pixelate(checkWritable(L, 1), static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}

int ImageLua::outline(lua_State* L) {
    Image& image = checkWritable(L, 1);
    ImageFilters::outline(image, lua::Stack::read<math::Color>(L, 2), static_cast<float>(luaL_optnumber(L, 3, 1.0)));
    return 0;
}

// Every effect reads its options here on the frame thread, and its step runs on the task pool, where a value out of range rejects the promise.
std::function<void(Image&)> ImageLua::readEffect(lua_State* L, int index) {
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
        const math::Color value = lua::Stack::read<math::Color>(L, -1);
        lua_pop(L, 1);
        return value;
    };
    // clang-format on
    if (effect == "blur") {
        lua::Table::checkFields(L, index, {kBlurEffectFields});
        return [radius = number("radius", 0.0F)](Image& image) { ImageFilters::blur(image, radius); };
    }
    if (effect == "sharpen") {
        lua::Table::checkFields(L, index, {kAmountEffectFields});
        return [amount = number("amount", 1.0F)](Image& image) { ImageFilters::sharpen(image, amount); };
    }
    if (effect == "adjust") {
        lua::Table::checkFields(L, index, {kAdjustEffectFields});
        ImageFilters::Adjustment adjustment{.brightness = number("brightness", 0.0F), .contrast = number("contrast", 1.0F), .saturation = number("saturation", 1.0F), .hue = number("hue", 0.0F)};
        return [adjustment](Image& image) { ImageFilters::adjust(image, adjustment); };
    }
    if (effect == "grayscale") {
        lua::Table::checkFields(L, index, {kAmountEffectFields});
        return [amount = number("amount", 1.0F)](Image& image) { ImageFilters::grayscale(image, amount); };
    }
    if (effect == "invert") {
        lua::Table::checkFields(L, index, {kInvertEffectFields});
        return [](Image& image) { ImageFilters::invert(image); };
    }
    if (effect == "tint") {
        lua::Table::checkFields(L, index, {kTintEffectFields});
        return [tinted = color("color"), amount = number("amount", 1.0F)](Image& image) { ImageFilters::tint(image, tinted, amount); };
    }
    if (effect == "pixelate") {
        lua::Table::checkFields(L, index, {kPixelateEffectFields});
        return [size = static_cast<int>(number("size", 1.0F))](Image& image) { ImageFilters::pixelate(image, size); };
    }
    if (effect == "outline") {
        lua::Table::checkFields(L, index, {kOutlineEffectFields});
        return [outlined = color("color"), thickness = number("thickness", 1.0F)](Image& image) { ImageFilters::outline(image, outlined, thickness); };
    }
    luaL_error(L, "The image effect \"%s\" is unknown. It is \"blur\", \"sharpen\", \"adjust\", \"grayscale\", \"invert\", \"tint\", \"pixelate\" or \"outline\".", effect.c_str());
    return {};
}

// Returns a promise of a copy of this image with the effects of the list applied in order on the task pool, while this image stays as it is.
int ImageLua::applyAsync(lua_State* L) {
    const std::shared_ptr<const Image> source = lua::Userdata::checkShared<Image>(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    std::vector<std::function<void(Image&)>> steps;
    for (lua_Integer index = 1; lua_rawgeti(L, 2, index) != LUA_TNIL; ++index) {
        steps.push_back(readEffect(L, lua_gettop(L)));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    core::Engine& engine = lua::Runtime::getEngine(L);
    auto promise = std::make_shared<varn::async::Promise>(engine.getScriptRuntime());
    // clang-format off
    engine.getJobs().run([source, steps = std::move(steps)] {
        Image result = *source;
        for (const std::function<void(Image&)>& step : steps) {
            step(result);
        }
        return result;
    }, [promise](core::JobSystem::Result<Image> result) {
        if (!result.isOk()) {
            promise->reject(result.error);
            return;
        }
        auto image = std::make_shared<Image>(std::move(*result.value));
        promise->resolveCustom([image](lua_State* state) { lua::Userdata::emplace<Image>(state, image); });
    });
    // clang-format on
    varn::async::Promise::push(L, promise);
    return 1;
}

int ImageLua::decodePlaceholder(lua_State* L) {
    push(L, ImagePlaceholder::decode(lua::Stack::read<std::string_view>(L, 1), static_cast<int>(luaL_checkinteger(L, 2)), static_cast<int>(luaL_checkinteger(L, 3))));
    return 1;
}

int ImageLua::width(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Image>(L, 1).getWidth());
    return 1;
}

int ImageLua::height(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Image>(L, 1).getHeight());
    return 1;
}

void ImageLua::install(lua_State* L) {
    lua::ClassBuilder<Image>(L).function("pixel", &lua::Binding::native<&pixel>).function("setPixel", &lua::Binding::native<&setPixel>).function("fill", &lua::Binding::native<&fill>).function("blit", &lua::Binding::native<&blit>).function("blitAsync", &lua::Binding::native<&blitAsync>).function("crop", &lua::Binding::native<&crop>).function("bleed", &lua::Binding::native<&bleed>).function("visibleArea", &lua::Binding::native<&visibleArea>).function("clone", &lua::Binding::native<&clone>).function("encodePng", &lua::Binding::native<&encodePng>).function("encodePngAsync", &lua::Binding::native<&encodePngAsync>).function("palette", &lua::Binding::native<&palette>).function("swatches", &lua::Binding::native<&swatches>).function("placeholder", &lua::Binding::native<&placeholder>).function("blur", &lua::Binding::native<&blur>).function("sharpen", &lua::Binding::native<&sharpen>).function("adjust", &lua::Binding::native<&adjust>).function("grayscale", &lua::Binding::native<&grayscale>).function("invert", &lua::Binding::native<&invert>).function("tint", &lua::Binding::native<&tint>).function("pixelate", &lua::Binding::native<&pixelate>).function("outline", &lua::Binding::native<&outline>).function("applyAsync", &lua::Binding::native<&applyAsync>).property("width", &width).property("height", &height).install();
}

} // namespace haylen::graphics
