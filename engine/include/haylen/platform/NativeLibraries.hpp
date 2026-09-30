#pragma once

#include <filesystem>
#include <initializer_list>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace haylen::platform {

// Finds and loads the native libraries of an app and keeps them loaded for the rest of the process, because native code may still run from them after the app stops. A library that the app links statically, as iOS apps do with static libraries, registers its symbols here instead of being loaded.
class NativeLibraries final {
  public:
    struct Symbol {
        const char* name = nullptr;
        void* address = nullptr;
    };

    // A library the process can call. The path is what Varn's `ffi.load` opens to reach the same loaded library, and a linked library has neither a path nor a handle, because its symbols are part of the app.
    struct Library {
        std::string name;
        std::string path;
        void* handle = nullptr;
        bool linked = false;
    };

    // Whether the platform loads native libraries, which the browser does not.
    [[nodiscard]] static bool isAvailable() noexcept;

    // Adds a folder that `open` searches before the folders of the platform, such as the one `make.py` builds the libraries of an app into during development.
    static void addSearchFolder(std::filesystem::path folder);

    // Registers a library linked into the app with the symbols it exposes. Generated code calls it before `main`, and it replaces an earlier registration of the name.
    static void registerLinked(std::string library, std::initializer_list<Symbol> symbols);

    // Loads a library from a path, or from a name looked up in the search folders, the folders of the platform and the linked libraries in that order. Throws `std::runtime_error` that lists every place it looked.
    [[nodiscard]] static Library open(std::string_view name);

    // Returns the address of a symbol of the library, or null.
    [[nodiscard]] static void* findSymbol(const Library& library, std::string_view name);

    // Returns the address of a symbol of the linked libraries, of the libraries loaded so far or of the app itself, or null.
    [[nodiscard]] static void* findSymbol(std::string_view name);

    // Returns the file names a library of this name has on the platform, such as `libname.dylib` and `name.framework/name` on Apple platforms. A name with an extension is already a file name.
    [[nodiscard]] static std::vector<std::string> getFileNames(std::string_view name);

  private:
    struct State {
        std::mutex mutex;
        std::vector<std::filesystem::path> folders;
        std::unordered_map<std::string, std::unordered_map<std::string, void*>> linked;
        std::vector<Library> loaded;
    };

    // Generated code registers linked libraries before `main`, so the state exists from its first use.
    [[nodiscard]] static State& getState();

    [[nodiscard]] static std::vector<std::filesystem::path> getPlatformFolders();
    [[nodiscard]] static std::filesystem::path getExecutableFolder();

    // Opens a file of a candidate, or returns null and describes why it failed.
    [[nodiscard]] static void* load(const std::filesystem::path& file, std::string& failure);
    [[nodiscard]] static void* lookup(void* handle, const std::string& name);
    [[nodiscard]] static void* lookupProcess(const std::string& name);

    // Returns what Varn's `ffi.load` needs to reach a library this class loaded from the file.
    [[nodiscard]] static std::string getLoadPath(const std::filesystem::path& file);
};

} // namespace haylen::platform
