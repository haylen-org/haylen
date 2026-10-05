#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "content/ContentBuilder.hpp"
#include "content/Delivery.hpp"
#include "content/KeyStore.hpp"
#include "content/ReleaseInspector.hpp"
#include "content/format/Manifest.hpp"

namespace haylen::content {

// The command line of `haylen-content`, the host tool that `haylen.py` runs to create the keys of apps and to build, verify, inspect, compare, publish and compact their protected releases with the format library of the engine. Every command takes the key folder of the app it works on, and nothing it prints holds a key.
class ContentTool final {
  public:
    ContentTool(std::ostream& output, std::ostream& errors);

    // Runs a command line without the name of the tool, and returns the exit code of the process.
    int run(std::span<const std::string> arguments);

  private:
    // A compaction keeps the current generation of every channel and the one before it, which apps that are still downloading it need.
    static constexpr std::uint64_t kDefaultKeep = 2;

    // The words of a command line: the positional arguments in order and the options by name, each of which a command takes once.
    class Arguments final {
      public:
        explicit Arguments(std::span<const std::string> words);

        [[nodiscard]] std::string takePositional(std::string_view name);
        [[nodiscard]] std::string takeOption(std::string_view name);
        [[nodiscard]] std::optional<std::string> takeOptional(std::string_view name);
        [[nodiscard]] std::optional<std::uint64_t> takeOptionalNumber(std::string_view name);
        [[nodiscard]] std::uint64_t takeNumber(std::string_view name);
        [[nodiscard]] bool takeFlag(std::string_view name);

        // Throws for the words that no part of the command took.
        void finish() const;

      private:
        std::vector<std::string> positionals;
        std::map<std::string, std::string, std::less<>> options;
        std::vector<std::string> flags;
    };

    void runKeys(Arguments& arguments);
    void runBuild(Arguments& arguments);
    void runVerify(Arguments& arguments);
    void runInspect(Arguments& arguments);
    void runDiff(Arguments& arguments);
    void runPublish(Arguments& arguments);
    void runCompact(Arguments& arguments);

    void printKeys(const KeyStore& store);
    void printDomain(const ReleaseInspector::Domain& domain, bool chunks);
    void printStatistics(std::string_view domain, const ContentBuilder::Statistics& statistics);

    [[nodiscard]] static ReleaseInspector makeInspector(const KeyStore& store);
    [[nodiscard]] static std::string_view getDeliveryName(Delivery delivery) noexcept;
    [[nodiscard]] static std::string_view getDomainName(Manifest::Domain domain) noexcept;

    std::ostream& output;
    std::ostream& errors;
};

} // namespace haylen::content
