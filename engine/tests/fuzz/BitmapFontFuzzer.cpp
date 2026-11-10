#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "gpu/NullDevice.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/text/BitmapFont.hpp"

namespace haylen::text {

// Reads each input as a BMFont file in its text or binary format, which its first byte picks by putting the magic of the binary format before the rest or not, and lays out text with what reads, its pages drawn from a white texture. A file may fail only with the error of data that is not a BMFont file.
class BitmapFontFuzzer final {
  public:
    static void read(std::span<const std::uint8_t> bytes) {
        if (bytes.empty()) {
            return;
        }
        std::vector<std::uint8_t> file;
        if (bytes[0] % 2 == 1) {
            file.assign(kBinaryMagic.begin(), kBinaryMagic.end());
        }
        file.insert(file.end(), bytes.begin() + 1, bytes.end());

        try {
            const BitmapFont::Description description = BitmapFont::parse(file);
            if (description.pages.size() > kMaxPages) {
                return;
            }
            BitmapFont font(description, std::vector<graphics::Texture>(description.pages.size(), getDevice().getWhiteTexture()));
            (void)font.layout("Sphinx of quartz, 0123 \xD8\xA8\xD8\xB3\xD9\x85", {.size = 20.0F, .maxWidth = 100.0F});
        } catch (const std::invalid_argument&) {}
    }

  private:
    static constexpr std::string_view kBinaryMagic{"BMF\x03", 4};

    // A file names up to 65535 pages, and a few already reach every way a page is used.
    static constexpr std::size_t kMaxPages = 8;

    // The device lives for the whole run, which the process ends without destroying it.
    static graphics::Device& getDevice() {
        static graphics::Device* const device = new graphics::Device(std::make_unique<gpu::NullDevice>());
        return *device;
    }
};

} // namespace haylen::text

// The entry point libFuzzer calls with each input it generates.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    haylen::text::BitmapFontFuzzer::read(std::span(data, size));
    return 0;
}
