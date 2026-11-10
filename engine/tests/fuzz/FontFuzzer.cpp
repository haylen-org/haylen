#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "gpu/NullDevice.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/text/TrueTypeFont.hpp"
#include "text/FontFile.hpp"
#include "ui/ImGuiFontLoader.hpp"

namespace haylen::text {

// Reads each input as a TrueType or OpenType font file the two ways the engine draws fonts: it lays out text in every script with its distance field atlas, and it draws the same characters at two sizes through the font loader of ImGui. A file that is not a font may fail only with the error of data that is not a font, and a glyph or an atlas too large for the device only with its own error.
class FontFuzzer final {
  public:
    static void read(std::span<const std::uint8_t> bytes) {
        layOut(bytes);
        drawWithImGui(bytes);
    }

  private:
    static constexpr std::string_view kText = "Sphinx of quartz 0123 \xC3\xA9\xE2\x82\xAC \xE6\x97\xA5\xE6\x9C\xAC \xD8\xA8\xD8\xB3\xD9\x85 \xE0\xA4\xA8\xE0\xA4\xAE\xE0\xA4\xB8\xE0\xA5\x8D\xE0\xA4\xA4\xE0\xA5\x87 \xF0\x9F\x98\x80";
    static constexpr std::u32string_view kCharacters = U"Sphinx 0é€日بन\U0001F600";

    // A device whose textures are at most 2048 pixels on a side, which bounds the memory that one glyph can take. It lives for the whole run, which the process ends without destroying it.
    static constexpr int kLargestTexture = 2048;

    static graphics::Device& getDevice() {
        static graphics::Device* const device = makeDevice();
        return *device;
    }

    static graphics::Device* makeDevice() {
        gpu::Device::Capabilities capabilities = gpu::NullDevice::getDefaultCapabilities();
        capabilities.maxImageSize = kLargestTexture;
        return new graphics::Device(std::make_unique<gpu::NullDevice>(gpu::Device::Language::None, capabilities));
    }

    static void layOut(std::span<const std::uint8_t> bytes) {
        try {
            TrueTypeFont font(getDevice(), std::vector<std::uint8_t>(bytes.begin(), bytes.end()), {.bakeSize = 24.0F, .spread = 4, .atlasSize = 64});
            (void)font.layout(kText, {.size = 20.0F, .maxWidth = 200.0F});
            font.sync();
        } catch (const std::runtime_error&) {}
    }

    // The UI hands ImGui only the files that read as fonts. Each input gets an atlas and a context of its own, which no renderer serves, so the textures of the atlas end with them.
    static void drawWithImGui(std::span<const std::uint8_t> bytes) {
        if (!FontFile::read(bytes)) {
            return;
        }
        std::vector<std::uint8_t> copy(bytes.begin(), bytes.end());
        ImFontAtlas atlas;
        atlas.TexMaxWidth = kLargestTexture;
        atlas.TexMaxHeight = kLargestTexture;
        atlas.SetFontLoader(&ui::ImGuiFontLoader::get());
        const std::unique_ptr<ImGuiContext, decltype(&ImGui::DestroyContext)> context(ImGui::CreateContext(&atlas), &ImGui::DestroyContext);
        ImGui::GetIO().BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

        ImFontConfig config;
        config.FontDataOwnedByAtlas = false;
        ImFont* font = atlas.AddFontFromMemoryTTF(copy.data(), static_cast<int>(copy.size()), 24.0F, &config);
        for (const float size : {13.0F, 40.0F}) {
            ImFontBaked* baked = font->GetFontBaked(size);
            for (const char32_t character : kCharacters) {
                (void)baked->FindGlyph(static_cast<ImWchar>(character));
            }
        }
    }
};

} // namespace haylen::text

// The entry point libFuzzer calls with each input it generates.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    haylen::text::FontFuzzer::read(std::span(data, size));
    return 0;
}
