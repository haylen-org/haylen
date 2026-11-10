#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <unordered_set>
#include <vector>

#include "haylen/text/Font.hpp"

namespace haylen::text {

// The fonts of the system that draw what no font of a family holds, such as Chinese that a person types into an app that ships no Chinese font. A family that uses them asks for the characters it cannot draw, the engine looks for the font of the system that draws each one and loads that face off the frame thread, and the family lays its text out again once the face arrives. It keeps one font for each face of the system, never reads a face it refused again and asks for every character once, so a character that no font of the system draws is never asked for again. It lives on the frame thread.
class SystemFonts final {
  public:
    // A face of the system with the name that tells it apart from every other face.
    struct Face {
        std::string name;
        std::shared_ptr<Font> font;
    };

    // Asks for the font of the system that draws the code point, unless it was asked for before, and returns whether its answer is still to come.
    bool request(char32_t codePoint);

    // Moves the code points asked for since the last call into `codePoints`, which the engine then looks up.
    void takeRequests(std::vector<char32_t>& codePoints);

    // Ends the lookup of the code points, whose faces arrived through `add` or that no font of the system draws.
    void finish(std::span<const char32_t> codePoints);

    // Adds a face that a lookup found and counts a new revision, so families lay their text out again.
    void add(Face face);

    // Remembers a face that a lookup refused, so no lookup reads it again.
    void refuse(std::string name);

    // The names of the faces that arrived and of those refused, which a lookup skips.
    [[nodiscard]] std::vector<std::string> getKnownNames() const;
    [[nodiscard]] std::span<const Face> getFaces() const noexcept {
        return faces;
    }

    // Grows each time a face arrives.
    [[nodiscard]] std::uint64_t getRevision() const noexcept {
        return revision;
    }

    // Whether any lookup has not answered yet.
    [[nodiscard]] bool isWaiting() const noexcept {
        return !pending.empty();
    }

  private:
    std::vector<Face> faces;
    std::vector<std::string> refused;
    std::unordered_set<char32_t> asked;
    std::unordered_set<char32_t> pending;
    std::vector<char32_t> requests;
    std::uint64_t revision = 0;
};

} // namespace haylen::text
