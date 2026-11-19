#include "support/TestFiles.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <stdexcept>

#include "haylen/graphics/Image.hpp"
#include "haylen/math/Color.hpp"

namespace haylen::test {

std::vector<std::uint8_t> TestFiles::read(const std::filesystem::path& file) {
    const std::string text = readText(file);
    return {text.begin(), text.end()};
}

std::string TestFiles::readText(const std::filesystem::path& file) {
    std::ifstream stream(file, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("The test could not open the file \"" + file.string() + "\".");
    }

    std::string text{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    if (stream.bad()) {
        throw std::runtime_error("The test could not read the file \"" + file.string() + "\".");
    }

    return text;
}

void TestFiles::write(const std::filesystem::path& file, std::span<const std::uint8_t> bytes) {
    std::ofstream stream(file, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    stream.close();
    if (!stream) {
        throw std::runtime_error("The test could not write the file \"" + file.string() + "\".");
    }
}

void TestFiles::write(const std::filesystem::path& file, std::string_view text) {
    write(file, std::span(reinterpret_cast<const std::uint8_t*>(text.data()), text.size()));
}

void TestFiles::append(const std::filesystem::path& file, std::string_view text) {
    std::ofstream stream(file, std::ios::binary | std::ios::app);
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    stream.close();
    if (!stream) {
        throw std::runtime_error("The test could not append to the file \"" + file.string() + "\".");
    }
}

void TestFiles::damage(const std::filesystem::path& file, std::uintmax_t offset) {
    std::fstream stream(file, std::ios::binary | std::ios::in | std::ios::out);
    stream.seekg(static_cast<std::streamoff>(offset));
    const int byte = stream.get();
    stream.seekp(static_cast<std::streamoff>(offset));
    stream.put(static_cast<char>(byte ^ 0x01));
    stream.close();
    if (byte == std::char_traits<char>::eof() || !stream) {
        throw std::runtime_error("The test could not damage the byte " + std::to_string(offset) + " of the file \"" + file.string() + "\".");
    }
}

std::vector<std::uint8_t> TestFiles::bytes(const std::string& text) {
    return {text.begin(), text.end()};
}

std::string TestFiles::toText(std::span<const std::uint8_t> data) {
    return {data.begin(), data.end()};
}

std::vector<std::uint8_t> TestFiles::pngImage(int width, int height, std::uint32_t rgba) {
    return graphics::Image(width, height, math::Color::fromHex(rgba)).encodePng();
}

std::vector<std::uint8_t> TestFiles::wav(std::uint32_t sampleRate, std::uint16_t channels, const std::vector<float>& samples) {
    std::vector<std::uint8_t> file;
    // clang-format off
    const auto append = [&file](std::uint32_t value, int bytes) {
        for (int index = 0; index < bytes; ++index) {
            file.push_back(static_cast<std::uint8_t>((value >> (8 * index)) & 0xFFU));
        }
    };
    // clang-format on
    const auto appendText = [&file](const char* text) { file.insert(file.end(), text, text + 4); };
    const auto dataSize = static_cast<std::uint32_t>(samples.size() * 2);

    appendText("RIFF");
    append(36 + dataSize, 4);
    appendText("WAVE");
    appendText("fmt ");
    append(16, 4);
    append(1, 2);
    append(channels, 2);
    append(sampleRate, 4);
    append(sampleRate * channels * 2, 4);
    append(channels * 2U, 2);
    append(16, 2);
    appendText("data");
    append(dataSize, 4);

    for (const float sample : samples) {
        const auto value = static_cast<std::int16_t>(std::lround(std::clamp(sample, -1.0F, 1.0F) * 32767.0F));
        append(static_cast<std::uint16_t>(value), 2);
    }

    return file;
}

std::vector<std::uint8_t> TestFiles::randomBytes(std::size_t size, std::uint64_t seed) {
    std::vector<std::uint8_t> bytes(size);
    std::uint64_t state = seed;
    for (std::size_t offset = 0; offset < size; offset += sizeof(std::uint64_t)) {
        state += 0x9E3779B97F4A7C15;
        std::uint64_t value = state;
        value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9;
        value = (value ^ (value >> 27)) * 0x94D049BB133111EB;
        value ^= value >> 31;
        for (std::size_t index = 0; index < sizeof(value) && offset + index < size; ++index) {
            bytes[offset + index] = static_cast<std::uint8_t>(value >> (8 * index));
        }
    }

    return bytes;
}

} // namespace haylen::test
