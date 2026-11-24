#include "math/Geometry3Binding.hpp"

#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/math/Geometry3.hpp"
#include "math/NumberText.hpp"

namespace haylen::lua::math {

int Geometry3Binding::planeToString(lua_State* L) {
    const haylen::math::Plane& plane = lua::Userdata::check<haylen::math::Plane>(L, 1);
    lua_pushfstring(L, "Plane((%f, %f, %f), %f)", NumberText::plain(plane.normal.x), NumberText::plain(plane.normal.y), NumberText::plain(plane.normal.z), NumberText::plain(plane.distance));
    return 1;
}

int Geometry3Binding::planeEqual(lua_State* L) {
    lua_pushboolean(L, lua::Stack::read<haylen::math::Plane>(L, 1) == lua::Stack::read<haylen::math::Plane>(L, 2) ? 1 : 0);
    return 1;
}

// A plane keeps a normal of length one, which every distance it measures relies on.
void Geometry3Binding::checkNormal(haylen::math::Vec3 normal) {
    if (!(std::fabs(normal.getLength() - 1.0F) <= 1e-4F)) {
        throw std::invalid_argument("The normal of a plane must have a length of one. Make the plane with \"plane\" of \"haylen.math\", which scales its normal.");
    }
}

// A ray keeps a direction of length one, so its hits measure distances.
void Geometry3Binding::checkDirection(haylen::math::Vec3 direction) {
    if (!(std::fabs(direction.getLength() - 1.0F) <= 1e-4F)) {
        throw std::invalid_argument("The direction of a ray must have a length of one. Make the ray with \"ray3\" of \"haylen.math\", which scales its direction.");
    }
}

// A sphere or a capsule reaches a distance that cannot be negative.
void Geometry3Binding::checkRadius(float radius) {
    if (!std::isfinite(radius) || radius < 0.0F) {
        throw std::invalid_argument("A radius must be a finite number of 0 or more.");
    }
}

void Geometry3Binding::checkExtents(haylen::math::Vec3 halfExtents) {
    if (!halfExtents.isFinite() || halfExtents.x < 0.0F || halfExtents.y < 0.0F || halfExtents.z < 0.0F) {
        throw std::invalid_argument("The half extents of an oriented box must be finite numbers of 0 or more.");
    }
}

haylen::math::Ray3 Geometry3Binding::newRay(haylen::math::Vec3 origin, haylen::math::Vec3 direction, std::optional<float> length) {
    return {origin, direction.getNormalized(), length.value_or(std::numeric_limits<float>::infinity())};
}

haylen::math::Box3 Geometry3Binding::boxBounds(std::vector<haylen::math::Vec3> points) {
    return haylen::math::Box3::bounds(points);
}

void Geometry3Binding::pushHit(lua_State* L, const std::optional<haylen::math::RayHit3>& hit) {
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
int Geometry3Binding::rayCast(lua_State* L) {
    const haylen::math::Ray3& ray = lua::Userdata::check<haylen::math::Ray3>(L, 1);
    if (const auto* triangle = lua::Userdata::test<haylen::math::Triangle3>(L, 2)) {
        pushHit(L, haylen::math::Geometry3::raycast(ray, *triangle));
    } else if (const auto* box = lua::Userdata::test<haylen::math::Box3>(L, 2)) {
        pushHit(L, haylen::math::Geometry3::raycast(ray, *box));
    } else if (const auto* oriented = lua::Userdata::test<haylen::math::Obb>(L, 2)) {
        pushHit(L, haylen::math::Geometry3::raycast(ray, *oriented));
    } else if (const auto* sphere = lua::Userdata::test<haylen::math::Sphere>(L, 2)) {
        pushHit(L, haylen::math::Geometry3::raycast(ray, *sphere));
    } else if (const auto* plane = lua::Userdata::test<haylen::math::Plane>(L, 2)) {
        pushHit(L, haylen::math::Geometry3::raycast(ray, *plane));
    } else if (const auto* capsule = lua::Userdata::test<haylen::math::Capsule3>(L, 2)) {
        pushHit(L, haylen::math::Geometry3::raycast(ray, *capsule));
    } else {
        return luaL_error(L, "A ray casts against a Triangle3, a Box3, an Obb, a Sphere, a Plane or a Capsule3, not a %s.", luaL_typename(L, 2));
    }

    return 1;
}

// Tells points and boxes apart by their type.
int Geometry3Binding::boxContains(lua_State* L) {
    const haylen::math::Box3& self = lua::Userdata::check<haylen::math::Box3>(L, 1);
    if (const auto* other = lua::Userdata::test<haylen::math::Box3>(L, 2)) {
        lua::Stack::push(L, self.contains(*other));
        return 1;
    }

    lua::Stack::push(L, self.contains(lua::Stack::read<haylen::math::Vec3>(L, 2)));
    return 1;
}

int Geometry3Binding::boxIntersects(lua_State* L) {
    const haylen::math::Box3& self = lua::Userdata::check<haylen::math::Box3>(L, 1);
    if (const auto* sphere = lua::Userdata::test<haylen::math::Sphere>(L, 2)) {
        lua::Stack::push(L, sphere->intersects(self));
        return 1;
    }

    lua::Stack::push(L, self.intersects(lua::Userdata::check<haylen::math::Box3>(L, 2)));
    return 1;
}

int Geometry3Binding::boxMerged(lua_State* L) {
    const haylen::math::Box3& self = lua::Userdata::check<haylen::math::Box3>(L, 1);
    if (const auto* other = lua::Userdata::test<haylen::math::Box3>(L, 2)) {
        lua::Stack::push(L, self.merged(*other));
        return 1;
    }

    lua::Stack::push(L, self.merged(lua::Stack::read<haylen::math::Vec3>(L, 2)));
    return 1;
}

haylen::math::Box3 Geometry3Binding::boxTransformed(const haylen::math::Box3& self, const haylen::math::Mat4& placement) {
    return self.transformed(placement);
}

int Geometry3Binding::sphereIntersects(lua_State* L) {
    const haylen::math::Sphere& self = lua::Userdata::check<haylen::math::Sphere>(L, 1);
    if (const auto* box = lua::Userdata::test<haylen::math::Box3>(L, 2)) {
        lua::Stack::push(L, self.intersects(*box));
        return 1;
    }

    lua::Stack::push(L, self.intersects(lua::Userdata::check<haylen::math::Sphere>(L, 2)));
    return 1;
}

bool Geometry3Binding::obbIntersects(const haylen::math::Obb& self, const haylen::math::Obb& other) {
    return haylen::math::Geometry3::intersects(self, other);
}

haylen::math::Vec3 Geometry3Binding::triangleNormal(const haylen::math::Triangle3& self) {
    return self.getNormal();
}

int Geometry3Binding::segmentClosestPoints(lua_State* L) {
    const auto [onSelf, onOther] = haylen::math::Geometry3::closestPoints(lua::Userdata::check<haylen::math::Segment3>(L, 1), lua::Userdata::check<haylen::math::Segment3>(L, 2));
    lua::Stack::push(L, onSelf);
    lua::Stack::push(L, onOther);
    return 2;
}

haylen::math::Frustum Geometry3Binding::newFrustum(const haylen::math::Mat4& viewProjection, std::optional<haylen::math::Mat4::DepthRange> depthRange) {
    return haylen::math::Frustum::fromMatrix(viewProjection, depthRange.value_or(haylen::math::Mat4::DepthRange::ZeroToOne));
}

int Geometry3Binding::frustumPlanes(lua_State* L) {
    const std::span<const haylen::math::Plane> planes = lua::Userdata::check<haylen::math::Frustum>(L, 1).getPlanes();
    lua_createtable(L, static_cast<int>(planes.size()), 0);
    for (std::size_t index = 0; index < planes.size(); ++index) {
        lua::Stack::push(L, planes[index]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    return 1;
}

bool Geometry3Binding::frustumContains(const haylen::math::Frustum& self, haylen::math::Vec3 point, std::optional<float> tolerance) {
    return self.contains(point, tolerance.value_or(haylen::math::Frustum::kTolerance));
}

int Geometry3Binding::frustumClassify(lua_State* L) {
    const haylen::math::Frustum& self = lua::Userdata::check<haylen::math::Frustum>(L, 1);
    const float tolerance = lua::Converter<float>::readOr(L, 3, haylen::math::Frustum::kTolerance);
    if (const auto* sphere = lua::Userdata::test<haylen::math::Sphere>(L, 2)) {
        lua::Stack::push(L, self.classify(*sphere, tolerance));
        return 1;
    }

    lua::Stack::push(L, self.classify(lua::Userdata::check<haylen::math::Box3>(L, 2), tolerance));
    return 1;
}

void Geometry3Binding::install(lua_State* L) {
    using haylen::math::Box3;
    using haylen::math::Capsule3;
    using haylen::math::Frustum;
    using haylen::math::Obb;
    using haylen::math::Plane;
    using haylen::math::Ray3;
    using haylen::math::Segment3;
    using haylen::math::Sphere;
    using haylen::math::Triangle3;

    lua::ClassBuilder<Plane>(L).checkedField<&Plane::normal, &checkNormal>("normal").field<&Plane::distance>("distance").method<&Plane::getSignedDistance>("signedDistance").method<&Plane::getClosestPoint>("closestPoint").method<&Plane::getCoefficients>("coefficients").meta("__eq", &lua::Binding::native<&planeEqual>).meta("__tostring", &lua::Binding::native<&planeToString>).install();

    lua::ClassBuilder<Ray3>(L).field<&Ray3::origin>("origin").checkedField<&Ray3::direction, &checkDirection>("direction").field<&Ray3::length>("length").method<&Ray3::at>("at").method<&Ray3::getEndPoint>("endPoint").function("cast", &lua::Binding::native<&rayCast>).install();

    lua::ClassBuilder<Segment3>(L).field<&Segment3::start>("start").field<&Segment3::end>("end").method<&Segment3::getLength>("length").method<&Segment3::getClosestPoint>("closestPoint").function("closestPoints", &lua::Binding::native<&segmentClosestPoints>).install();

    lua::ClassBuilder<Box3>(L).field<&Box3::min>("min").field<&Box3::max>("max").method<&Box3::getCenter>("center").method<&Box3::getSize>("size").method<&Box3::getHalfExtents>("halfExtents").method<&Box3::isEmpty>("isEmpty").function("contains", &lua::Binding::native<&boxContains>).function("intersects", &lua::Binding::native<&boxIntersects>).function("merged", &lua::Binding::native<&boxMerged>).method<&Box3::expanded>("expanded").method<&Box3::clamp>("clamp").function("transformed", &lua::Binding::function<&boxTransformed>).meta("__eq", &lua::Binding::native<&lua::Userdata::equal<Box3>>).install();

    lua::ClassBuilder<Sphere>(L).field<&Sphere::center>("center").checkedField<&Sphere::radius, &checkRadius>("radius").method<&Sphere::contains>("contains").function("intersects", &lua::Binding::native<&sphereIntersects>).method<&Sphere::getBounds>("bounds").install();

    lua::ClassBuilder<Obb>(L).field<&Obb::center>("center").field<&Obb::rotation>("rotation").checkedField<&Obb::halfExtents, &checkExtents>("halfExtents").method<&Obb::getAxes>("axes").method<&Obb::contains>("contains").method<&Obb::getClosestPoint>("closestPoint").method<&Obb::getBounds>("bounds").function("intersects", &lua::Binding::function<&obbIntersects>).install();

    lua::ClassBuilder<Capsule3>(L).field<&Capsule3::start>("start").field<&Capsule3::end>("end").checkedField<&Capsule3::radius, &checkRadius>("radius").method<&Capsule3::getAxis>("axis").method<&Capsule3::contains>("contains").method<&Capsule3::getBounds>("bounds").install();

    lua::ClassBuilder<Triangle3>(L).field<&Triangle3::a>("a").field<&Triangle3::b>("b").field<&Triangle3::c>("c").method<&Triangle3::getArea>("area").function("normal", &lua::Binding::function<&triangleNormal>).method<&Triangle3::getClosestPoint>("closestPoint").install();

    lua::ClassBuilder<Frustum>(L).function("planes", &lua::Binding::native<&frustumPlanes>).function("contains", &lua::Binding::function<&frustumContains>).function("classify", &lua::Binding::native<&frustumClassify>).install();
}

void Geometry3Binding::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"plane", &lua::Binding::function<&haylen::math::Plane::fromNormal>}, {"planeFromPoint", &lua::Binding::function<&haylen::math::Plane::fromPoint>}, {"planeFromPoints", &lua::Binding::function<&haylen::math::Plane::fromPoints>}, {"ray3", &lua::Binding::function<&newRay>}, {"ray3Between", &lua::Binding::function<&haylen::math::Ray3::between>}, {"segment3", &lua::Binding::function<&newSegment>}, {"box3", &lua::Binding::function<&newBox>}, {"box3FromCenter", &lua::Binding::function<&haylen::math::Box3::fromCenter>}, {"box3Bounds", &lua::Binding::function<&boxBounds>}, {"sphere", &lua::Binding::function<&newSphere>}, {"obb", &lua::Binding::function<&newObb>}, {"obbFromTransform", &lua::Binding::function<&haylen::math::Obb::fromTransform>}, {"capsule3", &lua::Binding::function<&newCapsule>}, {"triangle3", &lua::Binding::function<&newTriangle>}, {"frustum", &lua::Binding::function<&newFrustum>}, {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

haylen::math::Segment3 Geometry3Binding::newSegment(haylen::math::Vec3 start, haylen::math::Vec3 end) {
    return {start, end};
}

haylen::math::Box3 Geometry3Binding::newBox(haylen::math::Vec3 minimum, haylen::math::Vec3 maximum) {
    return {minimum, maximum};
}

haylen::math::Sphere Geometry3Binding::newSphere(haylen::math::Vec3 center, float radius) {
    checkRadius(radius);
    return {center, radius};
}

haylen::math::Obb Geometry3Binding::newObb(haylen::math::Vec3 center, std::optional<haylen::math::Quat> rotation, haylen::math::Vec3 halfExtents) {
    checkExtents(halfExtents);
    return {center, rotation.value_or(haylen::math::Quat::identity()), halfExtents};
}

haylen::math::Capsule3 Geometry3Binding::newCapsule(haylen::math::Vec3 start, haylen::math::Vec3 end, float radius) {
    checkRadius(radius);
    return {start, end, radius};
}

haylen::math::Triangle3 Geometry3Binding::newTriangle(haylen::math::Vec3 a, haylen::math::Vec3 b, haylen::math::Vec3 c) {
    return {a, b, c};
}

} // namespace haylen::lua::math
