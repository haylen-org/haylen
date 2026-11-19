#include "audio/SoundData.hpp"

#include <utility>

namespace haylen::audio {

debug::ObjectCounter& SoundData::counter = *new debug::ObjectCounter("Sound", debug::ObjectCounter::Kind::Native);

SoundData::SoundData(Content value) : content(std::make_shared<const Content>(std::move(value))) {
    tracked.setBytes(content->samples.size() * sizeof(float));
}

std::shared_ptr<const SoundData::Content> SoundData::getContent() const {
    const std::scoped_lock lock(mutex);
    return content;
}

void SoundData::replace(const SoundData& value) {
    std::shared_ptr<const Content> fresh = value.getContent();
    tracked.setBytes(fresh->samples.size() * sizeof(float));
    const std::scoped_lock lock(mutex);
    content = std::move(fresh);
}

} // namespace haylen::audio
