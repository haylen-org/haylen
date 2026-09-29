#include "text/PhraseBreaker.hpp"

#include "core/EmbeddedFiles.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/core/Utf8.hpp"

namespace haylen::text {

bool PhraseBreaker::isThai(char32_t codePoint) noexcept {
    return codePoint >= U'\U00000E00' && codePoint <= U'\U00000E7F';
}

PhraseBreaker::Model PhraseBreaker::parse(std::span<const std::uint8_t> json) {
    const core::Json document = core::Json::parse(json.begin(), json.end());
    Model model;
    for (std::size_t feature = 0; feature < kFeatures.size(); ++feature) {
        const auto group = document.find(std::string(kFeatures[feature]));
        if (group == document.end()) {
            continue;
        }
        for (const auto& [key, weight] : group->items()) {
            model.weights[feature].emplace(core::Utf8::decode(key), weight.get<int>());
            model.total += weight.get<int>();
        }
    }
    return model;
}

const PhraseBreaker::Model& PhraseBreaker::getThaiModel() {
    static const Model& model = *new const Model(parse(core::EmbeddedFiles::getThaiPhraseModel()));
    return model;
}

int PhraseBreaker::weigh(const Model& model, std::size_t feature, std::u32string_view text, std::size_t begin, std::size_t length) {
    const std::unordered_map<std::u32string, int>& weights = model.weights[feature];
    const auto found = weights.find(std::u32string(text.substr(begin, length)));
    return found != weights.end() ? found->second : 0;
}

// Every place between two letters weighs the single letters up to three before and after it, the pairs around it and the triples that cross it, and a phrase starts where those weights add up to more than half of every weight of the model.
std::vector<bool> PhraseBreaker::findThaiPhrases(std::u32string_view text) {
    const Model& model = getThaiModel();
    std::vector<bool> starts(text.size(), false);
    for (std::size_t index = 1; index < text.size(); ++index) {
        const bool twoBefore = index > 1;
        const bool threeBefore = index > 2;
        const bool oneAfter = index + 1 < text.size();
        const bool twoAfter = index + 2 < text.size();
        long long score = 0;
        score += threeBefore ? weigh(model, 0, text, index - 3, 1) : 0;
        score += twoBefore ? weigh(model, 1, text, index - 2, 1) : 0;
        score += weigh(model, 2, text, index - 1, 1);
        score += weigh(model, 3, text, index, 1);
        score += oneAfter ? weigh(model, 4, text, index + 1, 1) : 0;
        score += twoAfter ? weigh(model, 5, text, index + 2, 1) : 0;
        score += twoBefore ? weigh(model, 6, text, index - 2, 2) : 0;
        score += weigh(model, 7, text, index - 1, 2);
        score += oneAfter ? weigh(model, 8, text, index, 2) : 0;
        score += threeBefore ? weigh(model, 9, text, index - 3, 3) : 0;
        score += twoBefore ? weigh(model, 10, text, index - 2, 3) : 0;
        score += oneAfter ? weigh(model, 11, text, index - 1, 3) : 0;
        score += twoAfter ? weigh(model, 12, text, index, 3) : 0;
        starts[index] = score * 2 > model.total;
    }
    return starts;
}

} // namespace haylen::text
