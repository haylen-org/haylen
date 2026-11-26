#pragma once

#include <memory>

#include "haylen/3d/physics/Body.hpp"
#include "haylen/3d/physics/World.hpp"
#include "haylen/math/Quat.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::lua::physics3d {

// A body held by Lua, which keeps its world alive. Its user value is the Lua world object, which keeps the data of the bodies, and the transform and velocities that tweens animate go through it.
struct ScriptedBody {
    std::shared_ptr<haylen::physics3d::World> world;
    haylen::physics3d::Body body;

    [[nodiscard]] bool isValid() const noexcept {
        return body.isValid();
    }
    [[nodiscard]] haylen::math::Vec3 getPosition() const {
        return body.getPosition();
    }
    void setPosition(haylen::math::Vec3 value) {
        body.setPosition(value);
    }
    [[nodiscard]] haylen::math::Quat getRotation() const {
        return body.getRotation();
    }
    void setRotation(haylen::math::Quat value) {
        body.setRotation(value);
    }
    [[nodiscard]] haylen::math::Vec3 getLinearVelocity() const {
        return body.getLinearVelocity();
    }
    void setLinearVelocity(haylen::math::Vec3 value) {
        body.setLinearVelocity(value);
    }
    [[nodiscard]] haylen::math::Vec3 getAngularVelocity() const {
        return body.getAngularVelocity();
    }
    void setAngularVelocity(haylen::math::Vec3 value) {
        body.setAngularVelocity(value);
    }
};

} // namespace haylen::lua::physics3d
