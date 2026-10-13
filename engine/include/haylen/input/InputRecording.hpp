#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace haylen::input {

// The buttons of every player on every fixed step of a run, with what a replay needs to reproduce it: the names of the buttons, the identifier of the app, the fixed rate, data of the app such as the states of its random generators, and an optional hash of the state of the app on each step that tells where a replay diverges. A step holds one mask per player whose bit `n` is the button `n`, which `ActionMap::isStepDown` or `CommandBuffer` read.
class InputRecording final {
  public:
    static constexpr std::size_t kMaxPlayers = 16;
    static constexpr std::size_t kMaxButtons = 32;
    static constexpr std::size_t kMaxNameSize = 255;
    static constexpr std::size_t kMaxDataSize = 1U << 20U;
    static constexpr std::uint32_t kMaxSteps = 1U << 26U;

    struct Header {
        std::vector<std::string> buttons;
        std::size_t players = 1;
        std::string identifier;
        double fixedRate = 60.0;
        std::string data;
    };

    // Throws `std::invalid_argument` for a header without buttons or players, with more than the bounds above, or with a name longer than `kMaxNameSize`.
    explicit InputRecording(Header value);

    // Sets the mask of a player for the step that `advance` closes, 0 until it is set.
    void write(std::size_t player, std::uint32_t mask);

    // Closes the step with the masks written for it, and with the hash of the state of the app when one is given. Throws `std::length_error` past `kMaxSteps`.
    void advance(std::optional<std::uint32_t> hash = std::nullopt);

    [[nodiscard]] const Header& getHeader() const noexcept {
        return header;
    }
    [[nodiscard]] std::uint32_t getStepCount() const noexcept {
        return stepCount;
    }
    [[nodiscard]] std::uint32_t getMask(std::uint32_t step, std::size_t player) const;
    [[nodiscard]] std::optional<std::uint32_t> getHash(std::uint32_t step) const;

    // Encodes the recording with each run of equal steps as one entry, so the steps of a player who holds a button cost a few bytes.
    [[nodiscard]] std::vector<std::byte> toBytes() const;

    // Throws `std::invalid_argument` with `The input recording is malformed: <reason>.` for bytes that are not a whole recording of this format.
    [[nodiscard]] static InputRecording fromBytes(std::span<const std::byte> bytes);

  private:
    static constexpr std::uint8_t kVersion = 1;
    static constexpr std::uint32_t kMagic = 0x52495948;

    // Steps that repeat the same masks, which the masks of the runs hold once per run, so memory follows the changes of the input and never the length of a run.
    struct Run {
        std::uint32_t step = 0;
        std::uint32_t count = 0;
    };

    static void validate(const Header& header);
    static void putVarint(std::vector<std::byte>& bytes, std::uint64_t value);
    static void putText(std::vector<std::byte>& bytes, const std::string& text);
    [[nodiscard]] static std::uint64_t takeVarint(std::span<const std::byte> bytes, std::size_t& offset, std::uint64_t maximum);
    [[nodiscard]] static std::string takeText(std::span<const std::byte> bytes, std::size_t& offset, std::size_t maximum);
    void appendRun(std::uint32_t count);

    Header header;
    std::vector<Run> runs;
    std::vector<std::uint32_t> masks;
    std::vector<std::uint32_t> pending;
    std::vector<std::uint32_t> hashes;
    std::uint32_t stepCount = 0;
    bool hashed = false;
};

} // namespace haylen::input
