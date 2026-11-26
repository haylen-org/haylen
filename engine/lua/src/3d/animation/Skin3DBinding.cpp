#include "3d/animation/Skin3DBinding.hpp"

#include <lua.hpp>

#include <algorithm>
#include <format>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "3d/animation/Skeleton3DBinding.hpp"
#include "3d/gltf/GltfBinding.hpp"
#include "core/FloatBufferBinding.hpp"
#include "haylen/3d/animation/Skinning3D.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua::animation3d {

using haylen::animation3d::Skin3D;
using haylen::animation3d::SkinWeights3D;

std::shared_ptr<const Skin3D> Skin3DBinding::checkSkin(lua_State* L, int index) {
    return lua::Userdata::checkShared<Skin3D>(L, index);
}

int Skin3DBinding::create(lua_State* L) {
    std::shared_ptr<const haylen::animation3d::Skeleton3D> skeleton = Skeleton3DBinding::checkSkeleton(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    std::vector<Skin3D::Joint> joints;
    const lua_Integer count = luaL_len(L, 2);
    for (lua_Integer place = 1; place <= count; ++place) {
        lua_rawgeti(L, 2, place);
        lua::Table::requireItemType(L, "joints", place, LUA_TTABLE);
        const int entry = lua_gettop(L);
        lua::Table::checkFields(L, entry, {kJointFields});
        lua_Integer bone = 0;
        lua::Table::readField(L, entry, "bone", bone);
        if (bone < 1) {
            throw std::invalid_argument(std::format("Joint {} of a skin names its bone by its place counted from 1, while it was given {}.", place, bone));
        }

        Skin3D::Joint joint{.bone = static_cast<std::size_t>(bone - 1)};
        lua::Table::readField(L, entry, "inverseBindMatrix", joint.inverseBindMatrix);
        lua_pop(L, 1);
        joints.push_back(joint);
    }

    lua::Userdata::emplace<Skin3D>(L, std::make_shared<Skin3D>(std::move(skeleton), std::move(joints)));
    return 1;
}

int Skin3DBinding::fromGltf(lua_State* L) {
    const haylen::gltf::Document& document = lua::Userdata::check<haylen::gltf::Document>(L, 1);
    const std::size_t skin = lua::gltf::GltfBinding::checkObject(L, 2, "skins", document.skins.size());
    lua::Userdata::emplace<Skin3D>(L, std::make_shared<Skin3D>(Skin3D::fromGltf(document, skin, Skeleton3DBinding::checkSkeleton(L, 3))));
    return 1;
}

int Skin3DBinding::weightsFromGltf(lua_State* L) {
    const haylen::gltf::Document& document = lua::Userdata::check<haylen::gltf::Document>(L, 1);
    const std::size_t mesh = lua::gltf::GltfBinding::checkObject(L, 2, "meshes", document.meshes.size());
    const std::shared_ptr<const Skin3D> skin = checkSkin(L, 3);
    lua_Integer primitive = 1;
    lua_Integer influences = 4;
    if (!lua_isnoneornil(L, 4)) {
        luaL_checktype(L, 4, LUA_TTABLE);
        lua::Table::checkFields(L, 4, {kWeightsFields});
        lua::Table::readField(L, 4, "primitive", primitive);
        lua::Table::readField(L, 4, "influences", influences);
    }

    const std::size_t primitives = document.meshes[mesh].primitives.size();
    if (primitive < 1 || static_cast<std::size_t>(primitive) > primitives) {
        throw std::invalid_argument(std::format("The mesh has {} primitives, counted from 1, so it has no primitive {}.", primitives, primitive));
    }

    if (influences < 1) {
        throw std::invalid_argument(std::format("A vertex takes 4 or 8 influences, while {} were asked for.", influences));
    }

    SkinWeights3D weights = SkinWeights3D::fromGltf(document, mesh, static_cast<std::size_t>(primitive - 1), skin->getJointCount(), static_cast<std::size_t>(influences));
    lua::Userdata::emplace<SkinWeights3D>(L, std::make_shared<SkinWeights3D>(std::move(weights)));
    return 1;
}

int Skin3DBinding::jointCount(lua_State* L) {
    lua::Stack::push(L, checkSkin(L, 1)->getJointCount());
    return 1;
}

// Lists the bone of each joint, counted from 1.
int Skin3DBinding::joints(lua_State* L) {
    const std::span<const Skin3D::Joint> list = checkSkin(L, 1)->getJoints();
    lua_createtable(L, static_cast<int>(list.size()), 0);
    for (std::size_t index = 0; index < list.size(); ++index) {
        lua_createtable(L, 0, 2);
        lua::Stack::push(L, list[index].bone + 1);
        lua_setfield(L, -2, "bone");
        lua::Stack::push(L, list[index].inverseBindMatrix);
        lua_setfield(L, -2, "inverseBindMatrix");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    return 1;
}

int Skin3DBinding::skinSkeleton(lua_State* L) {
    lua::Userdata::emplace<haylen::animation3d::Skeleton3D>(L, std::const_pointer_cast<haylen::animation3d::Skeleton3D>(checkSkin(L, 1)->getSkeletonPointer()));
    return 1;
}

// Writes the palette of a pose with `palette(pose, buffer[, first])`, twelve floats for each joint: the first three rows of its matrix.
int Skin3DBinding::palette(lua_State* L) {
    const std::shared_ptr<const Skin3D> skin = checkSkin(L, 1);
    const ScriptedPose3D& pose = Skeleton3DBinding::checkPose(L, 2);
    const std::span<float> target = lua::core::FloatBufferBinding::readRange(L, 3);
    const std::size_t floats = skin->getJointCount() * Skin3D::kRowFloats;
    if (target.size() < floats) {
        throw std::invalid_argument(std::format("The palette of {} joints takes {} values, while the buffer holds {} from its first one.", skin->getJointCount(), floats, target.size()));
    }

    matrices.resize(skin->getJointCount());
    skin->computePalette(pose.pose, matrices);
    Skin3D::writeRows(matrices, target.first(floats));
    return 0;
}

// Writes the palette of a pose as dual quaternions with `dualQuatPalette(pose, buffer[, first])`, eight floats for each joint: the real part and then the dual part, each `x, y, z, w`.
int Skin3DBinding::dualQuatPalette(lua_State* L) {
    const std::shared_ptr<const Skin3D> skin = checkSkin(L, 1);
    const ScriptedPose3D& pose = Skeleton3DBinding::checkPose(L, 2);
    const std::span<float> target = lua::core::FloatBufferBinding::readRange(L, 3);
    const std::size_t floats = skin->getJointCount() * kDualQuatFloats;
    if (target.size() < floats) {
        throw std::invalid_argument(std::format("The dual quaternion palette of {} joints takes {} values, while the buffer holds {} from its first one.", skin->getJointCount(), floats, target.size()));
    }

    dualQuats.resize(skin->getJointCount());
    skin->computeDualQuatPalette(pose.pose, dualQuats);
    for (std::size_t index = 0; index < dualQuats.size(); ++index) {
        const haylen::math::DualQuat& joint = dualQuats[index];
        float* values = target.data() + index * kDualQuatFloats;
        values[0] = joint.real.x;
        values[1] = joint.real.y;
        values[2] = joint.real.z;
        values[3] = joint.real.w;
        values[4] = joint.dual.x;
        values[5] = joint.dual.y;
        values[6] = joint.dual.z;
        values[7] = joint.dual.w;
    }

    return 0;
}

int Skin3DBinding::influenceCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<SkinWeights3D>(L, 1).getInfluenceCount());
    return 1;
}

int Skin3DBinding::vertexCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<SkinWeights3D>(L, 1).getVertexCount());
    return 1;
}

int Skin3DBinding::weightsJointCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<SkinWeights3D>(L, 1).getJointCount());
    return 1;
}

// Writes the joints of every vertex with `readJoints(buffer[, first])`, counted from zero as the joints of a skin are in a shader.
int Skin3DBinding::readJoints(lua_State* L) {
    const SkinWeights3D& weights = lua::Userdata::check<SkinWeights3D>(L, 1);
    const std::span<float> target = lua::core::FloatBufferBinding::readRange(L, 2);
    const std::span<const std::uint16_t> joints = weights.getJoints();
    if (target.size() < joints.size()) {
        throw std::invalid_argument(std::format("The joints of the skin weights take {} values, while the buffer holds {} from its first one.", joints.size(), target.size()));
    }

    for (std::size_t index = 0; index < joints.size(); ++index) {
        target[index] = static_cast<float>(joints[index]);
    }

    return 0;
}

int Skin3DBinding::readWeights(lua_State* L) {
    const SkinWeights3D& weights = lua::Userdata::check<SkinWeights3D>(L, 1);
    const std::span<float> target = lua::core::FloatBufferBinding::readRange(L, 2);
    const std::span<const float> values = weights.getWeights();
    if (target.size() < values.size()) {
        throw std::invalid_argument(std::format("The weights of the skin weights take {} values, while the buffer holds {} from its first one.", values.size(), target.size()));
    }

    std::ranges::copy(values, target.begin());
    return 0;
}

void Skin3DBinding::readVectors(std::span<const float> numbers, std::size_t count, std::vector<haylen::math::Vec3>& target, std::string_view what) {
    if (numbers.size() < count * 3) {
        throw std::invalid_argument(std::format("The {} of {} vertices take {} values, while the buffer holds {}.", what, count, count * 3, numbers.size()));
    }

    target.resize(count);
    for (std::size_t vertex = 0; vertex < count; ++vertex) {
        target[vertex] = {numbers[vertex * 3], numbers[vertex * 3 + 1], numbers[vertex * 3 + 2]};
    }
}

void Skin3DBinding::writeVectors(const std::vector<haylen::math::Vec3>& source, std::span<float> numbers) {
    for (std::size_t vertex = 0; vertex < source.size(); ++vertex) {
        numbers[vertex * 3] = source[vertex].x;
        numbers[vertex * 3 + 1] = source[vertex].y;
        numbers[vertex * 3 + 2] = source[vertex].z;
    }
}

// Bends positions on the CPU with `skin(pose, skin, positions, skinnedPositions[, {normals, skinnedNormals, dualQuaternions}])`, three floats for each vertex in float buffers, with matrices or with dual quaternions, on the job system for a large mesh.
int Skin3DBinding::skin(lua_State* L) {
    const SkinWeights3D& weights = lua::Userdata::check<SkinWeights3D>(L, 1);
    const ScriptedPose3D& pose = Skeleton3DBinding::checkPose(L, 2);
    const std::shared_ptr<const Skin3D> jointSkin = checkSkin(L, 3);
    const std::size_t count = weights.getVertexCount();
    readVectors(lua::Userdata::check<haylen::core::FloatBuffer>(L, 4).getValues(), count, positions, "positions");
    std::span<float> skinnedTarget = lua::Userdata::check<haylen::core::FloatBuffer>(L, 5).getValues();
    if (skinnedTarget.size() < count * 3) {
        throw std::invalid_argument(std::format("The skinned positions of {} vertices take {} values, while the buffer holds {}.", count, count * 3, skinnedTarget.size()));
    }

    std::span<float> normalTarget;
    bool dualQuaternions = false;
    normals.clear();
    if (!lua_isnoneornil(L, 6)) {
        luaL_checktype(L, 6, LUA_TTABLE);
        lua::Table::checkFields(L, 6, {kSkinFields});
        lua::Table::readField(L, 6, "dualQuaternions", dualQuaternions);
        lua_getfield(L, 6, "normals");
        if (!lua_isnil(L, -1)) {
            readVectors(lua::Userdata::check<haylen::core::FloatBuffer>(L, -1).getValues(), count, normals, "normals");
            lua_getfield(L, 6, "skinnedNormals");
            normalTarget = lua::Userdata::check<haylen::core::FloatBuffer>(L, -1).getValues();
            lua_pop(L, 1);
            if (normalTarget.size() < count * 3) {
                throw std::invalid_argument(std::format("The skinned normals of {} vertices take {} values, while the buffer holds {}.", count, count * 3, normalTarget.size()));
            }
        }

        lua_pop(L, 1);
    }

    skinnedPositions.resize(count);
    skinnedNormals.resize(normals.size());
    const haylen::animation3d::Skinning3D::Vertices vertices{.positions = positions, .normals = normals, .skinnedPositions = skinnedPositions, .skinnedNormals = skinnedNormals};
    haylen::core::JobSystem& jobs = lua::Runtime::getEngine(L).getJobs();
    if (dualQuaternions) {
        dualQuats.resize(jointSkin->getJointCount());
        jointSkin->computeDualQuatPalette(pose.pose, dualQuats);
        haylen::animation3d::Skinning3D::applyDualQuat(weights, dualQuats, vertices, &jobs);
    } else {
        matrices.resize(jointSkin->getJointCount());
        jointSkin->computePalette(pose.pose, matrices);
        haylen::animation3d::Skinning3D::applyLinear(weights, matrices, vertices, &jobs);
    }

    writeVectors(skinnedPositions, skinnedTarget);
    writeVectors(skinnedNormals, normalTarget);
    return 0;
}

void Skin3DBinding::install(lua_State* L) {
    lua::ClassBuilder<Skin3D>(L).property("jointCount", &lua::Binding::native<&jointCount>).property("joints", &lua::Binding::native<&joints>).property("skeleton", &lua::Binding::native<&skinSkeleton>).function("palette", &lua::Binding::native<&palette>).function("dualQuatPalette", &lua::Binding::native<&dualQuatPalette>).install();
    lua::ClassBuilder<SkinWeights3D>(L).property("influenceCount", &lua::Binding::native<&influenceCount>).property("vertexCount", &lua::Binding::native<&vertexCount>).property("jointCount", &lua::Binding::native<&weightsJointCount>).function("readJoints", &lua::Binding::native<&readJoints>).function("readWeights", &lua::Binding::native<&readWeights>).function("skin", &lua::Binding::native<&skin>).install();
}

} // namespace haylen::lua::animation3d
