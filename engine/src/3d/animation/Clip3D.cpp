#include "haylen/3d/animation/Clip3D.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <set>
#include <stdexcept>
#include <utility>

#include "3d/animation/GltfClip3DReader.hpp"

namespace haylen::animation3d {

Clip3D::Clip3D(Description description) : name(std::move(description.name)), tracks(std::move(description.tracks)), events(std::move(description.events)) {
    float end = 0.0F;
    std::set<std::pair<std::string_view, std::string_view>> animated;
    for (const Track3D& track : tracks) {
        if (!animated.emplace(track.getTarget(), track.getProperty()).second) {
            throw std::invalid_argument(std::format("The clip \"{}\" holds {} twice, while a clip animates each property of a target once.", name, track.describe()));
        }

        offsets.push_back(valueCount);
        valueCount += track.getComponentCount();
        end = std::max(end, track.getEndTime());
    }

    for (const Event& event : events) {
        if (!std::isfinite(event.time) || event.time < 0.0F) {
            throw std::invalid_argument(std::format("The clip \"{}\" has the event \"{}\" at {} seconds, while events come at finite times of zero or more seconds.", name, event.name, event.time));
        }

        if (event.name.empty()) {
            throw std::invalid_argument(std::format("The clip \"{}\" has an event without a name at {} seconds. Give every event a name.", name, event.time));
        }

        end = std::max(end, event.time);
    }

    std::ranges::stable_sort(events, {}, &Event::time);
    duration = description.duration.value_or(end);
    if (!std::isfinite(duration) || duration < end) {
        throw std::invalid_argument(std::format("The clip \"{}\" lasts {} seconds, while its keys and events run until {} seconds.", name, duration, end));
    }
}

Clip3D Clip3D::fromGltf(const gltf::Document& document, std::size_t animation) {
    return GltfClip3DReader::read(document, animation);
}

Clip3D Clip3D::fromMorphSequence(const MorphSequence& sequence) {
    if (sequence.targetCount == 0) {
        throw std::invalid_argument(std::format("The morph sequence \"{}\" has no targets, while it plays at least one.", sequence.name));
    }

    if (!std::isfinite(sequence.framesPerSecond) || sequence.framesPerSecond <= 0.0F) {
        throw std::invalid_argument(std::format("The morph sequence \"{}\" plays at a positive number of frames per second, while it was given {}.", sequence.name, sequence.framesPerSecond));
    }

    const std::size_t keys = sequence.targetCount + (sequence.loop ? 1 : 0);
    if (sequence.targetCount > Track3D::kMaxKeys / keys) {
        throw std::invalid_argument(std::format("The morph sequence \"{}\" has {} targets, whose {} keys of weights pass the {} weights that a sequence holds at most.", sequence.name, sequence.targetCount, keys, Track3D::kMaxKeys));
    }

    std::vector<float> times(keys);
    std::vector<float> values(keys * sequence.targetCount, 0.0F);
    for (std::size_t key = 0; key < keys; ++key) {
        times[key] = static_cast<float>(static_cast<double>(key) / sequence.framesPerSecond);
        values[key * sequence.targetCount + key % sequence.targetCount] = 1.0F;
    }

    Track3D track({.target = sequence.target, .property = "weights", .type = Track3D::Type::Weights, .interpolation = Track3D::Interpolation::Linear, .times = std::move(times), .values = std::move(values)});
    std::vector<Track3D> sequenceTracks;
    sequenceTracks.push_back(std::move(track));
    return Clip3D({.name = sequence.name, .tracks = std::move(sequenceTracks)});
}

std::size_t Clip3D::getValueOffset(std::size_t track) const {
    if (track >= offsets.size()) {
        throw std::out_of_range(std::format("The clip \"{}\" has {} tracks, so it has no track {}.", name, offsets.size(), track));
    }

    return offsets[track];
}

std::optional<std::size_t> Clip3D::findTrack(std::string_view target, std::string_view property) const noexcept {
    for (std::size_t index = 0; index < tracks.size(); ++index) {
        if (tracks[index].getTarget() == target && tracks[index].getProperty() == property) {
            return index;
        }
    }

    return std::nullopt;
}

std::size_t Clip3D::getMemorySize() const noexcept {
    std::size_t size = 0;
    for (const Track3D& track : tracks) {
        size += track.getMemorySize();
    }

    return size;
}

void Clip3D::sample(float time, std::span<float> values, std::span<std::size_t> cursors) const {
    if (values.size() < valueCount || cursors.size() < tracks.size()) {
        throw std::invalid_argument(std::format("A sample of the clip \"{}\" takes {} values and {} cursors, while {} values and {} cursors were given.", name, valueCount, tracks.size(), values.size(), cursors.size()));
    }

    for (std::size_t index = 0; index < tracks.size(); ++index) {
        tracks[index].sample(time, values.subspan(offsets[index], tracks[index].getComponentCount()), cursors[index]);
    }
}

Clip3D Clip3D::renamed(std::string value) const {
    return Clip3D({.name = std::move(value), .tracks = tracks, .events = events, .duration = duration});
}

Clip3D Clip3D::trimmed(float start, float end) const {
    if (!std::isfinite(start) || !std::isfinite(end) || start < 0.0F || end < start || end > duration) {
        throw std::invalid_argument(std::format("A trim of the clip \"{}\" runs within its {} seconds from a start to an end at or after it, while it was given {} and {}.", name, duration, start, end));
    }

    std::vector<Track3D> parts;
    parts.reserve(tracks.size());
    for (const Track3D& track : tracks) {
        parts.push_back(track.trimmed(start, end));
    }

    std::vector<Event> kept;
    for (const Event& event : events) {
        if (event.time >= start && event.time <= end) {
            kept.push_back({.time = event.time - start, .name = event.name, .data = event.data});
        }
    }

    return Clip3D({.name = name, .tracks = std::move(parts), .events = std::move(kept), .duration = end - start});
}

Clip3D Clip3D::joined(const Clip3D& next) const {
    std::vector<Track3D> joinedTracks;
    joinedTracks.reserve(tracks.size() + next.tracks.size());
    for (const Track3D& track : tracks) {
        const std::optional<std::size_t> other = next.findTrack(track.getTarget(), track.getProperty());
        joinedTracks.push_back(other ? track.appended(next.tracks[*other], duration) : track);
    }

    for (const Track3D& track : next.tracks) {
        if (!findTrack(track.getTarget(), track.getProperty())) {
            joinedTracks.push_back(track.shifted(duration));
        }
    }

    std::vector<Event> joinedEvents = events;
    for (const Event& event : next.events) {
        joinedEvents.push_back({.time = event.time + duration, .name = event.name, .data = event.data});
    }

    return Clip3D({.name = name, .tracks = std::move(joinedTracks), .events = std::move(joinedEvents), .duration = duration + next.duration});
}

Clip3D Clip3D::resampled(float framesPerSecond) const {
    std::vector<Track3D> resampledTracks;
    resampledTracks.reserve(tracks.size());
    for (const Track3D& track : tracks) {
        resampledTracks.push_back(track.getInterpolation() == Track3D::Interpolation::Step ? track : track.resampled(framesPerSecond, 0.0F, duration));
    }

    return Clip3D({.name = name, .tracks = std::move(resampledTracks), .events = events, .duration = duration});
}

Clip3D Clip3D::optimized(float tolerance) const {
    std::vector<Track3D> optimizedTracks;
    optimizedTracks.reserve(tracks.size());
    for (const Track3D& track : tracks) {
        optimizedTracks.push_back(track.optimized(tolerance));
    }

    return Clip3D({.name = name, .tracks = std::move(optimizedTracks), .events = events, .duration = duration});
}

// Half of the tolerance goes to the keys that the optimization drops and half to the steps of the values, which turn a rotation by up to twice the error of a component.
Clip3D Clip3D::compressed(float tolerance) const {
    const Clip3D fewer = optimized(tolerance * 0.5F);
    std::vector<Track3D> compressedTracks;
    compressedTracks.reserve(fewer.tracks.size());
    for (const Track3D& track : fewer.tracks) {
        const bool numbers = track.getType() != Track3D::Type::Boolean && track.getType() != Track3D::Type::String && track.getInterpolation() != Track3D::Interpolation::Bezier;
        if (!numbers) {
            compressedTracks.push_back(track);
            continue;
        }

        Track3D steps = track.quantized();
        const float scale = track.getType() == Track3D::Type::Quat ? 2.0F : 1.0F;
        compressedTracks.push_back(steps.getQuantizationError() * scale <= tolerance * 0.5F ? std::move(steps) : track);
    }

    return Clip3D({.name = name, .tracks = std::move(compressedTracks), .events = events, .duration = duration});
}

Clip3D Clip3D::toAdditive(const Clip3D& reference, float time) const {
    std::vector<Track3D> relative;
    relative.reserve(tracks.size());
    for (const Track3D& track : tracks) {
        if (track.getType() == Track3D::Type::Boolean || track.getType() == Track3D::Type::String) {
            relative.push_back(track);
            continue;
        }

        const std::optional<std::size_t> found = reference.findTrack(track.getTarget(), track.getProperty());
        if (!found) {
            throw std::invalid_argument(std::format("The reference clip \"{}\" has no {}, which the additive clip of \"{}\" needs.", reference.name, track.describe(), name));
        }

        const Track3D& base = reference.tracks[*found];
        std::vector<float> pose(base.getComponentCount());
        std::size_t cursor = 0;
        base.sample(time, pose, cursor);
        relative.push_back(track.relativeTo(pose));
    }

    return Clip3D({.name = name, .tracks = std::move(relative), .events = events, .duration = duration});
}

} // namespace haylen::animation3d
