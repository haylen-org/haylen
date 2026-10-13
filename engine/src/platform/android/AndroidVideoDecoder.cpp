#include "platform/android/AndroidVideoDecoder.hpp"

#include <dlfcn.h>
#include <media/NdkImage.h>
#include <media/NdkMediaFormat.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace haylen::platform {

AMediaDataSource* (*AndroidVideoDecoder::newSource)() = nullptr;
void (*AndroidVideoDecoder::setSourceData)(AMediaDataSource*, void*) = nullptr;
void (*AndroidVideoDecoder::setSourceReadAt)(AMediaDataSource*, AMediaDataSourceReadAt) = nullptr;
void (*AndroidVideoDecoder::setSourceGetSize)(AMediaDataSource*, AMediaDataSourceGetSize) = nullptr;
void (*AndroidVideoDecoder::setSourceClose)(AMediaDataSource*, AMediaDataSourceClose) = nullptr;
void (*AndroidVideoDecoder::deleteSource)(AMediaDataSource*) = nullptr;
media_status_t (*AndroidVideoDecoder::setExtractorSource)(AMediaExtractor*, AMediaDataSource*) = nullptr;

bool AndroidVideoDecoder::isAvailable() noexcept {
    static const bool available = load();
    return available;
}

// The library stays loaded for the process, like the functions it hands out.
bool AndroidVideoDecoder::load() noexcept {
    void* library = dlopen("libmediandk.so", RTLD_NOW | RTLD_LOCAL);
    if (library == nullptr) {
        return false;
    }
    newSource = reinterpret_cast<decltype(newSource)>(dlsym(library, "AMediaDataSource_new"));
    setSourceData = reinterpret_cast<decltype(setSourceData)>(dlsym(library, "AMediaDataSource_setUserdata"));
    setSourceReadAt = reinterpret_cast<decltype(setSourceReadAt)>(dlsym(library, "AMediaDataSource_setReadAt"));
    setSourceGetSize = reinterpret_cast<decltype(setSourceGetSize)>(dlsym(library, "AMediaDataSource_setGetSize"));
    setSourceClose = reinterpret_cast<decltype(setSourceClose)>(dlsym(library, "AMediaDataSource_setClose"));
    deleteSource = reinterpret_cast<decltype(deleteSource)>(dlsym(library, "AMediaDataSource_delete"));
    setExtractorSource = reinterpret_cast<decltype(setExtractorSource)>(dlsym(library, "AMediaExtractor_setDataSourceCustom"));
    return newSource != nullptr && setSourceData != nullptr && setSourceReadAt != nullptr && setSourceGetSize != nullptr && setSourceClose != nullptr && deleteSource != nullptr && setExtractorSource != nullptr;
}

// A failure on the way releases what opened before it, since the destructor of a decoder that never opened does not run.
AndroidVideoDecoder::AndroidVideoDecoder(std::shared_ptr<io::PackageReader> reader, std::string_view filePath) : path(filePath), file(std::move(reader)) {
    try {
        open();
    } catch (...) {
        release(sound);
        release(picture);
        if (images != nullptr) {
            AImageReader_delete(images);
        }
        throw;
    }
}

void AndroidVideoDecoder::open() {
    AMediaFormat* pictureFormat = openTrack(picture, "video/");
    if (pictureFormat == nullptr) {
        throw std::runtime_error("The video \"" + path + "\" holds no picture that this platform plays.");
    }
    std::int64_t duration = 0;
    AMediaFormat_getInt64(pictureFormat, AMEDIAFORMAT_KEY_DURATION, &duration);
    AMediaFormat_getInt32(pictureFormat, AMEDIAFORMAT_KEY_WIDTH, &info.width);
    AMediaFormat_getInt32(pictureFormat, AMEDIAFORMAT_KEY_HEIGHT, &info.height);
    info.duration = static_cast<double>(duration) / 1000000.0;
    if (AImageReader_new(info.width, info.height, AIMAGE_FORMAT_YUV_420_888, 2, &images) != AMEDIA_OK) {
        AMediaFormat_delete(pictureFormat);
        throw std::runtime_error("The video \"" + path + "\" needs an image reader that this device could not make.");
    }
    startCodec(picture, pictureFormat, true);

    AMediaFormat* soundFormat = openTrack(sound, "audio/");
    if (soundFormat == nullptr) {
        return;
    }
    std::int32_t rate = 0;
    std::int32_t channels = 0;
    AMediaFormat_getInt32(soundFormat, AMEDIAFORMAT_KEY_SAMPLE_RATE, &rate);
    AMediaFormat_getInt32(soundFormat, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &channels);
    info.sampleRate = static_cast<std::uint32_t>(rate);
    info.channels = std::min<std::uint32_t>(static_cast<std::uint32_t>(std::max(channels, 1)), 2);
    soundChannels = static_cast<std::uint32_t>(std::max(channels, 1));
    startCodec(sound, soundFormat, false);
}

AndroidVideoDecoder::~AndroidVideoDecoder() {
    release(sound);
    release(picture);
    AImageReader_delete(images);
}

ssize_t AndroidVideoDecoder::readAt(void* data, off64_t offset, void* buffer, size_t size) {
    auto* decoder = static_cast<AndroidVideoDecoder*>(data);
    try {
        const std::size_t read = decoder->file->read(static_cast<std::uint64_t>(offset), std::span<std::uint8_t>(static_cast<std::uint8_t*>(buffer), size));
        return read == 0 ? -1 : static_cast<ssize_t>(read);
    } catch (const std::exception&) {
        return -1;
    }
}

ssize_t AndroidVideoDecoder::getSize(void* data) {
    return static_cast<ssize_t>(static_cast<AndroidVideoDecoder*>(data)->file->getSize());
}

void AndroidVideoDecoder::close(void*) {}

AMediaFormat* AndroidVideoDecoder::openTrack(Track& track, std::string_view prefix) {
    track.source = newSource();
    setSourceData(track.source, this);
    setSourceReadAt(track.source, &readAt);
    setSourceGetSize(track.source, &getSize);
    setSourceClose(track.source, &close);
    track.extractor = AMediaExtractor_new();
    if (setExtractorSource(track.extractor, track.source) != AMEDIA_OK) {
        throw std::runtime_error("The video \"" + path + "\" is no file that this platform plays.");
    }
    const std::size_t count = AMediaExtractor_getTrackCount(track.extractor);
    for (std::size_t index = 0; index < count; ++index) {
        AMediaFormat* format = AMediaExtractor_getTrackFormat(track.extractor, index);
        const char* mime = nullptr;
        if (AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime) && std::string_view(mime).starts_with(prefix)) {
            AMediaExtractor_selectTrack(track.extractor, index);
            return format;
        }
        AMediaFormat_delete(format);
    }
    return nullptr;
}

// Pictures decode into the surface of the image reader.
void AndroidVideoDecoder::startCodec(Track& track, AMediaFormat* format, bool isPicture) {
    const char* name = nullptr;
    AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &name);
    const std::string mime = name != nullptr ? name : "";
    track.codec = AMediaCodec_createDecoderByType(mime.c_str());
    ANativeWindow* window = nullptr;
    if (isPicture) {
        AImageReader_getWindow(images, &window);
    }
    const bool started = track.codec != nullptr && AMediaCodec_configure(track.codec, format, window, nullptr, 0) == AMEDIA_OK && AMediaCodec_start(track.codec) == AMEDIA_OK;
    AMediaFormat_delete(format);
    if (!started) {
        throw std::runtime_error("The video \"" + path + "\" uses \"" + mime + "\", which no decoder of this device plays.");
    }
}

void AndroidVideoDecoder::feed(Track& track) {
    while (!track.inputEnded) {
        const ssize_t index = AMediaCodec_dequeueInputBuffer(track.codec, 0);
        if (index < 0) {
            return;
        }
        std::size_t capacity = 0;
        std::uint8_t* buffer = AMediaCodec_getInputBuffer(track.codec, static_cast<std::size_t>(index), &capacity);
        const ssize_t size = AMediaExtractor_readSampleData(track.extractor, buffer, capacity);
        if (size < 0) {
            AMediaCodec_queueInputBuffer(track.codec, static_cast<std::size_t>(index), 0, 0, 0, AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
            track.inputEnded = true;
            return;
        }
        AMediaCodec_queueInputBuffer(track.codec, static_cast<std::size_t>(index), 0, static_cast<std::size_t>(size), static_cast<std::uint64_t>(AMediaExtractor_getSampleTime(track.extractor)), 0);
        AMediaExtractor_advance(track.extractor);
    }
}

// The crop of an image ends before its right and bottom edges.
void AndroidVideoDecoder::takeImage(Frame& frame) {
    AImage* image = nullptr;
    for (int waits = 0; AImageReader_acquireNextImage(images, &image) != AMEDIA_OK; ++waits) {
        if (waits >= kImagePatience) {
            fail("the decoder of its picture rendered no image");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    AImageCropRect crop{};
    AImage_getCropRect(image, &crop);
    AndroidYuvImage yuv{.left = crop.left, .top = crop.top, .width = crop.right - crop.left, .height = crop.bottom - crop.top};
    for (int index = 0; index < 3; ++index) {
        AndroidYuvImage::Plane& plane = yuv.planes[static_cast<std::size_t>(index)];
        std::uint8_t* data = nullptr;
        int length = 0;
        AImage_getPlaneData(image, index, &data, &length);
        AImage_getPlaneRowStride(image, index, &plane.rowStride);
        AImage_getPlanePixelStride(image, index, &plane.pixelStride);
        plane.data = data;
        plane.size = static_cast<std::size_t>(std::max(length, 0));
    }
    try {
        yuv.toRgba(frame.pixels);
    } catch (...) {
        AImage_delete(image);
        throw;
    }
    AImage_delete(image);
    frame.width = yuv.width;
    frame.height = yuv.height;
}

void AndroidVideoDecoder::fail(std::string_view what) const {
    throw std::runtime_error("The video \"" + path + "\" stopped decoding: " + std::string(what) + ".");
}

// Pictures before the time of a seek decode on the way from the key frame before it and never show.
VideoDecoder::Read AndroidVideoDecoder::readFrame(Frame& frame) {
    for (int waits = 0; waits < kPatience;) {
        feed(picture);
        AMediaCodecBufferInfo buffer{};
        const ssize_t index = AMediaCodec_dequeueOutputBuffer(picture.codec, &buffer, kWaitMicroseconds);
        if (index < 0) {
            waits += index == AMEDIACODEC_INFO_TRY_AGAIN_LATER ? 1 : 0;
            continue;
        }
        const bool last = (buffer.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) != 0;
        const double time = static_cast<double>(buffer.presentationTimeUs) / 1000000.0;
        if (buffer.size > 0 && time >= skipUntil) {
            AMediaCodec_releaseOutputBuffer(picture.codec, static_cast<std::size_t>(index), true);
            takeImage(frame);
            frame.time = time;
            return Read::Frame;
        }
        AMediaCodec_releaseOutputBuffer(picture.codec, static_cast<std::size_t>(index), false);
        if (last) {
            picture.outputEnded = true;
            return Read::End;
        }
    }
    fail("the decoder of its picture gave nothing for three seconds");
}

// The sound decodes into 16-bit samples, or floats where the codec says so, of which the first two channels play.
bool AndroidVideoDecoder::readAudio(std::vector<float>& samples, double until) {
    int waits = 0;
    while (sound.codec != nullptr && !sound.outputEnded && soundTime < until) {
        if (waits >= kPatience) {
            fail("the decoder of its sound gave nothing for three seconds");
        }
        feed(sound);
        AMediaCodecBufferInfo buffer{};
        const ssize_t index = AMediaCodec_dequeueOutputBuffer(sound.codec, &buffer, kWaitMicroseconds);
        if (index == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
            AMediaFormat* format = AMediaCodec_getOutputFormat(sound.codec);
            std::int32_t channels = static_cast<std::int32_t>(soundChannels);
            std::int32_t encoding = 2;
            AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &channels);
            AMediaFormat_getInt32(format, "pcm-encoding", &encoding);
            AMediaFormat_delete(format);
            soundChannels = static_cast<std::uint32_t>(std::max(channels, 1));
            floatSound = encoding == 4;
            continue;
        }
        if (index < 0) {
            ++waits;
            continue;
        }
        waits = 0;
        std::size_t capacity = 0;
        const std::uint8_t* data = AMediaCodec_getOutputBuffer(sound.codec, static_cast<std::size_t>(index), &capacity) + buffer.offset;
        const std::size_t sampleSize = floatSound ? sizeof(float) : sizeof(std::int16_t);
        const std::size_t frames = static_cast<std::size_t>(buffer.size) / (sampleSize * soundChannels);
        for (std::size_t frame = 0; frame < frames; ++frame) {
            for (std::uint32_t channel = 0; channel < info.channels; ++channel) {
                const std::size_t at = (frame * soundChannels + std::min(channel, soundChannels - 1)) * sampleSize;
                if (floatSound) {
                    float value = 0.0F;
                    std::memcpy(&value, data + at, sizeof(value));
                    samples.push_back(value);
                } else {
                    std::int16_t value = 0;
                    std::memcpy(&value, data + at, sizeof(value));
                    samples.push_back(static_cast<float>(value) / 32768.0F);
                }
            }
        }
        soundTime = static_cast<double>(buffer.presentationTimeUs) / 1000000.0 + static_cast<double>(frames) / info.sampleRate;
        sound.outputEnded = (buffer.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) != 0;
        AMediaCodec_releaseOutputBuffer(sound.codec, static_cast<std::size_t>(index), false);
    }
    return sound.codec != nullptr && !sound.outputEnded;
}

void AndroidVideoDecoder::seek(double seconds) {
    const auto microseconds = static_cast<std::int64_t>(seconds * 1000000.0);
    for (Track* track : {&picture, &sound}) {
        if (track->codec == nullptr) {
            continue;
        }
        AMediaExtractor_seekTo(track->extractor, microseconds, AMEDIAEXTRACTOR_SEEK_PREVIOUS_SYNC);
        AMediaCodec_flush(track->codec);
        track->inputEnded = false;
        track->outputEnded = false;
    }
    skipUntil = seconds;
    soundTime = seconds;
}

void AndroidVideoDecoder::release(Track& track) noexcept {
    if (track.codec != nullptr) {
        AMediaCodec_stop(track.codec);
        AMediaCodec_delete(track.codec);
        track.codec = nullptr;
    }
    if (track.extractor != nullptr) {
        AMediaExtractor_delete(track.extractor);
        track.extractor = nullptr;
    }
    if (track.source != nullptr) {
        deleteSource(track.source);
        track.source = nullptr;
    }
}

} // namespace haylen::platform
