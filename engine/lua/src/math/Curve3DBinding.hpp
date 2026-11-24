#pragma once

#include <lua.hpp>

#include <array>
#include <span>
#include <string_view>

#include "haylen/lua/Type.hpp"
#include "haylen/math/CurveFrame.hpp"
#include "haylen/math/CurvePath3D.hpp"
#include "haylen/math/NurbsSurface.hpp"
#include "haylen/math/NurbsVolume.hpp"
#include "haylen/math/ParametricCurve3D.hpp"
#include "haylen/math/Spline3D.hpp"

namespace haylen::lua {

template <> struct Type<haylen::math::Spline3D> {
    static constexpr const char* name = "haylen.Spline3D";
    using Storage = haylen::math::Spline3D;
};

template <> struct Type<haylen::math::CurvePath3D> {
    static constexpr const char* name = "haylen.CurvePath3D";
    using Storage = haylen::math::CurvePath3D;
};

template <> struct Type<haylen::math::ParametricCurve3D> {
    static constexpr const char* name = "haylen.ParametricCurve3D";
    using Storage = haylen::math::ParametricCurve3D;
};

template <> struct Type<haylen::math::NurbsSurface> {
    static constexpr const char* name = "haylen.NurbsSurface";
    using Storage = haylen::math::NurbsSurface;
};

template <> struct Type<haylen::math::NurbsVolume> {
    static constexpr const char* name = "haylen.NurbsVolume";
    using Storage = haylen::math::NurbsVolume;
};

} // namespace haylen::lua

namespace haylen::lua::math {

// Installs the curves and NURBS shapes of 3D in `haylen.math`: the classes `Spline3D`, `CurvePath3D`, `ParametricCurve3D`, `NurbsSurface` and `NurbsVolume` with their constructors.
class Curve3DBinding final {
  public:
    static void install(lua_State* L);

    // Sets `spline3d`, `ellipse3d`, `arc3d`, `arcThrough3d`, `curvePath3d`, `helix`, `torusKnot`, `lissajous3d`, `nurbsSurface` and `nurbsVolume` on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 7> kSplineFields{"kind", "closed", "alpha", "tension", "weights", "knots", "degree"};
    static constexpr std::array<std::string_view, 1> kPathFields{"closed"};
    static constexpr std::array<std::string_view, 1> kFrameFields{"up"};
    static constexpr std::array<std::string_view, 2> kEllipseFields{"startAngle", "endAngle"};
    static constexpr std::array<std::string_view, 4> kHelixFields{"radius", "endRadius", "pitch", "turns"};
    static constexpr std::array<std::string_view, 4> kTorusKnotFields{"p", "q", "radius", "tubeRadius"};
    static constexpr std::array<std::string_view, 3> kLissajousFields{"amplitude", "frequency", "phase"};
    static constexpr std::array<std::string_view, 6> kSurfaceFields{"columns", "weights", "knotsU", "knotsV", "degreeU", "degreeV"};
    static constexpr std::array<std::string_view, 9> kVolumeFields{"columns", "rows", "weights", "knotsU", "knotsV", "knotsW", "degreeU", "degreeV", "degreeW"};

    static int newSpline(lua_State* L);
    static int newEllipse(lua_State* L);
    static int newArc(lua_State* L);
    static int newArcThrough(lua_State* L);
    static int newPath(lua_State* L);
    static int newHelix(lua_State* L);
    static int newTorusKnot(lua_State* L);
    static int newLissajous(lua_State* L);
    static int newSurface(lua_State* L);
    static int newVolume(lua_State* L);

    static int splinePoint(lua_State* L);
    static int splineDerivatives(lua_State* L);
    static int splineTangent(lua_State* L);
    static int splinePointAtDistance(lua_State* L);
    static int splineTangentAtDistance(lua_State* L);
    static int splineParameterAtDistance(lua_State* L);
    static int splineClosestPoint(lua_State* L);
    static int splineSample(lua_State* L);
    static int splineSampleEvenly(lua_State* L);
    static int splineSampleByDistance(lua_State* L);
    static int splineFrames(lua_State* L);
    static int splineLength(lua_State* L);
    static int splineKind(lua_State* L);
    static int splineClosed(lua_State* L);
    static int splinePoints(lua_State* L);
    static int splineSegmentCount(lua_State* L);
    static int splineDegree(lua_State* L);
    static int splineWeights(lua_State* L);
    static int splineKnots(lua_State* L);

    static int pathPoint(lua_State* L);
    static int pathTangent(lua_State* L);
    static int pathPointAtDistance(lua_State* L);
    static int pathTangentAtDistance(lua_State* L);
    static int pathClosestPoint(lua_State* L);
    static int pathSampleEvenly(lua_State* L);
    static int pathSampleByDistance(lua_State* L);
    static int pathFrames(lua_State* L);
    static int pathLength(lua_State* L);
    static int pathClosed(lua_State* L);
    static int pathCurveCount(lua_State* L);
    static int pathJoins(lua_State* L);

    static int curvePoint(lua_State* L);
    static int curveDerivatives(lua_State* L);
    static int curveTangent(lua_State* L);
    static int curvePointAtDistance(lua_State* L);
    static int curveTangentAtDistance(lua_State* L);
    static int curveParameterAtDistance(lua_State* L);
    static int curveClosestPoint(lua_State* L);
    static int curveSample(lua_State* L);
    static int curveSampleEvenly(lua_State* L);
    static int curveSampleByDistance(lua_State* L);
    static int curveFrames(lua_State* L);
    static int curveLength(lua_State* L);
    static int curveClosed(lua_State* L);

    static int surfacePoint(lua_State* L);
    static int surfaceDerivatives(lua_State* L);
    static int surfaceNormal(lua_State* L);
    static int surfaceColumns(lua_State* L);
    static int surfaceRows(lua_State* L);

    static int volumePoint(lua_State* L);
    static int volumeColumns(lua_State* L);
    static int volumeRows(lua_State* L);
    static int volumeLayers(lua_State* L);

    static void pushDerivatives(lua_State* L, const haylen::math::Spline3D::Derivatives& derivatives);
    static void pushClosest(lua_State* L, const haylen::math::Spline3D::ClosestPoint& closest);
    static void pushFrames(lua_State* L, std::span<const haylen::math::CurveFrame> frames);
    [[nodiscard]] static haylen::math::Vec3 readUp(lua_State* L, int index);
};

} // namespace haylen::lua::math
