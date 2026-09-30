#pragma once

#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/debug/ObjectCounter.hpp"
#include "haylen/debug/TrackedObject.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/text/Effect.hpp"
#include "haylen/text/Layout.hpp"
#include "haylen/text/RichTextDocument.hpp"
#include "haylen/text/RichTextOptions.hpp"
#include "haylen/text/RichTextRegistry.hpp"

namespace haylen::text {

// Text written in BBCode markup, laid out with a font family and animated by effects and a typewriter reveal. Layouts are cached by width and scale, and each frame applies the effects and the reveal to a copy of the layout. Effects run while that copy is built, so a method that changes or lays out the text throws `std::logic_error` when an effect of the same text calls it.
class RichText final {
  public:
    RichText(std::string markup, RichTextOptions textOptions, std::shared_ptr<RichTextRegistry> textRegistry);

    // Reads markup into its paragraphs, runs and objects. Malformed markup throws `std::invalid_argument` naming the line and column of the problem.
    [[nodiscard]] static RichTextDocument parse(std::string_view markup);

    // Replaces the markup, which starts the effects and the reveal again. Effects must be registered by then.
    void setMarkup(std::string value);
    [[nodiscard]] const std::string& getMarkup() const noexcept {
        return source;
    }
    [[nodiscard]] const RichTextDocument& getDocument() const noexcept {
        return document;
    }

    void setOptions(RichTextOptions value);
    [[nodiscard]] const RichTextOptions& getOptions() const noexcept {
        return options;
    }
    void setMaxWidth(float value);
    void setScale(float value);

    // Moves the effects and the reveal forward.
    void update(float deltaSeconds);
    [[nodiscard]] float getTime() const noexcept {
        return time;
    }

    // Returns the layout at the current width, or at another width without changing it, which is how a container measures the text.
    [[nodiscard]] const Layout& getLayout();
    [[nodiscard]] const Layout& getLayout(float maxWidth);
    [[nodiscard]] math::Vec2 getSize();

    // Returns the layout of this moment, with the effects applied and the characters the reveal has not reached hidden.
    [[nodiscard]] const Layout& getFrame();

    // Return the payload of the link or the text of the hint under a point of the text block.
    [[nodiscard]] std::optional<std::string> getLinkAt(math::Vec2 point);
    [[nodiscard]] std::optional<std::string> getHintAt(math::Vec2 point);

    [[nodiscard]] std::size_t getCharacterCount();
    [[nodiscard]] std::size_t getVisibleCharacters();
    [[nodiscard]] float getVisibleRatio();

    // Shows the first characters, continuing the reveal from there when it runs, where a count beyond the text shows everything.
    void setVisibleCharacters(std::size_t value);
    void setVisibleRatio(float value);

    // Tells whether the reveal still has characters to show.
    [[nodiscard]] bool isRevealing();

  private:
    struct CachedLayout {
        float maxWidth = 0.0F;
        float scale = 1.0F;
        std::uint64_t generation = 0;
        Layout layout;
    };

    static constexpr std::size_t kCachedLayouts = 4;
    static debug::ObjectCounter& counter;

    static void validate(const RichTextOptions& value);
    static void validateWidth(float value);
    [[nodiscard]] static bool isPositive(float value) noexcept;
    static void applyReveal(Layout& revealed, std::size_t visible);

    void requireIdle() const;
    void resetReveal() noexcept;
    [[nodiscard]] CachedLayout& getCachedLayout(float maxWidth);
    [[nodiscard]] const std::vector<float>& getRevealTimes();
    [[nodiscard]] std::size_t countRevealed(float clock);
    void applyEffects(Layout& moved);

    std::string source;
    RichTextDocument document;
    RichTextOptions options;
    std::shared_ptr<RichTextRegistry> registry;
    std::vector<Effect::Function> effects;
    std::vector<Effect::Parameters> parameters;
    std::list<CachedLayout> layouts;
    std::vector<float> revealTimes;
    std::vector<std::size_t> effectStarts;
    Layout frame;
    std::optional<std::size_t> visibleCharacters;
    std::uint64_t builds = 0;
    std::uint64_t framedGeneration = 0;
    float revealClock = 0.0F;
    float time = 0.0F;
    bool frameDirty = true;
    bool revealDirty = true;
    bool applyingEffects = false;
    debug::TrackedObject tracked{counter};
};

} // namespace haylen::text
