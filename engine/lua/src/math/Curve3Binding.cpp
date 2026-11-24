#include "math/Curve3Binding.hpp"

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

template <> struct EnumNames<haylen::math::Spline3::Kind> {
    static std::optional<haylen::math::Spline3::Kind> fromName(std::string_view name) {
        return haylen::math::Spline3::kindFromName(name);
    }
    static std::string_view name(haylen::math::Spline3::Kind value) {
        return haylen::math::Spline3::kindName(value);
    }
};

} // namespace haylen::core

namespace haylen::lua::math {

// Creates a spline with `spline3(points, {kind = 'catmullRom', closed = false})`, where a Catmull-Rom spline also takes `alpha` and `tension` and a NURBS spline `weights`, `knots` and `degree`.
int Curve3Binding::newSpline(lua_State* L) {
    auto points = lua::Stack::read<std::vector<haylen::math::Vec3>>(L, 1);
    haylen::math::Spline3::Options options;
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

    lua::Userdata::emplace<haylen::math::Spline3>(L, std::move(points), options);
    return 1;
}

// Creates a path with `curvePath3(curves, {closed = false})` from a list of `Spline3` values, which it copies.
int Curve3Binding::newPath(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    std::vector<haylen::math::Spline3> curves;
    const auto count = static_cast<lua_Integer>(lua_rawlen(L, 1));
    for (lua_Integer index = 1; index <= count; ++index) {
        lua_rawgeti(L, 1, index);
        curves.push_back(lua::Userdata::check<haylen::math::Spline3>(L, lua_gettop(L)));
        lua_pop(L, 1);
    }

    bool closed = false;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kPathFields});
        lua::Table::readField(L, 2, "closed", closed);
    }

    lua::Userdata::emplace<haylen::math::CurvePath3>(L, std::move(curves), closed);
    return 1;
}

// Creates a surface with `nurbsSurface(points, {columns = n})`, whose points go row after row.
int Curve3Binding::newSurface(lua_State* L) {
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
int Curve3Binding::newVolume(lua_State* L) {
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

int Curve3Binding::splinePoint(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline3>(L, 1).getPoint(lua::Stack::read<float>(L, 2)));
    return 1;
}

int Curve3Binding::splineDerivatives(lua_State* L) {
    const haylen::math::Spline3::Derivatives result = lua::Userdata::check<haylen::math::Spline3>(L, 1).getDerivatives(lua::Stack::read<float>(L, 2));
    lua_createtable(L, 0, 3);
    lua::Stack::push(L, result.point);
    lua_setfield(L, -2, "point");
    lua::Stack::push(L, result.first);
    lua_setfield(L, -2, "first");
    lua::Stack::push(L, result.second);
    lua_setfield(L, -2, "second");
    return 1;
}

int Curve3Binding::splineTangent(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline3>(L, 1).getTangent(lua::Stack::read<float>(L, 2)));
    return 1;
}

int Curve3Binding::splinePointAtDistance(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline3>(L, 1).getPointAtDistance(lua::Stack::read<float>(L, 2)));
    return 1;
}

int Curve3Binding::splineTangentAtDistance(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline3>(L, 1).getTangentAtDistance(lua::Stack::read<float>(L, 2)));
    return 1;
}

int Curve3Binding::splineParameterAtDistance(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline3>(L, 1).getParameterAtDistance(lua::Stack::read<float>(L, 2)));
    return 1;
}

int Curve3Binding::splineClosestPoint(lua_State* L) {
    pushClosest(L, lua::Userdata::check<haylen::math::Spline3>(L, 1).getClosestPoint(lua::Stack::read<haylen::math::Vec3>(L, 2)));
    return 1;
}

int Curve3Binding::splineSample(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline3>(L, 1).sample(lua::Stack::read<std::size_t>(L, 2)));
    return 1;
}

int Curve3Binding::splineSampleEvenly(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline3>(L, 1).sampleEvenly(lua::Stack::read<std::size_t>(L, 2)));
    return 1;
}

int Curve3Binding::splineSampleByDistance(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline3>(L, 1).sampleByDistance(lua::Stack::read<float>(L, 2)));
    return 1;
}

int Curve3Binding::splineFrames(lua_State* L) {
    const haylen::math::Spline3& spline = lua::Userdata::check<haylen::math::Spline3>(L, 1);
    const std::vector<haylen::math::CurveFrame> frames = spline.getFrames(lua::Stack::read<std::size_t>(L, 2), readUp(L, 3));
    pushFrames(L, frames);
    return 1;
}

int Curve3Binding::splineLength(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline3>(L, 1).getLength());
    return 1;
}

int Curve3Binding::splineKind(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline3>(L, 1).getKind());
    return 1;
}

int Curve3Binding::splineClosed(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline3>(L, 1).isClosed());
    return 1;
}

int Curve3Binding::splinePoints(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline3>(L, 1).getPoints());
    return 1;
}

int Curve3Binding::splineSegmentCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline3>(L, 1).getSegmentCount());
    return 1;
}

int Curve3Binding::splineDegree(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline3>(L, 1).getDegree());
    return 1;
}

int Curve3Binding::splineWeights(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline3>(L, 1).getWeights());
    return 1;
}

int Curve3Binding::splineKnots(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline3>(L, 1).getKnots());
    return 1;
}

int Curve3Binding::pathPoint(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::CurvePath3>(L, 1).getPoint(lua::Stack::read<float>(L, 2)));
    return 1;
}

int Curve3Binding::pathTangent(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::CurvePath3>(L, 1).getTangent(lua::Stack::read<float>(L, 2)));
    return 1;
}

int Curve3Binding::pathPointAtDistance(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::CurvePath3>(L, 1).getPointAtDistance(lua::Stack::read<float>(L, 2)));
    return 1;
}

int Curve3Binding::pathTangentAtDistance(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::CurvePath3>(L, 1).getTangentAtDistance(lua::Stack::read<float>(L, 2)));
    return 1;
}

int Curve3Binding::pathClosestPoint(lua_State* L) {
    pushClosest(L, lua::Userdata::check<haylen::math::CurvePath3>(L, 1).getClosestPoint(lua::Stack::read<haylen::math::Vec3>(L, 2)));
    return 1;
}

int Curve3Binding::pathSampleEvenly(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::CurvePath3>(L, 1).sampleEvenly(lua::Stack::read<std::size_t>(L, 2)));
    return 1;
}

int Curve3Binding::pathSampleByDistance(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::CurvePath3>(L, 1).sampleByDistance(lua::Stack::read<float>(L, 2)));
    return 1;
}

int Curve3Binding::pathFrames(lua_State* L) {
    const haylen::math::CurvePath3& path = lua::Userdata::check<haylen::math::CurvePath3>(L, 1);
    const std::vector<haylen::math::CurveFrame> frames = path.getFrames(lua::Stack::read<std::size_t>(L, 2), readUp(L, 3));
    pushFrames(L, frames);
    return 1;
}

int Curve3Binding::pathLength(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::CurvePath3>(L, 1).getLength());
    return 1;
}

int Curve3Binding::pathClosed(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::CurvePath3>(L, 1).isClosed());
    return 1;
}

int Curve3Binding::pathCurveCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::CurvePath3>(L, 1).getCurves().size());
    return 1;
}

int Curve3Binding::pathJoins(lua_State* L) {
    const std::vector<haylen::math::CurvePath3::Continuity>& joins = lua::Userdata::check<haylen::math::CurvePath3>(L, 1).getJoins();
    lua_createtable(L, static_cast<int>(joins.size()), 0);
    for (std::size_t index = 0; index < joins.size(); ++index) {
        const std::string_view name = haylen::math::CurvePath3::continuityName(joins[index]);
        lua_pushlstring(L, name.data(), name.size());
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    return 1;
}

int Curve3Binding::surfacePoint(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::NurbsSurface>(L, 1).getPoint(lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)));
    return 1;
}

int Curve3Binding::surfaceDerivatives(lua_State* L) {
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

int Curve3Binding::surfaceNormal(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::NurbsSurface>(L, 1).getNormal(lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)));
    return 1;
}

int Curve3Binding::surfaceColumns(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::NurbsSurface>(L, 1).getColumns());
    return 1;
}

int Curve3Binding::surfaceRows(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::NurbsSurface>(L, 1).getRows());
    return 1;
}

int Curve3Binding::volumePoint(lua_State* L) {
    const haylen::math::NurbsVolume& volume = lua::Userdata::check<haylen::math::NurbsVolume>(L, 1);
    lua::Stack::push(L, volume.getPoint(lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4)));
    return 1;
}

int Curve3Binding::volumeColumns(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::NurbsVolume>(L, 1).getColumns());
    return 1;
}

int Curve3Binding::volumeRows(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::NurbsVolume>(L, 1).getRows());
    return 1;
}

int Curve3Binding::volumeLayers(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::NurbsVolume>(L, 1).getLayers());
    return 1;
}

void Curve3Binding::pushClosest(lua_State* L, const haylen::math::Spline3::ClosestPoint& closest) {
    lua_createtable(L, 0, 3);
    lua::Stack::push(L, closest.t);
    lua_setfield(L, -2, "t");
    lua::Stack::push(L, closest.point);
    lua_setfield(L, -2, "point");
    lua::Stack::push(L, closest.distance);
    lua_setfield(L, -2, "distance");
}

// Pushes the frames as a list of tables with `position`, `tangent`, `normal` and `binormal`.
void Curve3Binding::pushFrames(lua_State* L, std::span<const haylen::math::CurveFrame> frames) {
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
haylen::math::Vec3 Curve3Binding::readUp(lua_State* L, int index) {
    haylen::math::Vec3 up = haylen::math::Vec3::up();
    if (!lua_isnoneornil(L, index)) {
        luaL_checktype(L, index, LUA_TTABLE);
        lua::Table::checkFields(L, index, {kFrameFields});
        lua::Table::readField(L, index, "up", up);
    }

    return up;
}

void Curve3Binding::install(lua_State* L) {
    lua::ClassBuilder<haylen::math::Spline3>(L).function("point", &lua::Binding::native<&splinePoint>).function("derivatives", &lua::Binding::native<&splineDerivatives>).function("tangent", &lua::Binding::native<&splineTangent>).function("pointAtDistance", &lua::Binding::native<&splinePointAtDistance>).function("tangentAtDistance", &lua::Binding::native<&splineTangentAtDistance>).function("parameterAtDistance", &lua::Binding::native<&splineParameterAtDistance>).function("closestPoint", &lua::Binding::native<&splineClosestPoint>).function("sample", &lua::Binding::native<&splineSample>).function("sampleEvenly", &lua::Binding::native<&splineSampleEvenly>).function("sampleByDistance", &lua::Binding::native<&splineSampleByDistance>).function("frames", &lua::Binding::native<&splineFrames>).property("length", &splineLength).property("kind", &splineKind).property("closed", &splineClosed).property("points", &splinePoints).property("segmentCount", &splineSegmentCount).property("degree", &splineDegree).property("weights", &splineWeights).property("knots", &splineKnots).install();

    lua::ClassBuilder<haylen::math::CurvePath3>(L).function("point", &lua::Binding::native<&pathPoint>).function("tangent", &lua::Binding::native<&pathTangent>).function("pointAtDistance", &lua::Binding::native<&pathPointAtDistance>).function("tangentAtDistance", &lua::Binding::native<&pathTangentAtDistance>).function("closestPoint", &lua::Binding::native<&pathClosestPoint>).function("sampleEvenly", &lua::Binding::native<&pathSampleEvenly>).function("sampleByDistance", &lua::Binding::native<&pathSampleByDistance>).function("frames", &lua::Binding::native<&pathFrames>).property("length", &pathLength).property("closed", &pathClosed).property("curveCount", &pathCurveCount).property("joins", &pathJoins).install();

    lua::ClassBuilder<haylen::math::NurbsSurface>(L).function("point", &lua::Binding::native<&surfacePoint>).function("derivatives", &lua::Binding::native<&surfaceDerivatives>).function("normal", &lua::Binding::native<&surfaceNormal>).property("columns", &surfaceColumns).property("rows", &surfaceRows).install();

    lua::ClassBuilder<haylen::math::NurbsVolume>(L).function("point", &lua::Binding::native<&volumePoint>).property("columns", &volumeColumns).property("rows", &volumeRows).property("layers", &volumeLayers).install();
}

void Curve3Binding::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"spline3", &lua::Binding::native<&newSpline>}, {"curvePath3", &lua::Binding::native<&newPath>}, {"nurbsSurface", &lua::Binding::native<&newSurface>}, {"nurbsVolume", &lua::Binding::native<&newVolume>}, {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

} // namespace haylen::lua::math
