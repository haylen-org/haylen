#pragma once

#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/StreamSocket.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace haylen::test {

// A local HTTP server that serves the files of a folder the way a content delivery network serves protected content: whole or by a single byte range, with a strong validator per file and `If-Range`. It can also misbehave on purpose, ignoring ranges, answering with another status, dropping the connection part way, stalling or answering another range, and it records every request so tests see what a client asked for.
class ContentServer final {
  public:
    struct Exchange {
        std::string path;
        std::optional<std::uint64_t> first;
        std::optional<std::uint64_t> last;
        int status = 0;
        std::uint64_t bytes = 0;
    };

    explicit ContentServer(std::filesystem::path servedFolder);
    ~ContentServer();

    ContentServer(const ContentServer&) = delete;
    ContentServer& operator=(const ContentServer&) = delete;

    // The address of the folder, ending with a slash.
    [[nodiscard]] std::string getUrl() const;
    [[nodiscard]] const std::filesystem::path& getFolder() const noexcept {
        return folder;
    }

    // Gives the file a new validator, as a server does when a file changes.
    void touch(const std::string& path);

    // Answers every range with the whole file, as a server without ranges does.
    void setIgnoresRanges(bool value) noexcept {
        ignoresRanges = value;
    }

    // Answers the requests of a path with a status and no body.
    void setStatus(const std::string& path, int status);

    // Closes the connection of the next responses once they sent this many bytes of their body.
    void dropAfter(std::uint64_t bytes, int times = 1) noexcept;

    // Waits before every body, longer than any test lets a request take.
    void setStalls(bool value) noexcept {
        stalls = value;
    }

    // Answers ranges one byte later than asked.
    void setShiftsRanges(bool value) noexcept {
        shiftsRanges = value;
    }

    [[nodiscard]] std::vector<Exchange> getExchanges() const;
    [[nodiscard]] std::uint64_t getSentBytes() const;
    void clearExchanges();

  private:
    class Handler;
    class Factory;

    static constexpr int kStallMilliseconds = 3000;

    void serve(const std::string& target, const std::string& range, const std::string& ifRange, Poco::Net::HTTPServerResponse& response, Poco::Net::StreamSocket& socket);
    [[nodiscard]] std::string getValidator(const std::string& path);
    void record(Exchange exchange);

    std::filesystem::path folder;
    Poco::Net::ServerSocket listener;
    std::unique_ptr<Poco::Net::HTTPServer> server;
    std::atomic<bool> ignoresRanges = false;
    std::atomic<bool> stalls = false;
    std::atomic<bool> shiftsRanges = false;
    std::atomic<std::uint64_t> dropBytes = 0;
    std::atomic<int> drops = 0;
    std::atomic<bool> stopping = false;
    mutable std::mutex mutex;
    std::map<std::string, int> versions;
    std::map<std::string, int> statuses;
    std::vector<Exchange> exchanges;
};

} // namespace haylen::test
