#include "3d/animation/Skeleton3DBinding.hpp"

#include <lua.hpp>

#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include "3d/animation/Clip3DBinding.hpp"
#include "3d/gltf/GltfBinding.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua::animation3d {

using haylen::animation3d::Pose3D;
using haylen::animation3d::PoseSampler3D;
using haylen::animation3d::Skeleton3D;

std::shared_ptr<const Skeleton3D> Skeleton3DBinding::checkSkeleton(lua_State* L, int index) {
    return lua::Userdata::checkShared<Skeleton3D>(L, index);
}

ScriptedPose3D& Skeleton3DBinding::checkPose(lua_State* L, int index) {
    return lua::Userdata::check<ScriptedPose3D>(L, index);
}

// A skeleton never changes once made, so Lua holds the same object that poses and skins share.
void Skeleton3DBinding::push(lua_State* L, std::shared_ptr<const Skeleton3D> skeleton) {
    lua::Userdata::emplace<Skeleton3D>(L, std::const_pointer_cast<Skeleton3D>(std::move(skeleton)));
}

std::size_t Skeleton3DBinding::readBone(lua_State* L, int index, const Skeleton3D& skeleton) {
    if (lua_type(L, index) == LUA_TSTRING) {
        const std::string_view name = lua::Stack::read<std::string_view>(L, index);
        const std::optional<std::size_t> bone = skeleton.findBone(name);
        if (!bone) {
            throw std::invalid_argument(std::format("The skeleton has no bone \"{}\".", name));
        }

        return *bone;
    }

    const lua_Integer place = lua::Stack::read<lua_Integer>(L, index);
    if (place < 1 || static_cast<std::size_t>(place) > skeleton.getBoneCount()) {
        throw std::invalid_argument(std::format("The skeleton has {} bones, counted from 1, so it has no bone {}.", skeleton.getBoneCount(), place));
    }

    return static_cast<std::size_t>(place - 1);
}

int Skeleton3DBinding::create(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    std::vector<Skeleton3D::Bone> bones;
    const lua_Integer count = luaL_len(L, 1);
    for (lua_Integer place = 1; place <= count; ++place) {
        lua_rawgeti(L, 1, place);
        lua::Table::requireItemType(L, "bones", place, LUA_TTABLE);
        const int entry = lua_gettop(L);
        lua::Table::checkFields(L, entry, {kBoneFields});
        Skeleton3D::Bone bone;
        lua::Table::readField(L, entry, "name", bone.name);
        lua::Table::readField(L, entry, "position", bone.rest.position);
        lua::Table::readField(L, entry, "rotation", bone.rest.rotation);
        lua::Table::readField(L, entry, "scale", bone.rest.scale);
        lua_getfield(L, entry, "parent");
        if (!lua_isnil(L, -1)) {
            lua_Integer parent = 0;
            lua::Table::readValue(L, "parent", parent);
            if (parent < 1) {
                throw std::invalid_argument(std::format("The parent of the bone \"{}\" is the place of a bone counted from 1, while it was given {}.", bone.name, parent));
            }

            bone.parent = static_cast<std::size_t>(parent - 1);
        }

        lua_pop(L, 2);
        bones.push_back(std::move(bone));
    }

    push(L, std::make_shared<const Skeleton3D>(std::move(bones)));
    return 1;
}

int Skeleton3DBinding::fromGltf(lua_State* L) {
    const haylen::gltf::Document& document = lua::Userdata::check<haylen::gltf::Document>(L, 1);
    const std::size_t skin = lua::gltf::GltfBinding::checkObject(L, 2, "skins", document.skins.size());
    push(L, std::make_shared<const Skeleton3D>(Skeleton3D::fromGltf(document, skin)));
    return 1;
}

int Skeleton3DBinding::createPose(lua_State* L) {
    lua::Userdata::emplace<ScriptedPose3D>(L, std::make_shared<ScriptedPose3D>(ScriptedPose3D{.pose = Pose3D(checkSkeleton(L, 1)), .sampler = nullptr, .cursors = {}}));
    return 1;
}

int Skeleton3DBinding::boneCount(lua_State* L) {
    lua::Stack::push(L, checkSkeleton(L, 1)->getBoneCount());
    return 1;
}

// Lists the bones as `{name, parent, position, rotation, scale}`, with the parent counted from 1 and nil for a root.
int Skeleton3DBinding::bones(lua_State* L) {
    const std::span<const Skeleton3D::Bone> list = checkSkeleton(L, 1)->getBones();
    lua_createtable(L, static_cast<int>(list.size()), 0);
    for (std::size_t index = 0; index < list.size(); ++index) {
        const Skeleton3D::Bone& bone = list[index];
        lua_createtable(L, 0, 5);
        lua::Stack::push(L, bone.name);
        lua_setfield(L, -2, "name");
        if (bone.parent) {
            lua::Stack::push(L, *bone.parent + 1);
            lua_setfield(L, -2, "parent");
        }

        lua::Stack::push(L, bone.rest.position);
        lua_setfield(L, -2, "position");
        lua::Stack::push(L, bone.rest.rotation);
        lua_setfield(L, -2, "rotation");
        lua::Stack::push(L, bone.rest.scale);
        lua_setfield(L, -2, "scale");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    return 1;
}

int Skeleton3DBinding::findBone(lua_State* L) {
    const std::optional<std::size_t> bone = checkSkeleton(L, 1)->findBone(lua::Stack::read<std::string_view>(L, 2));
    if (!bone) {
        lua_pushnil(L);
        return 1;
    }

    lua::Stack::push(L, *bone + 1);
    return 1;
}

int Skeleton3DBinding::poseSkeleton(lua_State* L) {
    push(L, checkPose(L, 1).pose.getSkeletonPointer());
    return 1;
}

int Skeleton3DBinding::setRest(lua_State* L) {
    checkPose(L, 1).pose.setRest();
    return 0;
}

// Writes the bones that a clip moves at a time with `sample(clip, time)`, binding a clip once until another clip comes.
int Skeleton3DBinding::sample(lua_State* L) {
    ScriptedPose3D& self = checkPose(L, 1);
    const std::shared_ptr<haylen::animation3d::Clip3D>& clip = lua::Userdata::checkShared<haylen::animation3d::Clip3D>(L, 2);
    const float time = lua::Stack::read<float>(L, 3);
    if (!self.sampler || self.sampler->getClipPointer() != clip) {
        self.sampler = std::make_shared<const PoseSampler3D>(clip, self.pose.getSkeletonPointer());
        self.cursors.assign(self.sampler->getBoundTrackCount(), 0);
    }

    self.sampler->sample(time, self.pose.getLocalTransforms(), self.cursors);
    return 0;
}

int Skeleton3DBinding::transform(lua_State* L) {
    ScriptedPose3D& self = checkPose(L, 1);
    lua::Stack::push(L, self.pose.getLocalTransforms()[readBone(L, 2, self.pose.getSkeleton())]);
    return 1;
}

int Skeleton3DBinding::setTransform(lua_State* L) {
    ScriptedPose3D& self = checkPose(L, 1);
    self.pose.getLocalTransforms()[readBone(L, 2, self.pose.getSkeleton())] = lua::Stack::read<haylen::math::Transform3D>(L, 3);
    return 0;
}

int Skeleton3DBinding::computeModelMatrices(lua_State* L) {
    checkPose(L, 1).pose.computeModelMatrices();
    return 0;
}

// Returns the matrix of a bone in the space of the skeleton as of the last `computeModelMatrices`.
int Skeleton3DBinding::modelMatrix(lua_State* L) {
    ScriptedPose3D& self = checkPose(L, 1);
    lua::Stack::push(L, self.pose.getModelMatrices()[readBone(L, 2, self.pose.getSkeleton())]);
    return 1;
}

void Skeleton3DBinding::install(lua_State* L) {
    lua::ClassBuilder<Skeleton3D>(L).property("boneCount", &lua::Binding::native<&boneCount>).property("bones", &lua::Binding::native<&bones>).function("findBone", &lua::Binding::native<&findBone>).meta("__eq", &lua::Userdata::equal<Skeleton3D>).install();
    lua::ClassBuilder<ScriptedPose3D>(L).property("skeleton", &lua::Binding::native<&poseSkeleton>).function("setRest", &lua::Binding::native<&setRest>).function("sample", &lua::Binding::native<&sample>).function("transform", &lua::Binding::native<&transform>).function("setTransform", &lua::Binding::native<&setTransform>).function("computeModelMatrices", &lua::Binding::native<&computeModelMatrices>).function("modelMatrix", &lua::Binding::native<&modelMatrix>).install();
}

} // namespace haylen::lua::animation3d
