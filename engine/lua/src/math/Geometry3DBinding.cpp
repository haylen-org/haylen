#include "math/Geometry3DBinding.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/math/Geometry3D.hpp"
#include "math/MatrixBinding.hpp"
#include "math/NumberText.hpp"

namespace haylen::lua::math {

int Geometry3DBinding::planeToString(lua_State* L) {
    const haylen::math::Plane& plane = lua::Userdata::check<haylen::math::Plane>(L, 1);
    lua_pushfstring(L, "Plane((%f, %f, %f), %f)", NumberText::plain(plane.normal.x), NumberText::plain(plane.normal.y), NumberText::plain(plane.normal.z), NumberText::plain(plane.distance));
    return 1;
}

int Geometry3DBinding::planeEqual(lua_State* L) {
    lua_pushboolean(L, lua::Stack::read<haylen::math::Plane>(L, 1) == lua::Stack::read<haylen::math::Plane>(L, 2) ? 1 : 0);
    return 1;
}

// A plane keeps a normal of length one, which every distance it measures relies on.
void Geometry3DBinding::checkNormal(haylen::math::Vec3 normal) {
    if (!(std::fabs(normal.getLength() - 1.0F) <= 1e-4F)) {
        throw std::invalid_argument("The normal of a plane must have a length of one. Make the plane with \"plane\" of \"haylen.math\", which scales its normal.");
    }
}

// A ray keeps a direction of length one, so its hits measure distances.
void Geometry3DBinding::checkDirection(haylen::math::Vec3 direction) {
    if (!(std::fabs(direction.getLength() - 1.0F) <= 1e-4F)) {
        throw std::invalid_argument("The direction of a ray must have a length of one. Make the ray with \"ray3d\" of \"haylen.math\", which scales its direction.");
    }
}

// A sphere or a capsule reaches a distance that cannot be negative.
void Geometry3DBinding::checkRadius(float radius) {
    if (!std::isfinite(radius) || radius < 0.0F) {
        throw std::invalid_argument("A radius must be a finite number of 0 or more.");
    }
}

void Geometry3DBinding::checkExtents(haylen::math::Vec3 halfExtents) {
    if (!halfExtents.isFinite() || halfExtents.x < 0.0F || halfExtents.y < 0.0F || halfExtents.z < 0.0F) {
        throw std::invalid_argument("The half extents of an oriented box must be finite numbers of 0 or more.");
    }
}

haylen::math::Ray3D Geometry3DBinding::newRay(haylen::math::Vec3 origin, haylen::math::Vec3 direction, std::optional<float> length) {
    return {origin, direction.getNormalized(), length.value_or(std::numeric_limits<float>::infinity())};
}

// Takes the matrix `projection * view`, the point of the view and the options of `MatrixBinding::readDepthOptions`.
int Geometry3DBinding::rayFromView(lua_State* L) {
    const haylen::math::Mat4 viewProjection = lua::Stack::read<haylen::math::Mat4>(L, 1);
    const haylen::math::Vec2 point = lua::Stack::read<haylen::math::Vec2>(L, 2);
    const auto [depthRange, reversedDepth] = MatrixBinding::readDepthOptions(L, 3);
    lua::Stack::push(L, haylen::math::Ray3D::fromView(viewProjection, point, depthRange, reversedDepth));
    return 1;
}

haylen::math::Box Geometry3DBinding::boxBounds(std::vector<haylen::math::Vec3> points) {
    return haylen::math::Box::bounds(points);
}

void Geometry3DBinding::pushHit(lua_State* L, const std::optional<haylen::math::RayHit3D>& hit) {
    if (!hit) {
        lua_pushnil(L);
        return;
    }

    lua_createtable(L, 0, 3);
    lua::Stack::push(L, hit->distance);
    lua_setfield(L, -2, "distance");
    lua::Stack::push(L, hit->point);
    lua_setfield(L, -2, "point");
    lua::Stack::push(L, hit->normal);
    lua_setfield(L, -2, "normal");
}

// Casts against the shape at index 2, which its type names.
int Geometry3DBinding::rayCast(lua_State* L) {
    const haylen::math::Ray3D& ray = lua::Userdata::check<haylen::math::Ray3D>(L, 1);
    if (const auto* triangle = lua::Userdata::test<haylen::math::Triangle>(L, 2)) {
        pushHit(L, haylen::math::Geometry3D::raycast(ray, *triangle));
    } else if (const auto* box = lua::Userdata::test<haylen::math::Box>(L, 2)) {
        pushHit(L, haylen::math::Geometry3D::raycast(ray, *box));
    } else if (const auto* oriented = lua::Userdata::test<haylen::math::Obb>(L, 2)) {
        pushHit(L, haylen::math::Geometry3D::raycast(ray, *oriented));
    } else if (const auto* sphere = lua::Userdata::test<haylen::math::Sphere>(L, 2)) {
        pushHit(L, haylen::math::Geometry3D::raycast(ray, *sphere));
    } else if (const auto* plane = lua::Userdata::test<haylen::math::Plane>(L, 2)) {
        pushHit(L, haylen::math::Geometry3D::raycast(ray, *plane));
    } else if (const auto* capsule = lua::Userdata::test<haylen::math::Capsule>(L, 2)) {
        pushHit(L, haylen::math::Geometry3D::raycast(ray, *capsule));
    } else {
        return luaL_error(L, "A ray casts against a Triangle, a Box, an Obb, a Sphere, a Plane or a Capsule, not a %s.", luaL_typename(L, 2));
    }

    return 1;
}

// Tells points and boxes apart by their type.
int Geometry3DBinding::boxContains(lua_State* L) {
    const haylen::math::Box& self = lua::Userdata::check<haylen::math::Box>(L, 1);
    if (const auto* other = lua::Userdata::test<haylen::math::Box>(L, 2)) {
        lua::Stack::push(L, self.contains(*other));
        return 1;
    }

    lua::Stack::push(L, self.contains(lua::Stack::read<haylen::math::Vec3>(L, 2)));
    return 1;
}

int Geometry3DBinding::boxIntersects(lua_State* L) {
    const haylen::math::Box& self = lua::Userdata::check<haylen::math::Box>(L, 1);
    if (const auto* sphere = lua::Userdata::test<haylen::math::Sphere>(L, 2)) {
        lua::Stack::push(L, sphere->intersects(self));
        return 1;
    }

    lua::Stack::push(L, self.intersects(lua::Userdata::check<haylen::math::Box>(L, 2)));
    return 1;
}

int Geometry3DBinding::boxMerged(lua_State* L) {
    const haylen::math::Box& self = lua::Userdata::check<haylen::math::Box>(L, 1);
    if (const auto* other = lua::Userdata::test<haylen::math::Box>(L, 2)) {
        lua::Stack::push(L, self.merged(*other));
        return 1;
    }

    lua::Stack::push(L, self.merged(lua::Stack::read<haylen::math::Vec3>(L, 2)));
    return 1;
}

haylen::math::Box Geometry3DBinding::boxTransformed(const haylen::math::Box& self, const haylen::math::Mat4& placement) {
    return self.transformed(placement);
}

int Geometry3DBinding::sphereIntersects(lua_State* L) {
    const haylen::math::Sphere& self = lua::Userdata::check<haylen::math::Sphere>(L, 1);
    if (const auto* box = lua::Userdata::test<haylen::math::Box>(L, 2)) {
        lua::Stack::push(L, self.intersects(*box));
        return 1;
    }

    lua::Stack::push(L, self.intersects(lua::Userdata::check<haylen::math::Sphere>(L, 2)));
    return 1;
}

bool Geometry3DBinding::obbIntersects(const haylen::math::Obb& self, const haylen::math::Obb& other) {
    return haylen::math::Geometry3D::intersects(self, other);
}

haylen::math::Vec3 Geometry3DBinding::triangleNormal(const haylen::math::Triangle& self) {
    return self.getNormal();
}

int Geometry3DBinding::segmentClosestPoints(lua_State* L) {
    const auto [onSelf, onOther] = haylen::math::Geometry3D::closestPoints(lua::Userdata::check<haylen::math::Segment3D>(L, 1), lua::Userdata::check<haylen::math::Segment3D>(L, 2));
    lua::Stack::push(L, onSelf);
    lua::Stack::push(L, onOther);
    return 2;
}

haylen::math::Frustum Geometry3DBinding::newFrustum(const haylen::math::Mat4& viewProjection, std::optional<haylen::math::Mat4::DepthRange> depthRange) {
    return haylen::math::Frustum::fromMatrix(viewProjection, depthRange.value_or(haylen::math::Mat4::DepthRange::ZeroToOne));
}

int Geometry3DBinding::frustumPlanes(lua_State* L) {
    const std::span<const haylen::math::Plane> planes = lua::Userdata::check<haylen::math::Frustum>(L, 1).getPlanes();
    lua_createtable(L, static_cast<int>(planes.size()), 0);
    for (std::size_t index = 0; index < planes.size(); ++index) {
        lua::Stack::push(L, planes[index]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    return 1;
}

bool Geometry3DBinding::frustumContains(const haylen::math::Frustum& self, haylen::math::Vec3 point, std::optional<float> tolerance) {
    return self.contains(point, tolerance.value_or(haylen::math::Frustum::kTolerance));
}

int Geometry3DBinding::frustumClassify(lua_State* L) {
    const haylen::math::Frustum& self = lua::Userdata::check<haylen::math::Frustum>(L, 1);
    const float tolerance = lua::Converter<float>::readOr(L, 3, haylen::math::Frustum::kTolerance);
    if (const auto* sphere = lua::Userdata::test<haylen::math::Sphere>(L, 2)) {
        lua::Stack::push(L, self.classify(*sphere, tolerance));
        return 1;
    }

    lua::Stack::push(L, self.classify(lua::Userdata::check<haylen::math::Box>(L, 2), tolerance));
    return 1;
}

// Returns the positions in the list of the boxes that are not wholly outside, from 1.
int Geometry3DBinding::frustumCull(lua_State* L) {
    const haylen::math::Frustum& self = lua::Userdata::check<haylen::math::Frustum>(L, 1);
    const std::vector<haylen::math::Box> boxes = lua::Stack::read<std::vector<haylen::math::Box>>(L, 2);
    const float tolerance = lua::Converter<float>::readOr(L, 3, haylen::math::Frustum::kTolerance);
    std::vector<std::uint32_t> visible(boxes.size());
    const std::size_t count = self.cull(boxes, visible, tolerance);
    lua_createtable(L, static_cast<int>(count), 0);
    for (std::size_t index = 0; index < count; ++index) {
        lua_pushinteger(L, static_cast<lua_Integer>(visible[index]) + 1);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    return 1;
}

void Geometry3DBinding::install(lua_State* L) {
    using haylen::math::Box;
    using haylen::math::Capsule;
    using haylen::math::Frustum;
    using haylen::math::Obb;
    using haylen::math::Plane;
    using haylen::math::Ray3D;
    using haylen::math::Segment3D;
    using haylen::math::Sphere;
    using haylen::math::Triangle;

    lua::ClassBuilder<Plane>(L).checkedField<&Plane::normal, &checkNormal>("normal").field<&Plane::distance>("distance").method<&Plane::getSignedDistance>("signedDistance").method<&Plane::getClosestPoint>("closestPoint").method<&Plane::getCoefficients>("coefficients").meta("__eq", &lua::Binding::native<&planeEqual>).meta("__tostring", &lua::Binding::native<&planeToString>).install();

    lua::ClassBuilder<Ray3D>(L).field<&Ray3D::origin>("origin").checkedField<&Ray3D::direction, &checkDirection>("direction").field<&Ray3D::length>("length").method<&Ray3D::at>("at").method<&Ray3D::getEndPoint>("endPoint").function("cast", &lua::Binding::native<&rayCast>).install();

    lua::ClassBuilder<Segment3D>(L).field<&Segment3D::start>("start").field<&Segment3D::end>("end").method<&Segment3D::getLength>("length").method<&Segment3D::getClosestPoint>("closestPoint").function("closestPoints", &lua::Binding::native<&segmentClosestPoints>).install();

    lua::ClassBuilder<Box>(L).field<&Box::min>("min").field<&Box::max>("max").method<&Box::getCenter>("center").method<&Box::getSize>("size").method<&Box::getHalfExtents>("halfExtents").method<&Box::isEmpty>("isEmpty").function("contains", &lua::Binding::native<&boxContains>).function("intersects", &lua::Binding::native<&boxIntersects>).function("merged", &lua::Binding::native<&boxMerged>).method<&Box::expanded>("expanded").method<&Box::clamp>("clamp").function("transformed", &lua::Binding::function<&boxTransformed>).meta("__eq", &lua::Binding::native<&lua::Userdata::equal<Box>>).install();

    lua::ClassBuilder<Sphere>(L).field<&Sphere::center>("center").checkedField<&Sphere::radius, &checkRadius>("radius").method<&Sphere::contains>("contains").function("intersects", &lua::Binding::native<&sphereIntersects>).method<&Sphere::getBounds>("bounds").install();

    lua::ClassBuilder<Obb>(L).field<&Obb::center>("center").field<&Obb::rotation>("rotation").checkedField<&Obb::halfExtents, &checkExtents>("halfExtents").method<&Obb::getAxes>("axes").method<&Obb::contains>("contains").method<&Obb::getClosestPoint>("closestPoint").method<&Obb::getBounds>("bounds").function("intersects", &lua::Binding::function<&obbIntersects>).install();

    lua::ClassBuilder<Capsule>(L).field<&Capsule::start>("start").field<&Capsule::end>("end").checkedField<&Capsule::radius, &checkRadius>("radius").method<&Capsule::getAxis>("axis").method<&Capsule::contains>("contains").method<&Capsule::getBounds>("bounds").install();

    lua::ClassBuilder<Triangle>(L).field<&Triangle::a>("a").field<&Triangle::b>("b").field<&Triangle::c>("c").method<&Triangle::getArea>("area").function("normal", &lua::Binding::function<&triangleNormal>).method<&Triangle::getClosestPoint>("closestPoint").install();

    lua::ClassBuilder<Frustum>(L).function("planes", &lua::Binding::native<&frustumPlanes>).function("contains", &lua::Binding::function<&frustumContains>).function("classify", &lua::Binding::native<&frustumClassify>).function("cull", &lua::Binding::native<&frustumCull>).install();
}

void Geometry3DBinding::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"plane", &lua::Binding::function<&haylen::math::Plane::fromNormal>}, {"planeFromPoint", &lua::Binding::function<&haylen::math::Plane::fromPoint>}, {"planeFromPoints", &lua::Binding::function<&haylen::math::Plane::fromPoints>}, {"ray3d", &lua::Binding::function<&newRay>}, {"ray3dBetween", &lua::Binding::function<&haylen::math::Ray3D::between>}, {"ray3dFromView", &lua::Binding::native<&rayFromView>}, {"segment3d", &lua::Binding::function<&newSegment>}, {"box", &lua::Binding::function<&newBox>}, {"boxFromCenter", &lua::Binding::function<&haylen::math::Box::fromCenter>}, {"boxBounds", &lua::Binding::function<&boxBounds>}, {"sphere", &lua::Binding::function<&newSphere>}, {"obb", &lua::Binding::function<&newObb>}, {"obbFromTransform", &lua::Binding::function<&haylen::math::Obb::fromTransform>}, {"capsule", &lua::Binding::function<&newCapsule>}, {"triangle", &lua::Binding::function<&newTriangle>}, {"frustum", &lua::Binding::function<&newFrustum>}, {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

haylen::math::Segment3D Geometry3DBinding::newSegment(haylen::math::Vec3 start, haylen::math::Vec3 end) {
    return {start, end};
}

haylen::math::Box Geometry3DBinding::newBox(haylen::math::Vec3 minimum, haylen::math::Vec3 maximum) {
    return {minimum, maximum};
}

haylen::math::Sphere Geometry3DBinding::newSphere(haylen::math::Vec3 center, float radius) {
    checkRadius(radius);
    return {center, radius};
}

haylen::math::Obb Geometry3DBinding::newObb(haylen::math::Vec3 center, std::optional<haylen::math::Quat> rotation, haylen::math::Vec3 halfExtents) {
    checkExtents(halfExtents);
    return {center, rotation.value_or(haylen::math::Quat::identity()), halfExtents};
}

haylen::math::Capsule Geometry3DBinding::newCapsule(haylen::math::Vec3 start, haylen::math::Vec3 end, float radius) {
    checkRadius(radius);
    return {start, end, radius};
}

haylen::math::Triangle Geometry3DBinding::newTriangle(haylen::math::Vec3 a, haylen::math::Vec3 b, haylen::math::Vec3 c) {
    return {a, b, c};
}

} // namespace haylen::lua::math
