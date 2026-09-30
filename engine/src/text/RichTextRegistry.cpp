#include "haylen/text/RichTextRegistry.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "text/MarkupParser.hpp"

namespace haylen::text {

RichTextRegistry::RichTextRegistry(std::shared_ptr<FontFamily> defaultFamily) : family(std::move(defaultFamily)) {
    if (!family) {
        throw std::invalid_argument("A rich text registry needs the family of the default font.");
    }
    registerBuiltInEffects();
}

void RichTextRegistry::registerBuiltInEffects() {
    effects.insert_or_assign("wave", &Effect::wave);
    effects.insert_or_assign("shake", &Effect::shake);
    effects.insert_or_assign("tornado", &Effect::tornado);
    effects.insert_or_assign("fade", &Effect::fade);
    effects.insert_or_assign("rainbow", &Effect::rainbow);
    effects.insert_or_assign("pulse", &Effect::pulse);
}

void RichTextRegistry::registerEffect(std::string name, Effect::Function effect) {
    if (name.empty() || name.find_first_of(" =[]/") != std::string::npos) {
        throw std::invalid_argument("A text effect name must be a single word, not \"" + name + "\".");
    }
    if (MarkupParser::isTag(name)) {
        throw std::invalid_argument("The name \"" + name + "\" belongs to a rich text tag, so no effect can take it.");
    }
    if (std::find(kBuiltInEffects.begin(), kBuiltInEffects.end(), name) != kBuiltInEffects.end()) {
        throw std::invalid_argument("The name \"" + name + "\" belongs to a built-in text effect, so no other effect can take it.");
    }
    if (!effect) {
        throw std::invalid_argument("The text effect \"" + name + "\" needs a function.");
    }
    effects.insert_or_assign(std::move(name), std::move(effect));
}

const Effect::Function* RichTextRegistry::findEffect(std::string_view name) const noexcept {
    const auto found = effects.find(name);
    return found != effects.end() ? &found->second : nullptr;
}

std::vector<std::string> RichTextRegistry::getEffectNames() const {
    std::vector<std::string> names;
    for (const auto& [name, effect] : effects) {
        names.push_back(name);
    }
    return names;
}

void RichTextRegistry::registerIcon(std::string name, Icon icon) {
    if (name.empty()) {
        throw std::invalid_argument("A text icon needs a name.");
    }
    if (!icon.texture.isValid()) {
        throw std::invalid_argument("The text icon \"" + name + "\" needs a texture.");
    }
    if (icon.source.isEmpty()) {
        icon.source = {0.0F, 0.0F, icon.texture.getSize().x, icon.texture.getSize().y};
    }
    icons.insert_or_assign(std::move(name), std::move(icon));
}

const RichTextRegistry::Icon* RichTextRegistry::findIcon(std::string_view name) const noexcept {
    const auto found = icons.find(name);
    return found != icons.end() ? &found->second : nullptr;
}

void RichTextRegistry::clear() {
    effects.clear();
    icons.clear();
    registerBuiltInEffects();
}

} // namespace haylen::text
