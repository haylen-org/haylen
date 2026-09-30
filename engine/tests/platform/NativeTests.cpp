#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/platform/NativeLibraries.hpp"
#include "platform/native/NativeSignature.hpp"
#include "support/TemporaryDirectory.hpp"

namespace haylen::platform {

class NativeSignatureTest : public ::testing::Test {
  protected:
    using Kind = NativeSignature::Parameter::Kind;

    [[nodiscard]] static std::string failureOf(std::string_view declaration) {
        try {
            (void)NativeSignature::parse(declaration);
        } catch (const std::invalid_argument& error) {
            return error.what();
        }
        return {};
    }
};

class NativeLibrariesTest : public ::testing::Test {
  protected:
    static int linkedTwice(int value) {
        return value * 2;
    }
};

TEST_F(NativeSignatureTest, ReadsNumbersPointersTextAndRangesOfBytes) {
    const NativeSignature signature = NativeSignature::parse("void (int32_t code, const char* text, const uint8_t data[size], size_t size, struct Session* session, double ratio, bool ready, unsigned long long big, float scale, const int16_t pair[2], char const * label, char* buffer, enum Mode mode)");
    const std::vector<NativeSignature::Parameter>& parameters = signature.getParameters();
    ASSERT_EQ(parameters.size(), 13U);

    EXPECT_EQ(parameters[0].kind, Kind::Integer);
    EXPECT_EQ(parameters[0].size, 4U);
    EXPECT_TRUE(parameters[0].isSigned);
    EXPECT_EQ(parameters[0].name, "code");
    EXPECT_EQ(parameters[1].kind, Kind::Text);
    EXPECT_EQ(parameters[2].kind, Kind::Bytes);
    EXPECT_EQ(parameters[2].size, 1U);
    EXPECT_EQ(parameters[2].lengthParameter, 3U);
    EXPECT_EQ(parameters[3].kind, Kind::Integer);
    EXPECT_FALSE(parameters[3].isSigned);
    EXPECT_EQ(parameters[3].size, sizeof(std::size_t));
    EXPECT_EQ(parameters[4].kind, Kind::Pointer);
    EXPECT_EQ(parameters[5].kind, Kind::Double);
    EXPECT_EQ(parameters[6].kind, Kind::Boolean);
    EXPECT_EQ(parameters[7].size, 8U);
    EXPECT_FALSE(parameters[7].isSigned);
    EXPECT_EQ(parameters[8].kind, Kind::Float);
    EXPECT_EQ(parameters[9].kind, Kind::Bytes);
    EXPECT_EQ(parameters[9].size, 2U);
    EXPECT_EQ(parameters[9].count, 2U);
    EXPECT_FALSE(parameters[9].lengthParameter.has_value());
    EXPECT_EQ(parameters[10].kind, Kind::Text);
    EXPECT_EQ(parameters[11].kind, Kind::Pointer) << "Only constant text is copied as a string.";
    EXPECT_EQ(parameters[12].kind, Kind::Integer);

    EXPECT_TRUE(NativeSignature::parse("void (void)").getParameters().empty());
    EXPECT_TRUE(NativeSignature::parse("void()").getParameters().empty());
    EXPECT_EQ(NativeSignature::parse("void (unsigned count)").getParameters()[0].name, "count");
}

TEST_F(NativeSignatureTest, RejectsWhatACallbackCannotReceive) {
    EXPECT_NE(failureOf("int (int value)").find("returns nothing"), std::string::npos);
    EXPECT_NE(failureOf("void* (int value)").find("returns nothing"), std::string::npos);
    EXPECT_NE(failureOf("void (Session session)").find("not a C number type"), std::string::npos);
    EXPECT_NE(failureOf("void (struct Session session)").find("not a C number type"), std::string::npos);
    EXPECT_NE(failureOf("void (void value)").find("not a C number type"), std::string::npos);
    EXPECT_NE(failureOf("void (const void data[4])").find("does not know"), std::string::npos);
    EXPECT_NE(failureOf("void (const uint8_t data[size], double size)").find("must name an integer parameter"), std::string::npos);
    EXPECT_NE(failureOf("void (const uint8_t data[missing])").find("must name an integer parameter"), std::string::npos);
    EXPECT_NE(failureOf("void (const uint8_t data[])").find("needs a count"), std::string::npos);
    EXPECT_NE(failureOf("void (long double value)").find("unknown type \"long double\""), std::string::npos);
    EXPECT_NE(failureOf("void (int value) extra").find("continues after its parameters"), std::string::npos);
    EXPECT_NE(failureOf("void (int value; int other)").find("unexpected character \";\""), std::string::npos);
    EXPECT_NE(failureOf("void (int first int second)").find("unknown type \"int first int\""), std::string::npos);
    EXPECT_NE(failureOf("void (int first[2] int second)").find("\"int\" where a comma or \")\" belongs"), std::string::npos);
    EXPECT_NE(failureOf("void (int value,)").find("a comma without a parameter"), std::string::npos);
    EXPECT_NE(failureOf("void (int value").find("ends before the \")\""), std::string::npos);
    EXPECT_NE(failureOf("void (,)").find("has no type"), std::string::npos);
}

TEST_F(NativeLibrariesTest, NamesTheFilesOfALibraryOnThePlatform) {
    EXPECT_EQ(NativeLibraries::getFileNames("steam_api64.dll"), (std::vector<std::string>{"steam_api64.dll"}));
    EXPECT_EQ(NativeLibraries::getFileNames("libsteam_api.so.1"), (std::vector<std::string>{"libsteam_api.so.1"}));
#if defined(_WIN32)
    EXPECT_EQ(NativeLibraries::getFileNames("native_test"), (std::vector<std::string>{"native_test.dll"}));
#elif defined(__APPLE__)
    EXPECT_EQ(NativeLibraries::getFileNames("native_test"), (std::vector<std::string>{"libnative_test.dylib", "native_test.framework/native_test"}));
#else
    EXPECT_EQ(NativeLibraries::getFileNames("native_test"), (std::vector<std::string>{"libnative_test.so"}));
#endif
    EXPECT_TRUE(NativeLibraries::isAvailable());
}

TEST_F(NativeLibrariesTest, LoadsByPathAndByNameNextToTheExecutable) {
    const NativeLibraries::Library byPath = NativeLibraries::open(HAYLEN_NATIVE_TEST_LIBRARY);
    ASSERT_NE(byPath.handle, nullptr);
    EXPECT_FALSE(byPath.linked);
    auto* add = reinterpret_cast<std::int32_t (*)(std::int32_t, std::int32_t)>(NativeLibraries::findSymbol(byPath, "native_test_add"));
    ASSERT_NE(add, nullptr);
    EXPECT_EQ(add(20, 22), 42);
    EXPECT_EQ(NativeLibraries::findSymbol(byPath, "native_test_missing"), nullptr);

    // The tests run next to the library, which is where a packaged app keeps its libraries on desktop platforms.
    const NativeLibraries::Library byName = NativeLibraries::open("native_test");
    EXPECT_EQ(byName.handle, byPath.handle);
    EXPECT_EQ(std::filesystem::path(byName.path).filename(), std::filesystem::path(HAYLEN_NATIVE_TEST_LIBRARY).filename());
    EXPECT_EQ(NativeLibraries::findSymbol("native_test_add"), reinterpret_cast<void*>(add));
    EXPECT_EQ(NativeLibraries::findSymbol("native_test_nowhere"), nullptr);
}

TEST_F(NativeLibrariesTest, SearchesAddedFoldersBeforeThePlatformFolders) {
    const test::TemporaryDirectory folder;
    const std::filesystem::path library(HAYLEN_NATIVE_TEST_LIBRARY);
    const std::string copy = NativeLibraries::getFileNames("native_copy").front();
    std::filesystem::copy_file(library, folder.getPath() / copy);

    EXPECT_THROW((void)NativeLibraries::open("native_copy"), std::runtime_error);
    NativeLibraries::addSearchFolder(folder.getPath());
    const NativeLibraries::Library loaded = NativeLibraries::open("native_copy");
    EXPECT_NE(loaded.handle, nullptr);
    EXPECT_NE(NativeLibraries::findSymbol(loaded, "native_test_checksum"), nullptr);
}

TEST_F(NativeLibrariesTest, ListsEveryPlaceItSearched) {
    try {
        (void)NativeLibraries::open("native_nowhere");
        FAIL() << "A missing library loads.";
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("The native library \"native_nowhere\" could not be loaded. These are the places it searched:"), std::string::npos);
        EXPECT_NE(message.find(std::filesystem::path(HAYLEN_NATIVE_TEST_LIBRARY).parent_path().string()), std::string::npos);
        EXPECT_NE(message.find("not found"), std::string::npos);
        EXPECT_NE(message.find("the libraries linked into the app: not registered"), std::string::npos);
    }

    try {
        (void)NativeLibraries::open("/nowhere/libnative_nowhere.so");
        FAIL() << "A missing path loads.";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string(error.what()).find("/nowhere/libnative_nowhere.so: not found"), std::string::npos);
        EXPECT_EQ(std::string(error.what()).find("linked into the app"), std::string::npos);
    }
    EXPECT_THROW((void)NativeLibraries::open(""), std::invalid_argument);
}

TEST_F(NativeLibrariesTest, FindsTheSymbolsOfLinkedLibraries) {
    NativeLibraries::registerLinked("native_linked", {{"native_linked_twice", reinterpret_cast<void*>(&linkedTwice)}});
    const NativeLibraries::Library linked = NativeLibraries::open("native_linked");
    EXPECT_TRUE(linked.linked);
    EXPECT_EQ(linked.handle, nullptr);
    EXPECT_EQ(NativeLibraries::findSymbol(linked, "native_linked_twice"), reinterpret_cast<void*>(&linkedTwice));
    EXPECT_EQ(NativeLibraries::findSymbol(linked, "native_linked_other"), nullptr);
    EXPECT_EQ(NativeLibraries::findSymbol("native_linked_twice"), reinterpret_cast<void*>(&linkedTwice));

    NativeLibraries::registerLinked("native_unknown", {});
    EXPECT_EQ(NativeLibraries::findSymbol(NativeLibraries::open("native_unknown"), "native_linked_twice"), nullptr);
}

} // namespace haylen::platform
