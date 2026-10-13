#include "haylen/input/InputRecording.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace haylen::input {

InputRecording::InputRecording(Header value) : header(std::move(value)) {
    validate(header);
    pending.assign(header.players, 0);
}

void InputRecording::validate(const Header& header) {
    if (header.buttons.empty() || header.buttons.size() > kMaxButtons) {
        throw std::invalid_argument("An input recording needs from 1 to " + std::to_string(kMaxButtons) + " buttons.");
    }
    if (header.players == 0 || header.players > kMaxPlayers) {
        throw std::invalid_argument("An input recording needs from 1 to " + std::to_string(kMaxPlayers) + " players.");
    }
    const auto tooLong = [](const std::string& name) { return name.size() > kMaxNameSize; };
    if (std::ranges::any_of(header.buttons, tooLong) || tooLong(header.identifier)) {
        throw std::invalid_argument("The names of an input recording have at most " + std::to_string(kMaxNameSize) + " bytes.");
    }
    if (header.data.size() > kMaxDataSize) {
        throw std::invalid_argument("The data of an input recording has at most " + std::to_string(kMaxDataSize) + " bytes.");
    }
    if (!std::isfinite(header.fixedRate) || header.fixedRate <= 0.0) {
        throw std::invalid_argument("The fixed rate of an input recording must be positive.");
    }
}

void InputRecording::write(std::size_t player, std::uint32_t mask) {
    if (player >= header.players) {
        throw std::out_of_range("The input recording has no player " + std::to_string(player + 1) + ".");
    }
    pending[player] = mask;
}

void InputRecording::advance(std::optional<std::uint32_t> hash) {
    if (stepCount >= kMaxSteps) {
        throw std::length_error("An input recording holds at most " + std::to_string(kMaxSteps) + " steps.");
    }
    if (hash && !hashed) {
        hashed = true;
        hashes.assign(stepCount, 0);
    }
    if (hashed) {
        hashes.push_back(hash.value_or(0));
    }
    appendRun(1);
}

// Steps that repeat the masks of the last run extend it.
void InputRecording::appendRun(std::uint32_t count) {
    const std::size_t players = header.players;
    if (!runs.empty() && std::equal(pending.begin(), pending.end(), masks.end() - static_cast<std::ptrdiff_t>(players))) {
        runs.back().count += count;
    } else {
        runs.push_back({.step = stepCount, .count = count});
        masks.insert(masks.end(), pending.begin(), pending.end());
    }
    stepCount += count;
}

std::uint32_t InputRecording::getMask(std::uint32_t step, std::size_t player) const {
    if (step >= stepCount || player >= header.players) {
        throw std::out_of_range("The input recording has no step " + std::to_string(step + 1) + " for player " + std::to_string(player + 1) + ".");
    }
    const auto after = std::ranges::upper_bound(runs, step, {}, &Run::step);
    const auto run = static_cast<std::size_t>(after - runs.begin()) - 1;
    return masks[run * header.players + player];
}

std::optional<std::uint32_t> InputRecording::getHash(std::uint32_t step) const {
    if (!hashed || step >= stepCount) {
        return std::nullopt;
    }
    return hashes[step];
}

void InputRecording::putVarint(std::vector<std::byte>& bytes, std::uint64_t value) {
    do {
        const auto low = static_cast<std::uint8_t>(value & 0x7FU);
        value >>= 7U;
        bytes.push_back(static_cast<std::byte>(value != 0 ? low | 0x80U : low));
    } while (value != 0);
}

void InputRecording::putText(std::vector<std::byte>& bytes, const std::string& text) {
    putVarint(bytes, text.size());
    for (const char character : text) {
        bytes.push_back(static_cast<std::byte>(character));
    }
}

std::vector<std::byte> InputRecording::toBytes() const {
    std::vector<std::byte> bytes;
    putVarint(bytes, kMagic);
    putVarint(bytes, kVersion);
    putVarint(bytes, header.buttons.size());
    for (const std::string& button : header.buttons) {
        putText(bytes, button);
    }
    putVarint(bytes, header.players);
    putText(bytes, header.identifier);
    putVarint(bytes, std::bit_cast<std::uint64_t>(header.fixedRate));
    putText(bytes, header.data);
    putVarint(bytes, stepCount);
    putVarint(bytes, hashed ? 1U : 0U);

    // Each run is the number of steps that repeat the same masks, followed by those masks.
    for (std::size_t run = 0; run < runs.size(); ++run) {
        putVarint(bytes, runs[run].count);
        for (std::size_t player = 0; player < header.players; ++player) {
            putVarint(bytes, masks[run * header.players + player]);
        }
    }
    for (const std::uint32_t hash : hashes) {
        putVarint(bytes, hash);
    }
    return bytes;
}

std::uint64_t InputRecording::takeVarint(std::span<const std::byte> bytes, std::size_t& offset, std::uint64_t maximum) {
    std::uint64_t value = 0;
    for (unsigned shift = 0; shift < 64; shift += 7) {
        if (offset >= bytes.size()) {
            throw std::invalid_argument("The input recording is malformed: it ends before its last number.");
        }
        const auto byte = static_cast<std::uint8_t>(bytes[offset++]);
        value |= static_cast<std::uint64_t>(byte & 0x7FU) << shift;
        if ((byte & 0x80U) == 0) {
            if (value > maximum) {
                throw std::invalid_argument("The input recording is malformed: a number is out of range.");
            }
            return value;
        }
    }
    throw std::invalid_argument("The input recording is malformed: a number is too long.");
}

std::string InputRecording::takeText(std::span<const std::byte> bytes, std::size_t& offset, std::size_t maximum) {
    const auto size = static_cast<std::size_t>(takeVarint(bytes, offset, maximum));
    if (bytes.size() - offset < size) {
        throw std::invalid_argument("The input recording is malformed: it ends inside a text.");
    }
    std::string text(reinterpret_cast<const char*>(bytes.data() + offset), size);
    offset += size;
    return text;
}

InputRecording InputRecording::fromBytes(std::span<const std::byte> bytes) {
    std::size_t offset = 0;
    if (takeVarint(bytes, offset, std::numeric_limits<std::uint32_t>::max()) != kMagic) {
        throw std::invalid_argument("The input recording is malformed: it does not start like one.");
    }
    if (takeVarint(bytes, offset, 255) != kVersion) {
        throw std::invalid_argument("The input recording is malformed: its version is not one this engine reads.");
    }

    Header header;
    header.buttons.resize(static_cast<std::size_t>(takeVarint(bytes, offset, kMaxButtons)));
    for (std::string& button : header.buttons) {
        button = takeText(bytes, offset, kMaxNameSize);
    }
    header.players = static_cast<std::size_t>(takeVarint(bytes, offset, kMaxPlayers));
    header.identifier = takeText(bytes, offset, kMaxNameSize);
    header.fixedRate = std::bit_cast<double>(takeVarint(bytes, offset, std::numeric_limits<std::uint64_t>::max()));
    header.data = takeText(bytes, offset, kMaxDataSize);
    try {
        validate(header);
    } catch (const std::invalid_argument& error) {
        throw std::invalid_argument(std::string("The input recording is malformed: ") + error.what());
    }
    InputRecording recording(std::move(header));

    const auto steps = static_cast<std::uint32_t>(takeVarint(bytes, offset, kMaxSteps));
    const bool hashed = takeVarint(bytes, offset, 1) == 1;
    const std::size_t players = recording.header.players;
    while (recording.stepCount < steps) {
        const auto run = static_cast<std::uint32_t>(takeVarint(bytes, offset, steps - recording.stepCount));
        if (run == 0) {
            throw std::invalid_argument("The input recording is malformed: a run has no steps.");
        }
        for (std::size_t player = 0; player < players; ++player) {
            recording.pending[player] = static_cast<std::uint32_t>(takeVarint(bytes, offset, std::numeric_limits<std::uint32_t>::max()));
        }
        recording.appendRun(run);
    }
    if (hashed) {
        recording.hashed = true;
        recording.hashes.reserve(std::min<std::size_t>(steps, bytes.size() - offset));
        for (std::uint32_t step = 0; step < steps; ++step) {
            recording.hashes.push_back(static_cast<std::uint32_t>(takeVarint(bytes, offset, std::numeric_limits<std::uint32_t>::max())));
        }
    }
    if (offset != bytes.size()) {
        throw std::invalid_argument("The input recording is malformed: bytes follow its last step.");
    }
    std::ranges::fill(recording.pending, 0U);
    return recording;
}

} // namespace haylen::input
