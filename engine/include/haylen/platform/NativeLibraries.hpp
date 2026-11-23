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

    // A library the process can call. The path, in UTF-8, is what Varn's `ffi.load` opens to reach the same loaded library, and a linked library has neither a path nor a handle, because its symbols are part of the app.
    struct Library {
        std::string name;
        std::string path;
        void* handle = nullptr;
        bool linked = false;
    };

    // Whether the platform loads native libraries at run time, which a page and a port without the capability `dynamicLibraries` do not. Where it does not, only the libraries linked into the app load, by their names.
    [[nodiscard]] static bool isAvailable() noexcept;

    // Adds a folder that `open` searches before the folders of the platform, such as the one where the backend of the target `player` places the libraries of an app during development.
    static void addSearchFolder(std::filesystem::path folder);

    // Registers a library linked into the app with the symbols it exposes. Generated code calls it before `main`, and it replaces an earlier registration of the name.
    static void registerLinked(std::string library, std::initializer_list<Symbol> symbols);

    // Loads a library from a path, or from a name looked up in the search folders, the folders of the platform and the linked libraries in that order. Throws `std::runtime_error` that lists every place it looked.
    [[nodiscard]] static Library open(std::string_view name);

    // Returns the address of a symbol of the library, or null.
    [[nodiscard]] static void* findSymbol(const Library& library, std::string_view name);

    // Returns the address of a symbol of the linked libraries, of the libraries loaded so far or of the app itself, or null.
    [[nodiscard]] static void* findSymbol(std::string_view name);

    // A symbol of a library linked into the app, with the name of its library.
    struct LinkedSymbol {
        std::string library;
        std::string name;
        void* address = nullptr;
    };

    // Returns the symbols of the linked libraries, which a runtime of scripts finds by name without the app exporting them, such as `ffi.C` of Lua.
    [[nodiscard]] static std::vector<LinkedSymbol> getLinkedSymbols();

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

    // Finds a linked library on a platform that loads no library at run time.
    [[nodiscard]] static Library openLinked(std::string_view name);
};

} // namespace haylen::platform
