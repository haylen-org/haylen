#include "haylen/platform/NativeLibraries.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "platform/native/LibraryLoader.hpp"
#include "varn/runtime/Runtime.h"

namespace haylen::platform {

bool NativeLibraries::isAvailable() noexcept {
    return LibraryLoader::isAvailable();
}

NativeLibraries::State& NativeLibraries::getState() {
    static State& state = *new State();
    return state;
}

void NativeLibraries::addSearchFolder(std::filesystem::path folder) {
    State& state = getState();
    const std::scoped_lock lock(state.mutex);
    if (std::ranges::find(state.folders, folder) == state.folders.end()) {
        state.folders.push_back(std::move(folder));
    }
}

void NativeLibraries::registerLinked(std::string library, std::initializer_list<Symbol> symbols) {
    std::unordered_map<std::string, void*> table;
    for (const Symbol& symbol : symbols) {
        table.insert_or_assign(symbol.name, symbol.address);
    }

    State& state = getState();
    const std::scoped_lock lock(state.mutex);
    state.linked.insert_or_assign(std::move(library), std::move(table));
}

std::vector<std::string> NativeLibraries::getFileNames(std::string_view name) {
    const std::string text(name);
    if (std::filesystem::path(text).has_extension()) {
        return {text};
    }

    return LibraryLoader::getFileNames(name);
}

NativeLibraries::Library NativeLibraries::open(std::string_view name) {
    if (name.empty()) {
        throw std::invalid_argument("A native library needs a name or a path.");
    }

    if (!LibraryLoader::isAvailable()) {
        return openLinked(name);
    }

    State& state = getState();
    const std::scoped_lock lock(state.mutex);
    const std::filesystem::path given(name);
    std::vector<std::filesystem::path> candidates;
    if (given.has_parent_path()) {
        candidates.push_back(given);
    } else {
        std::vector<std::filesystem::path> folders = state.folders;
        std::ranges::copy(LibraryLoader::getFolders(), std::back_inserter(folders));

        for (const std::filesystem::path& folder : folders) {
            for (const std::string& file : getFileNames(name)) {
                candidates.push_back(folder / file);
            }
        }
    }

    std::string searched;
    for (const std::filesystem::path& candidate : candidates) {
        const std::u8string text = candidate.u8string();
        const std::string path(text.begin(), text.end());
        if (const auto found = std::ranges::find(state.loaded, path, &Library::path); found != state.loaded.end()) {
            return *found;
        }

        std::string failure;
        if (void* handle = LibraryLoader::load(candidate, failure)) {
            Library library{.name = std::string(name), .path = path, .handle = handle};
            state.loaded.push_back(library);
            return library;
        }

        searched += "\n  " + path + ": " + failure;
    }

    if (!given.has_parent_path() && state.linked.contains(std::string(name))) {
        return {.name = std::string(name), .linked = true};
    }

    if (!given.has_parent_path()) {
        searched += "\n  The libraries linked into the app: not registered";
    }

    throw std::runtime_error("The native library \"" + std::string(name) + "\" could not be loaded. These are the places it searched:" + searched + "\nPut the library in one of them or load it by its path.");
}

NativeLibraries::Library NativeLibraries::openLinked(std::string_view name) {
    State& state = getState();
    const std::scoped_lock lock(state.mutex);
    if (state.linked.contains(std::string(name))) {
        return {.name = std::string(name), .linked = true};
    }

    throw std::runtime_error("This platform loads no native library at run time, so the library \"" + std::string(name) + "\" loads only when the app links it, which it does not. Link the library into the app with \"link\": \"static\" and the symbols that Lua calls.");
}

void* NativeLibraries::findSymbol(const Library& library, std::string_view name) {
    if (!library.linked) {
        return LibraryLoader::lookup(library.handle, std::string(name));
    }

    State& state = getState();
    const std::scoped_lock lock(state.mutex);
    const auto table = state.linked.find(library.name);
    if (table == state.linked.end()) {
        return nullptr;
    }

    const auto symbol = table->second.find(std::string(name));
    return symbol != table->second.end() ? symbol->second : nullptr;
}

void* NativeLibraries::findSymbol(std::string_view name) {
    const std::string text(name);
    State& state = getState();
    const std::scoped_lock lock(state.mutex);
    for (const auto& [library, table] : state.linked) {
        if (const auto symbol = table.find(text); symbol != table.end()) {
            return symbol->second;
        }
    }

    for (const Library& library : state.loaded) {
        if (void* address = LibraryLoader::lookup(library.handle, text)) {
            return address;
        }
    }

    return LibraryLoader::lookupProcess(text);
}

void NativeLibraries::addLinkedSymbols(varn::runtime::Runtime& runtime) {
    State& state = getState();
    const std::scoped_lock lock(state.mutex);
    for (const auto& [library, table] : state.linked) {
        for (const auto& [symbol, address] : table) {
            if (!runtime.addSymbol(symbol, address)) {
                throw std::runtime_error("The symbol \"" + symbol + "\" of the linked library \"" + library + "\" has another address in another linked library. Register each symbol with one address.");
            }
        }
    }
}

} // namespace haylen::platform
