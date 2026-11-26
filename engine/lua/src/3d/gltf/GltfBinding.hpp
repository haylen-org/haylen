#pragma once

#include <lua.hpp>

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/3d/gltf/Document.hpp"
#include "haylen/3d/gltf/Reader.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Type.hpp"

namespace varn::async {
class Promise;
}

namespace haylen::core {
class Engine;
}

namespace haylen::lua {

template <> struct Type<haylen::gltf::Document> {
    static constexpr const char* name = "haylen.Document";
    using Storage = std::shared_ptr<haylen::gltf::Document>;
};

} // namespace haylen::lua

namespace haylen::lua::gltf {

// Installs `haylen.gltf`, which reads glTF 2.0 files of the package into a read-only `Document` without making anything to draw, for tools and for apps that read what a model holds. The objects of a document cross into Lua as one graph of tables, whose references between objects are the tables themselves, such as `node.mesh.primitives[1].material`, and each table names its place in the file in `index`, counted from zero as the file and the messages count it.
class GltfBinding final {
  public:
    static void install(lua_State* L);

    // Reads the index of the object of the document at index 1 that a table of its graph stands for, such as an accessor, and throws for any other value, which other modules that take objects of a document use too.
    [[nodiscard]] static std::size_t checkObject(lua_State* L, int index, const char* list, std::size_t count);

  private:
    // The lists of a document, in the order the graph builds them.
    static constexpr std::array<const char*, 13> kLists{"accessors", "animations", "buffers", "bufferViews", "cameras", "images", "materials", "meshes", "nodes", "samplers", "scenes", "skins", "textures"};
    static constexpr std::array<const char*, 7> kFields{"file", "asset", "scene", "extensionsUsed", "extensionsRequired", "extras", "extensions"};

    using SharedReader = std::shared_ptr<haylen::gltf::Reader>;

    static int open(lua_State* L);

    // Reads a document with `gltf.read(path)`, which fails for a file that is not on the device yet, and with `gltf.readAsync(path)`, which brings the file and the files beside it onto the device first and reads on the I/O pool and the task pool.
    static int read(lua_State* L);
    static int readAsync(lua_State* L);

    // The steps of `readAsync` after the file is on the device: opening it on the I/O pool, bringing the files beside it and loading them on the I/O pool, and checking the data on the task pool.
    static void openAsync(haylen::core::Engine& engine, const std::shared_ptr<varn::async::Promise>& promise, const std::string& path);
    static void loadAsync(haylen::core::Engine& engine, const std::shared_ptr<varn::async::Promise>& promise, const SharedReader& reader);
    static void finishAsync(haylen::core::Engine& engine, const std::shared_ptr<varn::async::Promise>& promise, const SharedReader& reader);

    static int graphField(lua_State* L);
    static int readAccessor(lua_State* L);
    static int readUnsigned(lua_State* L);
    static int decodeImage(lua_State* L);
    static int decodeImageAsync(lua_State* L);

    static void push(lua_State* L, std::shared_ptr<haylen::gltf::Document> document);

    // Pushes the graph of tables of the document at the index, building it the first time and keeping it in the user value of the document.
    static void pushGraph(lua_State* L, int index);
    static void buildGraph(lua_State* L, const haylen::gltf::Document& document);

    // Pushes the table of an object of a list of the graph, its name and its index, or nil for no object.
    static void pushReference(lua_State* L, int graph, const char* list, std::optional<std::size_t> index);
    static void setReference(lua_State* L, int graph, const char* key, const char* list, std::optional<std::size_t> index);
    static void setIndices(lua_State* L, int graph, const char* key, const char* list, const std::vector<std::size_t>& indices);
    static void setAttributes(lua_State* L, int graph, const char* key, const std::vector<haylen::gltf::Mesh::Attribute>& attributes);
    static void setTextureInfo(lua_State* L, int graph, const char* key, const std::optional<haylen::gltf::Material::TextureInfo>& texture);
    static void setJson(lua_State* L, const char* key, const haylen::core::Json& value);

    // Sets a field of the table on top of the stack, which nothing sets for an empty optional.
    template <typename T> static void set(lua_State* L, const char* key, const T& value) {
        lua::Stack::push(L, value);
        lua_setfield(L, -2, key);
    }

    // Pushes the table of an object for its fields, with its name, and pops it with its extras and extensions.
    static void beginObject(lua_State* L, int graph, const char* list, std::size_t index, const std::string& name);
    static void endObject(lua_State* L, const haylen::core::Json& extras, const haylen::core::Json& extensions);

    static void fillAccessor(lua_State* L, int graph, const haylen::gltf::Accessor& accessor);
    static void fillAnimation(lua_State* L, int graph, const haylen::gltf::Document& document, std::size_t index);
    static void fillBuffer(lua_State* L, const haylen::gltf::Buffer& buffer);
    static void fillBufferView(lua_State* L, int graph, const haylen::gltf::BufferView& view);
    static void fillCamera(lua_State* L, const haylen::gltf::Camera& camera);
    static void fillImage(lua_State* L, int graph, const haylen::gltf::Image& image);
    static void fillMaterial(lua_State* L, int graph, const haylen::gltf::Material& material);
    static void fillMesh(lua_State* L, int graph, const haylen::gltf::Mesh& mesh);
    static void fillNode(lua_State* L, int graph, const haylen::gltf::Node& node);
    static void fillSampler(lua_State* L, const haylen::gltf::Sampler& sampler);
    static void fillSkin(lua_State* L, int graph, const haylen::gltf::Skin& skin);
    static void fillTexture(lua_State* L, int graph, const haylen::gltf::Texture& texture);
    static void fillDocument(lua_State* L, int graph, const haylen::gltf::Document& document);
};

} // namespace haylen::lua::gltf
