#include "haylen/text/RichText.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "text/LayoutBuilder.hpp"
#include "text/MarkupParser.hpp"

namespace haylen::text {

debug::ObjectCounter RichText::counter("RichText", debug::ObjectCounter::Kind::Native);

RichText::RichText(std::string markup, RichTextOptions textOptions, std::shared_ptr<RichTextRegistry> textRegistry) : options(std::move(textOptions)), registry(std::move(textRegistry)) {
    if (!registry) {
        throw std::invalid_argument("Rich text needs the registry of text effects and icons.");
    }
    validate(options);
    setMarkup(std::move(markup));
}

RichTextDocument RichText::parse(std::string_view markup) {
    return MarkupParser(markup).parse();
}

void RichText::validate(const RichTextOptions& value) {
    if (!value.family) {
        throw std::invalid_argument("Rich text needs a font family.");
    }
    if (!(value.size > 0.0F) || !(value.scale > 0.0F) || !(value.lineSpacing > 0.0F)) {
        throw std::invalid_argument("Rich text needs a positive size, scale and line spacing.");
    }
    if (value.revealSpeed < 0.0F) {
        throw std::invalid_argument("Rich text cannot reveal at a negative speed.");
    }
}

// Unknown tags name effects, which must be registered, so a typo in a tag reads as a missing effect at the place it was written.
void RichText::setMarkup(std::string value) {
    RichTextDocument parsed = parse(value);
    std::vector<TextEffect::Function> functions;
    std::vector<TextEffect::Parameters> attributes;
    for (const RichTextDocument::Effect& effect : parsed.effects) {
        const TextEffect::Function* found = registry->findEffect(effect.name);
        if (found == nullptr) {
            throw std::invalid_argument("Rich text markup at line " + std::to_string(effect.line) + ", column " + std::to_string(effect.column) + ": [" + effect.name + "] is neither a tag nor a registered text effect.");
        }
        functions.push_back(*found);
        attributes.emplace_back(effect.name, effect.parameters);
    }

    source = std::move(value);
    document = std::move(parsed);
    effects = std::move(functions);
    parameters = std::move(attributes);
    effectStarts.clear();
    layouts.clear();
    time = 0.0F;
    resetReveal();
}

void RichText::setOptions(RichTextOptions value) {
    validate(value);
    options = std::move(value);
    layouts.clear();
    resetReveal();
}

void RichText::setMaxWidth(float value) {
    options.maxWidth = value;
    frameDirty = true;
}

void RichText::setScale(float value) {
    if (!(value > 0.0F)) {
        throw std::invalid_argument("Rich text needs a positive scale.");
    }
    options.scale = value;
    frameDirty = true;
}

void RichText::resetReveal() noexcept {
    revealClock = 0.0F;
    visibleCharacters = options.revealSpeed > 0.0F ? std::optional<std::size_t>(0) : std::nullopt;
    revealDirty = true;
    frameDirty = true;
}

void RichText::update(float deltaSeconds) {
    if (deltaSeconds < 0.0F) {
        throw std::invalid_argument("Rich text cannot go back in time.");
    }
    time += deltaSeconds;

    // A layout that waited for an image is built again at the next use, until the image arrives.
    std::erase_if(layouts, [](const CachedLayout& entry) { return entry.layout.waitingForImages; });
    if (options.revealSpeed > 0.0F && visibleCharacters) {
        revealClock += deltaSeconds;
        visibleCharacters = countRevealed(revealClock);
    }
    frameDirty = true;
}

// Layouts stay cached for the last few widths, so a container that measures at one width and draws at another lays out once for each.
RichText::CachedLayout& RichText::getCachedLayout(float maxWidth) {
    for (auto entry = layouts.begin(); entry != layouts.end(); ++entry) {
        if (entry->maxWidth == maxWidth && entry->scale == options.scale) {
            layouts.splice(layouts.begin(), layouts, entry);
            return layouts.front();
        }
    }

    RichTextOptions at = options;
    at.maxWidth = maxWidth;
    layouts.push_front({.maxWidth = maxWidth, .scale = options.scale, .generation = ++builds, .layout = LayoutBuilder(document, at, *registry).build()});
    if (layouts.size() > kCachedLayouts) {
        layouts.pop_back();
    }
    return layouts.front();
}

const TextLayout& RichText::getLayout() {
    return getCachedLayout(options.maxWidth).layout;
}

const TextLayout& RichText::getLayout(float maxWidth) {
    return getCachedLayout(maxWidth).layout;
}

math::Vec2 RichText::getSize() {
    return getLayout().size;
}

const std::vector<float>& RichText::getRevealTimes() {
    if (!revealDirty) {
        return revealTimes;
    }
    revealTimes.clear();
    float clock = 0.0F;
    for (const TextLayout::Character& character : getLayout().characters) {
        clock += character.pause + (options.revealSpeed > 0.0F ? 1.0F / (options.revealSpeed * character.speed) : 0.0F);
        revealTimes.push_back(clock);
    }
    revealDirty = false;
    return revealTimes;
}

std::size_t RichText::countRevealed(float clock) {
    const std::vector<float>& times = getRevealTimes();
    return static_cast<std::size_t>(std::upper_bound(times.begin(), times.end(), clock) - times.begin());
}

std::size_t RichText::getCharacterCount() {
    return getLayout().characters.size();
}

std::size_t RichText::getVisibleCharacters() {
    const std::size_t count = getCharacterCount();
    return std::min(visibleCharacters.value_or(count), count);
}

float RichText::getVisibleRatio() {
    const std::size_t count = getCharacterCount();
    return count == 0 ? 1.0F : static_cast<float>(getVisibleCharacters()) / static_cast<float>(count);
}

void RichText::setVisibleCharacters(std::size_t value) {
    const std::size_t shown = std::min(value, getCharacterCount());
    visibleCharacters = shown;
    revealClock = shown == 0 ? 0.0F : getRevealTimes()[shown - 1];
    frameDirty = true;
}

void RichText::setVisibleRatio(float value) {
    setVisibleCharacters(static_cast<std::size_t>(std::lround(std::clamp(value, 0.0F, 1.0F) * static_cast<float>(getCharacterCount()))));
}

bool RichText::isRevealing() {
    return getVisibleCharacters() < getCharacterCount();
}

std::optional<std::string> RichText::getLinkAt(math::Vec2 point) {
    for (const TextLayout::Area& area : getLayout().links) {
        if (area.rect.contains(point)) {
            return document.links[area.index];
        }
    }
    return std::nullopt;
}

std::optional<std::string> RichText::getHintAt(math::Vec2 point) {
    for (const TextLayout::Area& area : getLayout().hints) {
        if (area.rect.contains(point)) {
            return document.hints[area.index];
        }
    }
    return std::nullopt;
}

// Effects see every glyph of their tag, counted from the first character inside the tag, and apply from the outermost tag in.
void RichText::applyEffects(TextLayout& moved) {
    if (effectStarts.size() != effects.size()) {
        effectStarts.assign(effects.size(), 0);
        std::vector<bool> seen(effects.size(), false);
        for (const TextLayout::Glyph& glyph : moved.glyphs) {
            for (const std::size_t effect : document.styles[glyph.style].effects) {
                if (!seen[effect]) {
                    seen[effect] = true;
                    effectStarts[effect] = glyph.character;
                }
            }
        }
    }

    for (TextLayout::Glyph& glyph : moved.glyphs) {
        const std::vector<std::size_t>& tags = document.styles[glyph.style].effects;
        if (tags.empty()) {
            continue;
        }
        TextEffect::Glyph moving{.character = glyph.character, .codePoint = glyph.codePoint, .position = {glyph.position.x, glyph.baseline}, .time = time, .color = glyph.color};
        for (const std::size_t effect : tags) {
            moving.index = glyph.character - std::min(effectStarts[effect], glyph.character);
            effects[effect](moving, parameters[effect]);
        }
        glyph.position += moving.offset;
        glyph.baseline += moving.offset.y;
        glyph.color = moving.color;
        glyph.visible = moving.visible;
    }
}

// Characters the reveal has not reached hide, and the backgrounds and lines of partly revealed text end at the last revealed character, which is at their left when their text reads right to left.
void RichText::applyReveal(TextLayout& revealed, std::size_t visible) {
    for (TextLayout::Glyph& glyph : revealed.glyphs) {
        glyph.visible = glyph.visible && glyph.character < visible;
    }
    for (TextLayout::Image& image : revealed.images) {
        image.visible = image.character < visible;
    }
    for (TextLayout::Box& box : revealed.boxes) {
        box.visible = box.firstCharacter < visible;
        const bool followsText = box.kind == TextLayout::Box::Kind::Background || box.kind == TextLayout::Box::Kind::Underline || box.kind == TextLayout::Box::Kind::Strike;
        if (!box.visible || !followsText || box.lastCharacter < visible) {
            continue;
        }
        const math::Rect& last = revealed.characters[visible - 1].box;
        if (box.rightToLeft) {
            const float right = box.rect.getRight();
            box.rect.x = std::min(last.x, right);
            box.rect.width = right - box.rect.x;
        } else {
            box.rect.width = std::max(0.0F, last.getRight() - box.rect.x);
        }
    }
}

const TextLayout& RichText::getFrame() {
    const std::size_t visible = getVisibleCharacters();
    const CachedLayout& current = getCachedLayout(options.maxWidth);
    const bool revealing = visible < current.layout.characters.size();
    if (effects.empty() && !revealing) {
        return current.layout;
    }
    if (!frameDirty && framedGeneration == current.generation) {
        return frame;
    }

    frame = current.layout;
    applyEffects(frame);
    if (revealing) {
        applyReveal(frame, visible);
    }
    framedGeneration = current.generation;
    frameDirty = false;
    return frame;
}

} // namespace haylen::text
