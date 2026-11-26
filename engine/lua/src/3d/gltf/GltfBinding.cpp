#include "3d/gltf/GltfBinding.hpp"

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "core/FloatBufferBinding.hpp"
#include "graphics/ImageBinding.hpp"
#include "haylen/3d/gltf/AccessorReader.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/FloatBuffer.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/io/Path.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/ScriptPlugin.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "varn/async/Promise.h"

namespace haylen::lua::gltf {

using haylen::gltf::Document;

int GltfBinding::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"read", &lua::Binding::native<&read>},
        {"readAsync", &lua::Binding::native<&readAsync>},
        {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void GltfBinding::push(lua_State* L, std::shared_ptr<Document> document) {
    lua::Userdata::emplace<Document>(L, std::move(document));
}

int GltfBinding::read(lua_State* L) {
    const std::string path = io::Path::normalize(lua::Stack::read<std::string_view>(L, 1));
    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    assets::Manager& assets = engine.getAssets();
    assets.requireLocal(path);
    haylen::gltf::Reader reader = haylen::gltf::Reader::open(engine.getPackage(), path);
    for (const std::string& file : reader.getExternalFiles()) {
        assets.requireLocal(file);
    }

    reader.loadResources();
    push(L, std::make_shared<Document>(reader.finish()));
    return 1;
}

int GltfBinding::readAsync(lua_State* L) {
    const std::string path = io::Path::normalize(lua::Stack::read<std::string_view>(L, 1));
    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    auto promise = std::make_shared<varn::async::Promise>(engine.getPlugin<lua::ScriptPlugin>().getRuntime());
    // clang-format off
    engine.getAssets().acquireAsync({path}, [&engine, promise, path](const std::string& error) {
        if (!error.empty()) {
            promise->reject(error);
            return;
        }

        openAsync(engine, promise, path);
    });
    // clang-format on
    varn::async::Promise::push(L, promise);
    return 1;
}

void GltfBinding::openAsync(haylen::core::Engine& engine, const std::shared_ptr<varn::async::Promise>& promise, const std::string& path) {
    // clang-format off
    engine.getJobs().runIo([&package = engine.getPackage(), path] {
        return std::make_shared<haylen::gltf::Reader>(haylen::gltf::Reader::open(package, path));
    }, [&engine, promise](haylen::core::JobSystem::Result<SharedReader> result) {
        if (!result.isOk()) {
            promise->reject(result.error);
            return;
        }

        loadAsync(engine, promise, *result.value);
    });
    // clang-format on
}

void GltfBinding::loadAsync(haylen::core::Engine& engine, const std::shared_ptr<varn::async::Promise>& promise, const SharedReader& reader) {
    // clang-format off
    const auto load = [&engine, promise, reader](const std::string& error) {
        if (!error.empty()) {
            promise->reject(error);
            return;
        }

        engine.getJobs().runIo([reader] {
            reader->loadResources();
            return reader;
        }, [&engine, promise](haylen::core::JobSystem::Result<SharedReader> result) {
            if (!result.isOk()) {
                promise->reject(result.error);
                return;
            }

            finishAsync(engine, promise, *result.value);
        });
    };
    // clang-format on
    engine.getAssets().acquireAsync(reader->getExternalFiles(), load);
}

void GltfBinding::finishAsync(haylen::core::Engine& engine, const std::shared_ptr<varn::async::Promise>& promise, const SharedReader& reader) {
    // clang-format off
    engine.getJobs().run([reader] {
        return std::make_shared<Document>(reader->finish());
    }, [promise](haylen::core::JobSystem::Result<std::shared_ptr<Document>> result) {
        if (!result.isOk()) {
            promise->reject(result.error);
            return;
        }

        const std::shared_ptr<Document> document = *result.value;
        promise->resolveCustom([document](lua_State* state) { push(state, document); });
    });
    // clang-format on
}

int GltfBinding::graphField(lua_State* L) {
    pushGraph(L, 1);
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);
    return 1;
}

void GltfBinding::pushGraph(lua_State* L, int index) {
    const int document = lua_absindex(L, index);
    lua::Userdata::pushField(L, document, "graph");
    if (!lua_isnil(L, -1)) {
        return;
    }

    lua_pop(L, 1);
    buildGraph(L, lua::Userdata::check<Document>(L, document));
    lua::Userdata::setField(L, document, "graph", lua_gettop(L));
}

std::size_t GltfBinding::checkObject(lua_State* L, int index, const char* list, std::size_t count) {
    const std::string refusal = "The argument " + std::to_string(index) + " is not a table of the list \"" + list + "\" of this document.";
    if (!lua_istable(L, index)) {
        throw std::invalid_argument(refusal);
    }

    lua_getfield(L, index, "index");
    const lua_Integer position = lua_isinteger(L, -1) != 0 ? lua_tointeger(L, -1) : -1;
    lua_pop(L, 1);
    if (position < 0 || static_cast<std::size_t>(position) >= count) {
        throw std::invalid_argument(refusal);
    }

    // The table must be the one of this document, so a table of another document never reads this one.
    pushGraph(L, 1);
    lua_getfield(L, -1, list);
    lua_rawgeti(L, -1, position + 1);
    const bool same = lua_rawequal(L, -1, index) != 0;
    lua_pop(L, 3);
    if (!same) {
        throw std::invalid_argument(refusal);
    }

    return static_cast<std::size_t>(position);
}

int GltfBinding::readAccessor(lua_State* L) {
    const Document& document = lua::Userdata::check<Document>(L, 1);
    const std::size_t accessor = checkObject(L, 2, "accessors", document.accessors.size());
    const std::vector<float> values = haylen::gltf::AccessorReader::readFloats(document, accessor);
    if (values.size() > haylen::core::FloatBuffer::kMaxSize) {
        throw std::invalid_argument("The accessor holds more values than a float buffer takes, so read it from C++.");
    }

    auto buffer = std::make_shared<haylen::core::FloatBuffer>(values.size());
    buffer->set(0, values);
    lua::Userdata::emplace<haylen::core::FloatBuffer>(L, std::move(buffer));
    return 1;
}

int GltfBinding::readUnsigned(lua_State* L) {
    const Document& document = lua::Userdata::check<Document>(L, 1);
    const std::size_t accessor = checkObject(L, 2, "accessors", document.accessors.size());
    lua::Stack::push(L, haylen::gltf::AccessorReader::readUnsigned(document, accessor));
    return 1;
}

int GltfBinding::decodeImage(lua_State* L) {
    const Document& document = lua::Userdata::check<Document>(L, 1);
    const std::size_t image = checkObject(L, 2, "images", document.images.size());
    lua::graphics::ImageBinding::push(L, document.decodeImage(image, lua::graphics::ImageBinding::readMaxSide(L, 3)));
    return 1;
}

int GltfBinding::decodeImageAsync(lua_State* L) {
    const std::shared_ptr<Document>& document = lua::Userdata::checkShared<Document>(L, 1);
    const std::size_t image = checkObject(L, 2, "images", document->images.size());
    const int maxSide = lua::graphics::ImageBinding::readMaxSide(L, 3);
    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    auto promise = std::make_shared<varn::async::Promise>(engine.getPlugin<lua::ScriptPlugin>().getRuntime());
    // clang-format off
    engine.getJobs().run([document, image, maxSide] {
        return document->decodeImage(image, maxSide);
    }, [promise](haylen::core::JobSystem::Result<haylen::graphics::Image> result) {
        if (!result.isOk()) {
            promise->reject(result.error);
            return;
        }

        auto decoded = std::make_shared<haylen::graphics::Image>(std::move(*result.value));
        promise->resolveCustom([decoded](lua_State* state) { lua::Userdata::emplace<haylen::graphics::Image>(state, decoded); });
    });
    // clang-format on
    varn::async::Promise::push(L, promise);
    return 1;
}

void GltfBinding::pushReference(lua_State* L, int graph, const char* list, std::optional<std::size_t> index) {
    if (!index) {
        lua_pushnil(L);
        return;
    }

    lua_getfield(L, graph, list);
    lua_rawgeti(L, -1, static_cast<lua_Integer>(*index + 1));
    lua_remove(L, -2);
}

void GltfBinding::setReference(lua_State* L, int graph, const char* key, const char* list, std::optional<std::size_t> index) {
    pushReference(L, graph, list, index);
    lua_setfield(L, -2, key);
}

void GltfBinding::setIndices(lua_State* L, int graph, const char* key, const char* list, const std::vector<std::size_t>& indices) {
    lua_createtable(L, static_cast<int>(indices.size()), 0);
    for (std::size_t index = 0; index < indices.size(); ++index) {
        pushReference(L, graph, list, indices[index]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    lua_setfield(L, -2, key);
}

void GltfBinding::setAttributes(lua_State* L, int graph, const char* key, const std::vector<haylen::gltf::Mesh::Attribute>& attributes) {
    lua_createtable(L, 0, static_cast<int>(attributes.size()));
    for (const haylen::gltf::Mesh::Attribute& attribute : attributes) {
        setReference(L, graph, attribute.name.c_str(), "accessors", attribute.accessor);
    }

    lua_setfield(L, -2, key);
}

void GltfBinding::setTextureInfo(lua_State* L, int graph, const char* key, const std::optional<haylen::gltf::Material::TextureInfo>& texture) {
    if (!texture) {
        return;
    }

    lua_createtable(L, 0, 4);
    setReference(L, graph, "texture", "textures", texture->index);
    set(L, "texCoord", texture->texCoord);
    setJson(L, "extras", texture->extras);
    setJson(L, "extensions", texture->extensions);
    lua_setfield(L, -2, key);
}

void GltfBinding::setJson(lua_State* L, const char* key, const haylen::core::Json& value) {
    if (value.is_null()) {
        return;
    }

    lua::JsonConverter::push(L, value);
    lua_setfield(L, -2, key);
}

void GltfBinding::beginObject(lua_State* L, int graph, const char* list, std::size_t index, const std::string& name) {
    pushReference(L, graph, list, index);
    if (!name.empty()) {
        set(L, "name", name);
    }
}

void GltfBinding::endObject(lua_State* L, const haylen::core::Json& extras, const haylen::core::Json& extensions) {
    setJson(L, "extras", extras);
    setJson(L, "extensions", extensions);
    lua_pop(L, 1);
}

void GltfBinding::fillAccessor(lua_State* L, int graph, const haylen::gltf::Accessor& accessor) {
    setReference(L, graph, "bufferView", "bufferViews", accessor.bufferView);
    set(L, "byteOffset", accessor.byteOffset);
    set(L, "componentType", accessor.componentType);
    set(L, "normalized", accessor.normalized);
    set(L, "count", accessor.count);
    set(L, "type", accessor.type);
    if (!accessor.min.empty()) {
        set(L, "min", accessor.min);
    }

    if (!accessor.max.empty()) {
        set(L, "max", accessor.max);
    }

    if (!accessor.sparse) {
        return;
    }

    const haylen::gltf::Accessor::Sparse& sparse = *accessor.sparse;
    lua_createtable(L, 0, 8);
    set(L, "count", sparse.count);
    setReference(L, graph, "indicesBufferView", "bufferViews", sparse.indicesBufferView);
    set(L, "indicesByteOffset", sparse.indicesByteOffset);
    set(L, "indicesComponentType", sparse.indicesComponentType);
    setReference(L, graph, "valuesBufferView", "bufferViews", sparse.valuesBufferView);
    set(L, "valuesByteOffset", sparse.valuesByteOffset);
    setJson(L, "extras", sparse.extras);
    setJson(L, "extensions", sparse.extensions);
    lua_setfield(L, -2, "sparse");
}

void GltfBinding::fillAnimation(lua_State* L, int graph, const Document& document, std::size_t index) {
    const haylen::gltf::Animation& animation = document.animations[index];
    set(L, "duration", document.getDuration(index));
    lua_createtable(L, static_cast<int>(animation.samplers.size()), 0);
    const int samplers = lua_gettop(L);
    for (std::size_t sampler = 0; sampler < animation.samplers.size(); ++sampler) {
        const haylen::gltf::Animation::Sampler& entry = animation.samplers[sampler];
        lua_createtable(L, 0, 6);
        set(L, "index", sampler);
        setReference(L, graph, "input", "accessors", entry.input);
        setReference(L, graph, "output", "accessors", entry.output);
        set(L, "interpolation", entry.interpolation);
        setJson(L, "extras", entry.extras);
        setJson(L, "extensions", entry.extensions);
        lua_rawseti(L, samplers, static_cast<lua_Integer>(sampler + 1));
    }

    lua_createtable(L, static_cast<int>(animation.channels.size()), 0);
    for (std::size_t channel = 0; channel < animation.channels.size(); ++channel) {
        const haylen::gltf::Animation::Channel& entry = animation.channels[channel];
        lua_createtable(L, 0, 7);
        lua_rawgeti(L, samplers, static_cast<lua_Integer>(entry.sampler + 1));
        lua_setfield(L, -2, "sampler");
        setReference(L, graph, "node", "nodes", entry.node);
        set(L, "path", entry.path);
        setJson(L, "extras", entry.extras);
        setJson(L, "extensions", entry.extensions);
        setJson(L, "targetExtras", entry.targetExtras);
        setJson(L, "targetExtensions", entry.targetExtensions);
        lua_rawseti(L, -2, static_cast<lua_Integer>(channel + 1));
    }

    lua_setfield(L, samplers - 1, "channels");
    lua_setfield(L, -2, "samplers");
}

void GltfBinding::fillBuffer(lua_State* L, const haylen::gltf::Buffer& buffer) {
    set(L, "storage", buffer.storage);
    if (!buffer.uri.empty()) {
        set(L, "uri", buffer.uri);
        set(L, "path", buffer.path);
    }

    set(L, "byteLength", buffer.byteLength);
}

void GltfBinding::fillBufferView(lua_State* L, int graph, const haylen::gltf::BufferView& view) {
    setReference(L, graph, "buffer", "buffers", view.buffer);
    set(L, "byteOffset", view.byteOffset);
    set(L, "byteLength", view.byteLength);
    set(L, "byteStride", view.byteStride);
    set(L, "target", view.target);
}

void GltfBinding::fillCamera(lua_State* L, const haylen::gltf::Camera& camera) {
    set(L, "type", camera.type);
    lua_createtable(L, 0, 4);
    if (camera.type == haylen::gltf::Camera::Type::Perspective) {
        set(L, "aspectRatio", camera.perspective.aspectRatio);
        set(L, "yfov", camera.perspective.yfov);
        set(L, "zfar", camera.perspective.zfar);
        set(L, "znear", camera.perspective.znear);
        lua_setfield(L, -2, "perspective");
        return;
    }

    set(L, "xmag", camera.orthographic.xmag);
    set(L, "ymag", camera.orthographic.ymag);
    set(L, "zfar", camera.orthographic.zfar);
    set(L, "znear", camera.orthographic.znear);
    lua_setfield(L, -2, "orthographic");
}

void GltfBinding::fillImage(lua_State* L, int graph, const haylen::gltf::Image& image) {
    set(L, "storage", image.storage);
    setReference(L, graph, "bufferView", "bufferViews", image.bufferView);
    if (!image.uri.empty()) {
        set(L, "uri", image.uri);
        set(L, "path", image.path);
    }

    if (!image.mimeType.empty()) {
        set(L, "mimeType", image.mimeType);
    }
}

void GltfBinding::fillMaterial(lua_State* L, int graph, const haylen::gltf::Material& material) {
    set(L, "baseColorFactor", material.baseColorFactor);
    setTextureInfo(L, graph, "baseColorTexture", material.baseColorTexture);
    set(L, "metallicFactor", material.metallicFactor);
    set(L, "roughnessFactor", material.roughnessFactor);
    setTextureInfo(L, graph, "metallicRoughnessTexture", material.metallicRoughnessTexture);
    setTextureInfo(L, graph, "normalTexture", material.normalTexture);
    set(L, "normalScale", material.normalScale);
    setTextureInfo(L, graph, "occlusionTexture", material.occlusionTexture);
    set(L, "occlusionStrength", material.occlusionStrength);
    setTextureInfo(L, graph, "emissiveTexture", material.emissiveTexture);
    set(L, "emissiveFactor", material.emissiveFactor);
    set(L, "alphaMode", material.alphaMode);
    set(L, "alphaCutoff", material.alphaCutoff);
    set(L, "doubleSided", material.doubleSided);
    setJson(L, "pbrExtras", material.pbrExtras);
    setJson(L, "pbrExtensions", material.pbrExtensions);
}

void GltfBinding::fillMesh(lua_State* L, int graph, const haylen::gltf::Mesh& mesh) {
    lua_createtable(L, static_cast<int>(mesh.primitives.size()), 0);
    for (std::size_t index = 0; index < mesh.primitives.size(); ++index) {
        const haylen::gltf::Mesh::Primitive& primitive = mesh.primitives[index];
        lua_createtable(L, 0, 7);
        setAttributes(L, graph, "attributes", primitive.attributes);
        setReference(L, graph, "indices", "accessors", primitive.indices);
        setReference(L, graph, "material", "materials", primitive.material);
        set(L, "mode", primitive.mode);
        lua_createtable(L, static_cast<int>(primitive.targets.size()), 0);
        for (std::size_t target = 0; target < primitive.targets.size(); ++target) {
            lua_createtable(L, 0, 1);
            setAttributes(L, graph, "attributes", primitive.targets[target]);
            lua_getfield(L, -1, "attributes");
            lua_remove(L, -2);
            lua_rawseti(L, -2, static_cast<lua_Integer>(target + 1));
        }

        lua_setfield(L, -2, "targets");
        setJson(L, "extras", primitive.extras);
        setJson(L, "extensions", primitive.extensions);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    lua_setfield(L, -2, "primitives");
    set(L, "weights", mesh.weights);
}

void GltfBinding::fillNode(lua_State* L, int graph, const haylen::gltf::Node& node) {
    setIndices(L, graph, "children", "nodes", node.children);
    setReference(L, graph, "parent", "nodes", node.parent);
    setReference(L, graph, "camera", "cameras", node.camera);
    setReference(L, graph, "mesh", "meshes", node.mesh);
    setReference(L, graph, "skin", "skins", node.skin);
    set(L, "matrix", node.matrix);
    set(L, "translation", node.translation);
    set(L, "rotation", node.rotation);
    set(L, "scale", node.scale);
    set(L, "localMatrix", node.getLocalMatrix());
    set(L, "weights", node.weights);
}

void GltfBinding::fillSampler(lua_State* L, const haylen::gltf::Sampler& sampler) {
    set(L, "magFilter", sampler.magFilter);
    set(L, "minFilter", sampler.minFilter);
    set(L, "wrapS", sampler.wrapS);
    set(L, "wrapT", sampler.wrapT);
}

void GltfBinding::fillSkin(lua_State* L, int graph, const haylen::gltf::Skin& skin) {
    setReference(L, graph, "inverseBindMatrices", "accessors", skin.inverseBindMatrices);
    setReference(L, graph, "skeleton", "nodes", skin.skeleton);
    setIndices(L, graph, "joints", "nodes", skin.joints);
}

void GltfBinding::fillTexture(lua_State* L, int graph, const haylen::gltf::Texture& texture) {
    setReference(L, graph, "sampler", "samplers", texture.sampler);
    setReference(L, graph, "source", "images", texture.source);
}

void GltfBinding::fillDocument(lua_State* L, int graph, const Document& document) {
    set(L, "file", document.file);
    lua_createtable(L, 0, 5);
    set(L, "version", document.asset.version);
    if (!document.asset.minVersion.empty()) {
        set(L, "minVersion", document.asset.minVersion);
    }

    if (!document.asset.generator.empty()) {
        set(L, "generator", document.asset.generator);
    }

    if (!document.asset.copyright.empty()) {
        set(L, "copyright", document.asset.copyright);
    }

    setJson(L, "extras", document.asset.extras);
    lua_setfield(L, -2, "asset");
    setReference(L, graph, "scene", "scenes", document.scene);
    set(L, "extensionsUsed", document.extensionsUsed);
    set(L, "extensionsRequired", document.extensionsRequired);
    setJson(L, "extras", document.extras);
    setJson(L, "extensions", document.extensions);
}

void GltfBinding::buildGraph(lua_State* L, const Document& document) {
    lua_createtable(L, 0, static_cast<int>(kLists.size() + kFields.size()));
    const int graph = lua_gettop(L);
    const std::array<std::size_t, kLists.size()> sizes{document.accessors.size(), document.animations.size(), document.buffers.size(), document.bufferViews.size(), document.cameras.size(), document.images.size(), document.materials.size(), document.meshes.size(), document.nodes.size(), document.samplers.size(), document.scenes.size(), document.skins.size(), document.textures.size()};

    // Every object gets its table first, so the objects can name each other in any order.
    for (std::size_t list = 0; list < kLists.size(); ++list) {
        lua_createtable(L, static_cast<int>(sizes[list]), 0);
        for (std::size_t index = 0; index < sizes[list]; ++index) {
            lua_createtable(L, 0, 8);
            set(L, "index", index);
            lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
        }

        lua_setfield(L, graph, kLists[list]);
    }

    for (std::size_t index = 0; index < document.accessors.size(); ++index) {
        const haylen::gltf::Accessor& accessor = document.accessors[index];
        beginObject(L, graph, "accessors", index, accessor.name);
        fillAccessor(L, graph, accessor);
        endObject(L, accessor.extras, accessor.extensions);
    }

    for (std::size_t index = 0; index < document.animations.size(); ++index) {
        const haylen::gltf::Animation& animation = document.animations[index];
        beginObject(L, graph, "animations", index, animation.name);
        fillAnimation(L, graph, document, index);
        endObject(L, animation.extras, animation.extensions);
    }

    for (std::size_t index = 0; index < document.buffers.size(); ++index) {
        const haylen::gltf::Buffer& buffer = document.buffers[index];
        beginObject(L, graph, "buffers", index, buffer.name);
        fillBuffer(L, buffer);
        endObject(L, buffer.extras, buffer.extensions);
    }

    for (std::size_t index = 0; index < document.bufferViews.size(); ++index) {
        const haylen::gltf::BufferView& view = document.bufferViews[index];
        beginObject(L, graph, "bufferViews", index, view.name);
        fillBufferView(L, graph, view);
        endObject(L, view.extras, view.extensions);
    }

    for (std::size_t index = 0; index < document.cameras.size(); ++index) {
        const haylen::gltf::Camera& camera = document.cameras[index];
        beginObject(L, graph, "cameras", index, camera.name);
        fillCamera(L, camera);
        endObject(L, camera.extras, camera.extensions);
    }

    for (std::size_t index = 0; index < document.images.size(); ++index) {
        const haylen::gltf::Image& image = document.images[index];
        beginObject(L, graph, "images", index, image.name);
        fillImage(L, graph, image);
        endObject(L, image.extras, image.extensions);
    }

    for (std::size_t index = 0; index < document.materials.size(); ++index) {
        const haylen::gltf::Material& material = document.materials[index];
        beginObject(L, graph, "materials", index, material.name);
        fillMaterial(L, graph, material);
        endObject(L, material.extras, material.extensions);
    }

    for (std::size_t index = 0; index < document.meshes.size(); ++index) {
        const haylen::gltf::Mesh& mesh = document.meshes[index];
        beginObject(L, graph, "meshes", index, mesh.name);
        fillMesh(L, graph, mesh);
        endObject(L, mesh.extras, mesh.extensions);
    }

    for (std::size_t index = 0; index < document.nodes.size(); ++index) {
        const haylen::gltf::Node& node = document.nodes[index];
        beginObject(L, graph, "nodes", index, node.name);
        fillNode(L, graph, node);
        endObject(L, node.extras, node.extensions);
    }

    for (std::size_t index = 0; index < document.samplers.size(); ++index) {
        const haylen::gltf::Sampler& sampler = document.samplers[index];
        beginObject(L, graph, "samplers", index, sampler.name);
        fillSampler(L, sampler);
        endObject(L, sampler.extras, sampler.extensions);
    }

    for (std::size_t index = 0; index < document.scenes.size(); ++index) {
        const haylen::gltf::Scene& scene = document.scenes[index];
        beginObject(L, graph, "scenes", index, scene.name);
        setIndices(L, graph, "nodes", "nodes", scene.nodes);
        endObject(L, scene.extras, scene.extensions);
    }

    for (std::size_t index = 0; index < document.skins.size(); ++index) {
        const haylen::gltf::Skin& skin = document.skins[index];
        beginObject(L, graph, "skins", index, skin.name);
        fillSkin(L, graph, skin);
        endObject(L, skin.extras, skin.extensions);
    }

    for (std::size_t index = 0; index < document.textures.size(); ++index) {
        const haylen::gltf::Texture& texture = document.textures[index];
        beginObject(L, graph, "textures", index, texture.name);
        fillTexture(L, graph, texture);
        endObject(L, texture.extras, texture.extensions);
    }

    fillDocument(L, graph, document);
}

void GltfBinding::install(lua_State* L) {
    lua::ClassBuilder<Document> builder(L);
    builder.function("readAccessor", &lua::Binding::native<&readAccessor>);
    builder.function("readUnsigned", &lua::Binding::native<&readUnsigned>);
    builder.function("decodeImage", &lua::Binding::native<&decodeImage>);
    builder.function("decodeImageAsync", &lua::Binding::native<&decodeImageAsync>);
    for (const char* field : kFields) {
        builder.property(field, &lua::Binding::native<&graphField>);
    }

    for (const char* list : kLists) {
        builder.property(list, &lua::Binding::native<&graphField>);
    }

    builder.install();
    lua::Binding::preload(L, "haylen.gltf", &open);
}

} // namespace haylen::lua::gltf
