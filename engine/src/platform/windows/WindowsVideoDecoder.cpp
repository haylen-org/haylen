#include "platform/windows/WindowsVideoDecoder.hpp"

#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <format>
#include <stdexcept>
#include <utility>

#include "platform/windows/WindowsPackageStream.hpp"

namespace haylen::platform {

using Microsoft::WRL::ComPtr;

// Every thread that calls Media Foundation joins the multithreaded apartment of COM and keeps it, and the reference counts of `MFStartup` and `MFShutdown` keep Media Foundation running while a decoder lives.
void WindowsVideoDecoder::enterThread() {
    (void)CoInitializeEx(nullptr, COINIT_MULTITHREADED);
}

WindowsVideoDecoder::WindowsVideoDecoder(std::shared_ptr<io::PackageReader> file, std::string_view filePath) : path(filePath) {
    enterThread();
    check(MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET), "Media Foundation did not start");
    try {
        ComPtr<IStream> stream;
        stream.Attach(new WindowsPackageStream(std::move(file)));
        ComPtr<IMFByteStream> bytes;
        check(MFCreateMFByteStreamOnStream(stream.Get(), &bytes), "its bytes could not be read");
        ComPtr<IMFAttributes> attributes;
        check(MFCreateAttributes(&attributes, 1), "the source reader could not be made");
        check(attributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE), "the source reader could not be made");
        check(MFCreateSourceReaderFromByteStream(bytes.Get(), attributes.Get(), &reader), "it is no file that this platform plays");
        readFormats();
    } catch (...) {
        reader.Reset();
        MFShutdown();
        throw;
    }
}

WindowsVideoDecoder::~WindowsVideoDecoder() {
    reader.Reset();
    MFShutdown();
}

void WindowsVideoDecoder::check(HRESULT result, std::string_view what) const {
    if (FAILED(result)) {
        throw std::runtime_error(std::format("The video \"{}\" could not be played: {}, with the error 0x{:08X} of Media Foundation.", path, what, static_cast<std::uint32_t>(result)));
    }
}

// The picture comes as 32-bit RGB and the sound as floats, and the duration comes from the presentation of the source.
void WindowsVideoDecoder::readFormats() {
    ComPtr<IMFMediaType> picture;
    check(MFCreateMediaType(&picture), "the format of its picture could not be made");
    picture->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    picture->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    check(reader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), nullptr, picture.Get()), "it holds no picture that this platform plays");
    ComPtr<IMFMediaType> current;
    check(reader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), &current), "the format of its picture is unknown");
    UINT32 width = 0;
    UINT32 height = 0;
    check(MFGetAttributeSize(current.Get(), MF_MT_FRAME_SIZE, &width, &height), "the size of its picture is unknown");
    info.width = static_cast<int>(width);
    info.height = static_cast<int>(height);
    UINT32 defaultStride = 0;
    stride = SUCCEEDED(current->GetUINT32(MF_MT_DEFAULT_STRIDE, &defaultStride)) ? static_cast<LONG>(defaultStride) : static_cast<long>(width) * 4;

    PROPVARIANT duration;
    PropVariantInit(&duration);
    if (SUCCEEDED(reader->GetPresentationAttribute(static_cast<DWORD>(MF_SOURCE_READER_MEDIASOURCE), MF_PD_DURATION, &duration)) && duration.vt == VT_UI8) {
        info.duration = static_cast<double>(duration.uhVal.QuadPart) / kUnitsPerSecond;
    }
    PropVariantClear(&duration);

    ComPtr<IMFMediaType> sound;
    check(MFCreateMediaType(&sound), "the format of its sound could not be made");
    sound->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    sound->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
    if (FAILED(reader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), nullptr, sound.Get()))) {
        reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), FALSE);
        soundEnded = true;
        return;
    }
    ComPtr<IMFMediaType> decoded;
    check(reader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), &decoded), "the format of its sound is unknown");
    soundChannels = MFGetAttributeUINT32(decoded.Get(), MF_MT_AUDIO_NUM_CHANNELS, 0);
    info.sampleRate = MFGetAttributeUINT32(decoded.Get(), MF_MT_AUDIO_SAMPLES_PER_SECOND, 0);
    info.channels = std::min<std::uint32_t>(soundChannels, 2);
    soundEnded = info.channels == 0 || info.sampleRate == 0;
}

// A stride below zero means the rows come from the bottom up, and 32-bit RGB holds blue, green and red with an unused byte.
VideoDecoder::Read WindowsVideoDecoder::readFrame(Frame& frame) {
    enterThread();
    while (true) {
        DWORD flags = 0;
        LONGLONG timestamp = 0;
        ComPtr<IMFSample> sample;
        check(reader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), 0, nullptr, &flags, &timestamp, &sample), "its picture could not be decoded");
        if ((flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0) {
            return Read::End;
        }
        if ((flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) != 0) {
            readFormats();
        }
        if (!sample) {
            continue;
        }
        ComPtr<IMFMediaBuffer> buffer;
        check(sample->ConvertToContiguousBuffer(&buffer), "its picture could not be read");
        BYTE* data = nullptr;
        DWORD length = 0;
        check(buffer->Lock(&data, nullptr, &length), "its picture could not be read");
        const auto width = static_cast<std::size_t>(info.width);
        const auto height = static_cast<std::size_t>(info.height);
        const std::size_t rowBytes = static_cast<std::size_t>(std::abs(stride));
        if (length < rowBytes * height) {
            buffer->Unlock();
            throw std::runtime_error("The video \"" + path + "\" gave a picture smaller than its size.");
        }
        frame.pixels.resize(width * height * 4U);
        for (std::size_t row = 0; row < height; ++row) {
            const std::size_t sourceRow = stride < 0 ? height - 1 - row : row;
            const BYTE* from = data + sourceRow * rowBytes;
            std::uint8_t* to = frame.pixels.data() + row * width * 4U;
            for (std::size_t column = 0; column < width; ++column, from += 4, to += 4) {
                to[0] = from[2];
                to[1] = from[1];
                to[2] = from[0];
                to[3] = 255;
            }
        }
        buffer->Unlock();
        frame.width = info.width;
        frame.height = info.height;
        frame.time = static_cast<double>(timestamp) / kUnitsPerSecond;
        return Read::Frame;
    }
}

bool WindowsVideoDecoder::readAudio(std::vector<float>& samples, double until) {
    enterThread();
    while (!soundEnded && soundTime < until) {
        DWORD flags = 0;
        LONGLONG timestamp = 0;
        ComPtr<IMFSample> sample;
        check(reader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), 0, nullptr, &flags, &timestamp, &sample), "its sound could not be decoded");
        if ((flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0) {
            soundEnded = true;
            break;
        }
        if (!sample) {
            continue;
        }
        ComPtr<IMFMediaBuffer> buffer;
        check(sample->ConvertToContiguousBuffer(&buffer), "its sound could not be read");
        BYTE* data = nullptr;
        DWORD length = 0;
        check(buffer->Lock(&data, nullptr, &length), "its sound could not be read");
        const std::size_t frames = length / (sizeof(float) * soundChannels);
        for (std::size_t index = 0; index < frames; ++index) {
            for (std::uint32_t channel = 0; channel < info.channels; ++channel) {
                float value = 0.0F;
                std::memcpy(&value, data + (index * soundChannels + channel) * sizeof(float), sizeof(value));
                samples.push_back(value);
            }
        }
        buffer->Unlock();
        soundTime = static_cast<double>(timestamp) / kUnitsPerSecond + static_cast<double>(frames) / info.sampleRate;
    }
    return !soundEnded;
}

void WindowsVideoDecoder::seek(double seconds) {
    enterThread();
    PROPVARIANT position;
    PropVariantInit(&position);
    position.vt = VT_I8;
    position.hVal.QuadPart = static_cast<LONGLONG>(seconds * kUnitsPerSecond);
    const HRESULT result = reader->SetCurrentPosition(GUID_NULL, position);
    PropVariantClear(&position);
    check(result, "it could not seek");
    soundTime = seconds;
    soundEnded = info.channels == 0;
}

} // namespace haylen::platform
