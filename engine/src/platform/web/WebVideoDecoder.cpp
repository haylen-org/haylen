#include "platform/web/WebVideoDecoder.hpp"

#include <emscripten/em_js.h>

#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>

#include "haylen/core/Json.hpp"

// The JavaScript of the page goes into the module as it is written.
// clang-format off
EM_JS(int, haylen_js_video_begin, (), {
    return Module.haylen.video.begin();
});

EM_JS(void, haylen_js_video_append, (int id, const void* bytes, int length), {
    Module.haylen.video.append(id, bytes, length);
});

EM_JS(void, haylen_js_video_open, (int id, const char* type), {
    Module.haylen.video.open(id, UTF8ToString(type));
});

EM_JS(char*, haylen_js_video_info, (int id), {
    return stringToNewUTF8(Module.haylen.video.info(id));
});

EM_JS(void, haylen_js_video_control, (int id, int playing, double rate), {
    Module.haylen.video.control(id, playing !== 0, rate);
});

EM_JS(int, haylen_js_video_frame, (int id, void* pixels, int width, int height), {
    return Module.haylen.video.frame(id, pixels, width, height);
});

EM_JS(int, haylen_js_video_take_sound, (int id, float* samples, int capacity), {
    return Module.haylen.video.takeSound(id, samples, capacity);
});

EM_JS(void, haylen_js_video_seek, (int id, double seconds), {
    Module.haylen.video.seek(id, seconds);
});

EM_JS(double, haylen_js_video_time, (int id), {
    return Module.haylen.video.time(id);
});

EM_JS(void, haylen_js_video_close, (int id), {
    Module.haylen.video.close(id);
});
// clang-format on

namespace haylen::platform {

// The browser picks the decoder by the type of the file, which the extension of its path names.
WebVideoDecoder::WebVideoDecoder(io::PackageReader& file, std::string_view filePath) : path(filePath), id(haylen_js_video_begin()) {
    std::vector<std::uint8_t> piece(kPieceSize);
    const std::uint64_t size = file.getSize();
    for (std::uint64_t offset = 0; offset < size; offset += kPieceSize) {
        const std::size_t read = file.read(offset, piece);
        haylen_js_video_append(id, piece.data(), static_cast<int>(read));
    }
    const std::string extension = path.substr(path.find_last_of('.') + 1);
    const std::string type = extension == "webm" ? "video/webm" : (extension == "ogv" ? "video/ogg" : (extension == "mov" ? "video/quicktime" : "video/mp4"));
    haylen_js_video_open(id, type.c_str());
}

WebVideoDecoder::~WebVideoDecoder() {
    haylen_js_video_close(id);
}

const VideoDecoder::Info& WebVideoDecoder::getInfo() const noexcept {
    if (info.width == 0 && error.empty()) {
        refresh();
    }
    return info;
}

void WebVideoDecoder::refresh() const {
    char* text = haylen_js_video_info(id);
    const core::Json state = core::Json::parse(text);
    std::free(text);
    info.width = state.at("width").get<int>();
    info.height = state.at("height").get<int>();
    info.duration = state.at("duration").get<double>();
    info.sampleRate = state.at("sampleRate").get<std::uint32_t>();
    info.channels = info.sampleRate > 0 ? 2U : 0U;
    error = state.at("error").get<std::string>();
}

VideoDecoder::Read WebVideoDecoder::readFrame(Frame& frame) {
    frame.pixels.resize(static_cast<std::size_t>(info.width) * static_cast<std::size_t>(info.height) * 4U);
    const int read = haylen_js_video_frame(id, frame.pixels.data(), info.width, info.height);
    if (read == 3) {
        refresh();
        throw std::runtime_error("The video \"" + path + "\" could not be played by the browser: " + error);
    }
    if (read != 1) {
        return read == 2 ? Read::End : Read::Waiting;
    }
    frame.width = info.width;
    frame.height = info.height;
    frame.time = getTime();
    return Read::Frame;
}

bool WebVideoDecoder::readAudio(std::vector<float>& samples, double) {
    sound.resize(kSoundBlock);
    while (true) {
        const int copied = haylen_js_video_take_sound(id, sound.data(), static_cast<int>(sound.size()));
        samples.insert(samples.end(), sound.begin(), sound.begin() + copied);
        if (copied < static_cast<int>(sound.size())) {
            return true;
        }
    }
}

void WebVideoDecoder::seek(double seconds) {
    haylen_js_video_seek(id, seconds);
}

void WebVideoDecoder::control(bool playing, double rate) {
    haylen_js_video_control(id, playing ? 1 : 0, rate);
}

double WebVideoDecoder::getTime() const {
    return haylen_js_video_time(id);
}

} // namespace haylen::platform
