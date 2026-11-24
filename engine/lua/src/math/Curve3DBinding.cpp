#include "math/Curve3DBinding.hpp"

#include <optional>
#include <string>
#include <vector>

#include "haylen/core/EnumNames.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"

namespace haylen::core {

template <> struct EnumNames<haylen::math::Spline3D::Kind> {
    static std::optional<haylen::math::Spline3D::Kind> fromName(std::string_view name) {
        return haylen::math::Spline3D::kindFromName(name);
    }
    static std::string_view name(haylen::math::Spline3D::Kind value) {
        return haylen::math::Spline3D::kindName(value);
    }
};

} // namespace haylen::core

namespace haylen::lua::math {

// Creates a spline with `spline3d(points, {kind = 'catmullRom', closed = false})`, where a Catmull-Rom spline also takes `alpha` and `tension` and a NURBS spline `weights`, `knots` and `degree`.
int Curve3DBinding::newSpline(lua_State* L) {
    auto points = lua::Stack::read<std::vector<haylen::math::Vec3>>(L, 1);
    haylen::math::Spline3D::Options options;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kSplineFields});
        lua::Table::readField(L, 2, "kind", options.kind);
        lua::Table::readField(L, 2, "closed", options.closed);
        lua::Table::readField(L, 2, "alpha", options.alpha);
        lua::Table::readField(L, 2, "tension", options.tension);
        lua::Table::readField(L, 2, "weights", options.weights);
        lua::Table::readField(L, 2, "knots", options.knots);
        lua::Table::readField(L, 2, "degree", options.degree);
    }

    lua::Userdata::emplace<haylen::math::Spline3D>(L, std::move(points), options);
    return 1;
}

// Creates a path with `curvePath3d(curves, {closed = false})` from a list of `Spline3D` values, which it copies.
int Curve3DBinding::newPath(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    std::vector<haylen::math::Spline3D> curves;
    const auto count = static_cast<lua_Integer>(lua_rawlen(L, 1));
    for (lua_Integer index = 1; index <= count; ++index) {
        lua_rawgeti(L, 1, index);
        curves.push_back(lua::Userdata::check<haylen::math::Spline3D>(L, lua_gettop(L)));
        lua_pop(L, 1);
    }

    bool closed = false;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kPathFields});
        lua::Table::readField(L, 2, "closed", closed);
    }

    lua::Userdata::emplace<haylen::math::CurvePath3D>(L, std::move(curves), closed);
    return 1;
}

// Creates a surface with `nurbsSurface(points, {columns = n})`, whose points go row after row.
int Curve3DBinding::newSurface(lua_State* L) {
    auto points = lua::Stack::read<std::vector<haylen::math::Vec3>>(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    lua::Table::checkFields(L, 2, {kSurfaceFields});
    haylen::math::NurbsSurface::Options options;
    lua::Table::readField(L, 2, "columns", options.columns);
    lua::Table::readField(L, 2, "weights", options.weights);
    lua::Table::readField(L, 2, "knotsU", options.knotsU);
    lua::Table::readField(L, 2, "knotsV", options.knotsV);
    lua::Table::readField(L, 2, "degreeU", options.degreeU);
    lua::Table::readField(L, 2, "degreeV", options.degreeV);
    lua::Userdata::emplace<haylen::math::NurbsSurface>(L, std::move(points), std::move(options));
    return 1;
}

// Creates a volume with `nurbsVolume(points, {columns = n, rows = m})`, whose points go layer after layer and row after row.
int Curve3DBinding::newVolume(lua_State* L) {
    auto points = lua::Stack::read<std::vector<haylen::math::Vec3>>(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    lua::Table::checkFields(L, 2, {kVolumeFields});
    haylen::math::NurbsVolume::Options options;
    lua::Table::readField(L, 2, "columns", options.columns);
    lua::Table::readField(L, 2, "rows", options.rows);
    lua::Table::readField(L, 2, "weights", options.weights);
    lua::Table::readField(L, 2, "knotsU", options.knotsU);
    lua::Table::readField(L, 2, "knotsV", options.knotsV);
    lua::Table::readField(L, 2, "knotsW", options.knotsW);
    lua::Table::readField(L, 2, "degreeU", options.degreeU);
    lua::Table::readField(L, 2, "degreeV", options.degreeV);
    lua::Table::readField(L, 2, "degreeW", options.degreeW);
    lua::Userdata::emplace<haylen::math::NurbsVolume>(L, std::move(points), std::move(options));
    return 1;
}

int Curve3DBinding::splinePoint(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline3D>(L, 1).getPoint(lua::Stack::read<float>(L, 2)));
    return 1;
}

int Curve3DBinding::splineDerivatives(lua_State* L) {
    const haylen::math::Spline3D::Derivatives result = lua::Userdata::check<haylen::math::Spline3D>(L, 1).getDerivatives(lua::Stack::read<float>(L, 2));
    lua_createtable(L, 0, 3);
    lua::Stack::push(L, result.point);
    lua_setfield(L, -2, "point");
    lua::Stack::push(L, result.first);
    lua_setfield(L, -2, "first");
    lua::Stack::push(L, result.second);
    lua_setfield(L, -2, "second");
    return 1;
}

int Curve3DBinding::splineTangent(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline3D>(L, 1).getTangent(lua::Stack::read<float>(L, 2)));
    return 1;
}

int Curve3DBinding::splinePointAtDistance(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline3D>(L, 1).getPointAtDistance(lua::Stack::read<float>(L, 2)));
    return 1;
}

int Curve3DBinding::splineTangentAtDistance(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline3D>(L, 1).getTangentAtDistance(lua::Stack::read<float>(L, 2)));
    return 1;
}

int Curve3DBinding::splineParameterAtDistance(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline3D>(L, 1).getParameterAtDistance(lua::Stack::read<float>(L, 2)));
    return 1;
}

int Curve3DBinding::splineClosestPoint(lua_State* L) {
    pushClosest(L, lua::Userdata::check<haylen::math::Spline3D>(L, 1).getClosestPoint(lua::Stack::read<haylen::math::Vec3>(L, 2)));
    return 1;
}

int Curve3DBinding::splineSample(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline3D>(L, 1).sample(lua::Stack::read<std::size_t>(L, 2)));
    return 1;
}

int Curve3DBinding::splineSampleEvenly(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline3D>(L, 1).sampleEvenly(lua::Stack::read<std::size_t>(L, 2)));
    return 1;
}

int Curve3DBinding::splineSampleByDistance(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline3D>(L, 1).sampleByDistance(lua::Stack::read<float>(L, 2)));
    return 1;
}

int Curve3DBinding::splineFrames(lua_State* L) {
    const haylen::math::Spline3D& spline = lua::Userdata::check<haylen::math::Spline3D>(L, 1);
    const std::vector<haylen::math::CurveFrame> frames = spline.getFrames(lua::Stack::read<std::size_t>(L, 2), readUp(L, 3));
    pushFrames(L, frames);
    return 1;
}

int Curve3DBinding::splineLength(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline3D>(L, 1).getLength());
    return 1;
}

int Curve3DBinding::splineKind(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline3D>(L, 1).getKind());
    return 1;
}

int Curve3DBinding::splineClosed(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline3D>(L, 1).isClosed());
    return 1;
}

int Curve3DBinding::splinePoints(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline3D>(L, 1).getPoints());
    return 1;
}

int Curve3DBinding::splineSegmentCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline3D>(L, 1).getSegmentCount());
    return 1;
}

int Curve3DBinding::splineDegree(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline3D>(L, 1).getDegree());
    return 1;
}

int Curve3DBinding::splineWeights(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline3D>(L, 1).getWeights());
    return 1;
}

int Curve3DBinding::splineKnots(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline3D>(L, 1).getKnots());
    return 1;
}

int Curve3DBinding::pathPoint(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::CurvePath3D>(L, 1).getPoint(lua::Stack::read<float>(L, 2)));
    return 1;
}

int Curve3DBinding::pathTangent(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::CurvePath3D>(L, 1).getTangent(lua::Stack::read<float>(L, 2)));
    return 1;
}

int Curve3DBinding::pathPointAtDistance(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::CurvePath3D>(L, 1).getPointAtDistance(lua::Stack::read<float>(L, 2)));
    return 1;
}

int Curve3DBinding::pathTangentAtDistance(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::CurvePath3D>(L, 1).getTangentAtDistance(lua::Stack::read<float>(L, 2)));
    return 1;
}

int Curve3DBinding::pathClosestPoint(lua_State* L) {
    pushClosest(L, lua::Userdata::check<haylen::math::CurvePath3D>(L, 1).getClosestPoint(lua::Stack::read<haylen::math::Vec3>(L, 2)));
    return 1;
}

int Curve3DBinding::pathSampleEvenly(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::CurvePath3D>(L, 1).sampleEvenly(lua::Stack::read<std::size_t>(L, 2)));
    return 1;
}

int Curve3DBinding::pathSampleByDistance(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::CurvePath3D>(L, 1).sampleByDistance(lua::Stack::read<float>(L, 2)));
    return 1;
}

int Curve3DBinding::pathFrames(lua_State* L) {
    const haylen::math::CurvePath3D& path = lua::Userdata::check<haylen::math::CurvePath3D>(L, 1);
    const std::vector<haylen::math::CurveFrame> frames = path.getFrames(lua::Stack::read<std::size_t>(L, 2), readUp(L, 3));
    pushFrames(L, frames);
    return 1;
}

int Curve3DBinding::pathLength(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::CurvePath3D>(L, 1).getLength());
    return 1;
}

int Curve3DBinding::pathClosed(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::CurvePath3D>(L, 1).isClosed());
    return 1;
}

int Curve3DBinding::pathCurveCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::CurvePath3D>(L, 1).getCurves().size());
    return 1;
}

int Curve3DBinding::pathJoins(lua_State* L) {
    const std::vector<haylen::math::CurvePath3D::Continuity>& joins = lua::Userdata::check<haylen::math::CurvePath3D>(L, 1).getJoins();
    lua_createtable(L, static_cast<int>(joins.size()), 0);
    for (std::size_t index = 0; index < joins.size(); ++index) {
        const std::string_view name = haylen::math::CurvePath3D::continuityName(joins[index]);
        lua_pushlstring(L, name.data(), name.size());
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    return 1;
}

int Curve3DBinding::surfacePoint(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::NurbsSurface>(L, 1).getPoint(lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)));
    return 1;
}

int Curve3DBinding::surfaceDerivatives(lua_State* L) {
    const haylen::math::NurbsSurface::Derivatives result = lua::Userdata::check<haylen::math::NurbsSurface>(L, 1).getDerivatives(lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3));
    lua_createtable(L, 0, 3);
    lua::Stack::push(L, result.point);
    lua_setfield(L, -2, "point");
    lua::Stack::push(L, result.alongU);
    lua_setfield(L, -2, "alongU");
    lua::Stack::push(L, result.alongV);
    lua_setfield(L, -2, "alongV");
    return 1;
}

int Curve3DBinding::surfaceNormal(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::NurbsSurface>(L, 1).getNormal(lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)));
    return 1;
}

int Curve3DBinding::surfaceColumns(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::NurbsSurface>(L, 1).getColumns());
    return 1;
}

int Curve3DBinding::surfaceRows(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::NurbsSurface>(L, 1).getRows());
    return 1;
}

int Curve3DBinding::volumePoint(lua_State* L) {
    const haylen::math::NurbsVolume& volume = lua::Userdata::check<haylen::math::NurbsVolume>(L, 1);
    lua::Stack::push(L, volume.getPoint(lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4)));
    return 1;
}

int Curve3DBinding::volumeColumns(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::NurbsVolume>(L, 1).getColumns());
    return 1;
}

int Curve3DBinding::volumeRows(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::NurbsVolume>(L, 1).getRows());
    return 1;
}

int Curve3DBinding::volumeLayers(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::NurbsVolume>(L, 1).getLayers());
    return 1;
}

void Curve3DBinding::pushClosest(lua_State* L, const haylen::math::Spline3D::ClosestPoint& closest) {
    lua_createtable(L, 0, 3);
    lua::Stack::push(L, closest.t);
    lua_setfield(L, -2, "t");
    lua::Stack::push(L, closest.point);
    lua_setfield(L, -2, "point");
    lua::Stack::push(L, closest.distance);
    lua_setfield(L, -2, "distance");
}

// Pushes the frames as a list of tables with `position`, `tangent`, `normal` and `binormal`.
void Curve3DBinding::pushFrames(lua_State* L, std::span<const haylen::math::CurveFrame> frames) {
    lua_createtable(L, static_cast<int>(frames.size()), 0);
    for (std::size_t index = 0; index < frames.size(); ++index) {
        const haylen::math::CurveFrame& frame = frames[index];
        lua_createtable(L, 0, 4);
        lua::Stack::push(L, frame.position);
        lua_setfield(L, -2, "position");
        lua::Stack::push(L, frame.tangent);
        lua_setfield(L, -2, "tangent");
        lua::Stack::push(L, frame.normal);
        lua_setfield(L, -2, "normal");
        lua::Stack::push(L, frame.binormal);
        lua_setfield(L, -2, "binormal");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
}

// Reads `{up = vector}`, the up of the world when it is left out.
haylen::math::Vec3 Curve3DBinding::readUp(lua_State* L, int index) {
    haylen::math::Vec3 up = haylen::math::Vec3::up();
    if (!lua_isnoneornil(L, index)) {
        luaL_checktype(L, index, LUA_TTABLE);
        lua::Table::checkFields(L, index, {kFrameFields});
        lua::Table::readField(L, index, "up", up);
    }

    return up;
}

void Curve3DBinding::install(lua_State* L) {
    lua::ClassBuilder<haylen::math::Spline3D>(L).function("point", &lua::Binding::native<&splinePoint>).function("derivatives", &lua::Binding::native<&splineDerivatives>).function("tangent", &lua::Binding::native<&splineTangent>).function("pointAtDistance", &lua::Binding::native<&splinePointAtDistance>).function("tangentAtDistance", &lua::Binding::native<&splineTangentAtDistance>).function("parameterAtDistance", &lua::Binding::native<&splineParameterAtDistance>).function("closestPoint", &lua::Binding::native<&splineClosestPoint>).function("sample", &lua::Binding::native<&splineSample>).function("sampleEvenly", &lua::Binding::native<&splineSampleEvenly>).function("sampleByDistance", &lua::Binding::native<&splineSampleByDistance>).function("frames", &lua::Binding::native<&splineFrames>).property("length", &splineLength).property("kind", &splineKind).property("closed", &splineClosed).property("points", &splinePoints).property("segmentCount", &splineSegmentCount).property("degree", &splineDegree).property("weights", &splineWeights).property("knots", &splineKnots).install();

    lua::ClassBuilder<haylen::math::CurvePath3D>(L).function("point", &lua::Binding::native<&pathPoint>).function("tangent", &lua::Binding::native<&pathTangent>).function("pointAtDistance", &lua::Binding::native<&pathPointAtDistance>).function("tangentAtDistance", &lua::Binding::native<&pathTangentAtDistance>).function("closestPoint", &lua::Binding::native<&pathClosestPoint>).function("sampleEvenly", &lua::Binding::native<&pathSampleEvenly>).function("sampleByDistance", &lua::Binding::native<&pathSampleByDistance>).function("frames", &lua::Binding::native<&pathFrames>).property("length", &pathLength).property("closed", &pathClosed).property("curveCount", &pathCurveCount).property("joins", &pathJoins).install();

    lua::ClassBuilder<haylen::math::NurbsSurface>(L).function("point", &lua::Binding::native<&surfacePoint>).function("derivatives", &lua::Binding::native<&surfaceDerivatives>).function("normal", &lua::Binding::native<&surfaceNormal>).property("columns", &surfaceColumns).property("rows", &surfaceRows).install();

    lua::ClassBuilder<haylen::math::NurbsVolume>(L).function("point", &lua::Binding::native<&volumePoint>).property("columns", &volumeColumns).property("rows", &volumeRows).property("layers", &volumeLayers).install();
}

void Curve3DBinding::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"spline3d", &lua::Binding::native<&newSpline>}, {"curvePath3d", &lua::Binding::native<&newPath>}, {"nurbsSurface", &lua::Binding::native<&newSurface>}, {"nurbsVolume", &lua::Binding::native<&newVolume>}, {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

} // namespace haylen::lua::math
