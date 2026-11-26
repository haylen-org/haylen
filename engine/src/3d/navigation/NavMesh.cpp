#include "haylen/3d/navigation/NavMesh.hpp"

#include <DetourCommon.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>

#include <array>
#include <cfloat>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "3d/navigation/AreaFilter.hpp"
#include "3d/navigation/DetourVector.hpp"
#include "3d/navigation/NavMeshFile.hpp"
#include "3d/navigation/NavMeshState.hpp"
#include "3d/navigation/RandomPoints.hpp"
#include "3d/navigation/TileAssembler.hpp"
#include "haylen/content/Error.hpp"

namespace haylen::navigation3d {

const NavMesh::QueryOptions NavMesh::kDefaultQuery{};

NavMesh::NavMesh(std::unique_ptr<State> value) : state(std::move(value)) {}

NavMesh::NavMesh(NavMesh&& other) noexcept = default;
NavMesh& NavMesh::operator=(NavMesh&& other) noexcept = default;
NavMesh::~NavMesh() = default;

NavMesh NavMesh::build(const SourceGeometry& source, const BuildSettings& settings) {
    return NavMesh(State::bake(source, settings));
}

void NavMesh::buildAsync(core::JobSystem& jobs, std::shared_ptr<const SourceGeometry> source, const BuildSettings& settings, BuildCallback callback) {
    State::bakeAsync(jobs, std::move(source), settings, std::move(callback));
}

std::vector<std::uint8_t> NavMesh::encode() const {
    return NavMeshFile::encode(state->getContents());
}

NavMesh NavMesh::decode(std::span<const std::uint8_t> bytes, std::string_view file) {
    const NavMeshFile::Contents contents = NavMeshFile::decode(bytes, file);
    try {
        return NavMesh(State::make(contents));
    } catch (const TileAssembler::DamagedLayer&) {
        throw content::Error(content::Error::Code::UnsupportedFormat, NavMeshFile::describeDamage(file));
    }
}

const BuildSettings& NavMesh::getSettings() const noexcept {
    return state->settings;
}

const math::Box& NavMesh::getBounds() const noexcept {
    return state->grid.bounds;
}

std::size_t NavMesh::getTileCount() const noexcept {
    std::size_t count = 0;
    for (const State::Column& column : state->columns) {
        count += column.tiles.size();
    }

    return count;
}

std::size_t NavMesh::getPolygonCount() const noexcept {
    std::size_t count = 0;
    const dtNavMesh& detour = *state->mesh;
    for (int index = 0; index < detour.getMaxTiles(); ++index) {
        const dtMeshTile* tile = detour.getTile(index);
        if (tile->header != nullptr) {
            count += static_cast<std::size_t>(tile->header->polyCount - tile->header->offMeshConCount);
        }
    }

    return count;
}

const std::vector<std::string>& NavMesh::getAreas() const noexcept {
    return state->areas;
}

std::uint8_t NavMesh::getArea(std::string_view name) const {
    return state->getArea(name);
}

std::optional<NavMesh::Location> NavMesh::findNearest(math::Vec3 point, const QueryOptions& options) const {
    dtNavMeshQuery& query = state->getQuery(options.maxNodes);
    const AreaFilter filter(options.filter != nullptr ? *options.filter : QueryFilter{});
    const std::array<float, 3> center = DetourVector::of(point);
    const std::array<float, 3> extents = DetourVector::of(options.searchExtents);
    std::array<float, 3> nearest{};
    dtPolyRef polygon = 0;
    if (dtStatusFailed(query.findNearestPoly(center.data(), extents.data(), &filter, &polygon, nearest.data())) || polygon == 0) {
        return std::nullopt;
    }

    // The search of Detour gathers polygons by bounds a voxel coarser than the extents, so the point it finds must still lie within them.
    const math::Vec3 offset = DetourVector::toVec3(nearest.data()) - point;
    if (std::fabs(offset.x) > options.searchExtents.x || std::fabs(offset.y) > options.searchExtents.y || std::fabs(offset.z) > options.searchExtents.z) {
        return std::nullopt;
    }

    return state->locate(polygon, nearest);
}

std::optional<NavMesh::Path> NavMesh::findPath(math::Vec3 start, math::Vec3 goal, const QueryOptions& options) const {
    if (options.maxPolygons == 0 || options.maxPoints < 2) {
        throw std::invalid_argument("A path search keeps at least 1 polygon and 2 points.");
    }

    const std::optional<Location> from = findNearest(start, options);
    const std::optional<Location> to = findNearest(goal, options);
    if (!from || !to) {
        return std::nullopt;
    }

    dtNavMeshQuery& query = state->getQuery(options.maxNodes);
    const AreaFilter filter(options.filter != nullptr ? *options.filter : QueryFilter{});
    const std::array<float, 3> startPoint = DetourVector::of(from->position);
    std::array<float, 3> endPoint = DetourVector::of(to->position);
    state->polygons.resize(options.maxPolygons);
    int polygonCount = 0;

    // The search costs a step between polygons by the middles of their edges, which bends paths on wide polygons, so it also casts rays from the polygons it reaches and takes the straight way where one is clear.
    if (dtStatusFailed(query.initSlicedFindPath(from->polygon, to->polygon, startPoint.data(), endPoint.data(), &filter, DT_FINDPATH_ANY_ANGLE))) {
        return std::nullopt;
    }
    const dtStatus searched = query.updateSlicedFindPath(static_cast<int>(options.maxNodes), nullptr);
    const dtStatus status = query.finalizeSlicedFindPath(state->polygons.data(), &polygonCount, static_cast<int>(options.maxPolygons));
    if (dtStatusFailed(status) || polygonCount == 0) {
        return std::nullopt;
    }

    // A corridor that ends elsewhere than the goal ends at the point of its last polygon closest to the goal.
    Path path;
    const dtPolyRef last = state->polygons[static_cast<std::size_t>(polygonCount) - 1];
    path.partial = last != to->polygon || dtStatusInProgress(searched) || dtStatusDetail(status, DT_PARTIAL_RESULT) || dtStatusDetail(status, DT_OUT_OF_NODES) || dtStatusDetail(status, DT_BUFFER_TOO_SMALL);
    if (last != to->polygon) {
        query.closestPointOnPoly(last, endPoint.data(), endPoint.data(), nullptr);
    }

    state->points.resize(options.maxPoints * 3);
    state->pointFlags.resize(options.maxPoints);
    state->pointPolygons.resize(options.maxPoints);
    int pointCount = 0;
    const dtStatus straight = query.findStraightPath(startPoint.data(), endPoint.data(), state->polygons.data(), polygonCount, state->points.data(), state->pointFlags.data(), state->pointPolygons.data(), &pointCount, static_cast<int>(options.maxPoints));
    if (dtStatusFailed(straight)) {
        return std::nullopt;
    }
    if (dtStatusDetail(straight, DT_BUFFER_TOO_SMALL)) {
        path.partial = true;
    }

    path.points.reserve(static_cast<std::size_t>(pointCount));
    path.links.reserve(static_cast<std::size_t>(pointCount));
    for (int index = 0; index < pointCount; ++index) {
        const auto item = static_cast<std::size_t>(index);
        path.points.push_back(DetourVector::toVec3(&state->points[item * 3]));
        std::uint32_t link = 0;
        if ((state->pointFlags[item] & DT_STRAIGHTPATH_OFFMESH_CONNECTION) != 0) {
            const dtOffMeshConnection* connection = state->mesh->getOffMeshConnectionByRef(state->pointPolygons[item]);
            link = connection != nullptr ? connection->userId : 0;
        }
        path.links.push_back(link);
        if (index > 0) {
            path.length += math::Vec3::distance(path.points[item - 1], path.points[item]);
        }
    }

    return path;
}

std::optional<NavMesh::RayHit> NavMesh::raycast(math::Vec3 start, math::Vec3 end, const QueryOptions& options) const {
    const std::optional<Location> from = findNearest(start, options);
    if (!from) {
        return std::nullopt;
    }

    dtNavMeshQuery& query = state->getQuery(options.maxNodes);
    const AreaFilter filter(options.filter != nullptr ? *options.filter : QueryFilter{});
    const std::array<float, 3> startPoint = DetourVector::of(from->position);
    const std::array<float, 3> endPoint = DetourVector::of(end);
    state->polygons.resize(options.maxPolygons);
    dtRaycastHit hit{};
    hit.path = state->polygons.data();
    hit.maxPath = static_cast<int>(options.maxPolygons);
    if (dtStatusFailed(query.raycast(from->polygon, startPoint.data(), endPoint.data(), &filter, 0, &hit))) {
        return std::nullopt;
    }

    RayHit result;
    result.hit = hit.t != FLT_MAX;
    result.fraction = result.hit ? hit.t : 1.0F;
    result.position = math::Vec3::lerp(from->position, end, result.fraction);
    if (result.hit) {
        result.normal = DetourVector::toVec3(hit.hitNormal);
    }

    // The ray runs on the ground, so its stop takes the height of the polygon it stopped in.
    if (hit.pathCount > 0) {
        std::array<float, 3> point = DetourVector::of(result.position);
        float height = 0.0F;
        if (dtStatusSucceed(query.getPolyHeight(hit.path[hit.pathCount - 1], point.data(), &height))) {
            result.position.y = height;
        }
    }

    return result;
}

std::optional<NavMesh::Location> NavMesh::moveAlongSurface(math::Vec3 start, math::Vec3 end, const QueryOptions& options) const {
    const std::optional<Location> from = findNearest(start, options);
    if (!from) {
        return std::nullopt;
    }

    dtNavMeshQuery& query = state->getQuery(options.maxNodes);
    const AreaFilter filter(options.filter != nullptr ? *options.filter : QueryFilter{});
    const std::array<float, 3> startPoint = DetourVector::of(from->position);
    const std::array<float, 3> endPoint = DetourVector::of(end);
    std::array<float, 3> reached{};
    state->polygons.resize(options.maxPolygons);
    int visited = 0;
    if (dtStatusFailed(query.moveAlongSurface(from->polygon, startPoint.data(), endPoint.data(), &filter, reached.data(), state->polygons.data(), &visited, static_cast<int>(options.maxPolygons))) || visited == 0) {
        return std::nullopt;
    }

    const dtPolyRef polygon = state->polygons[static_cast<std::size_t>(visited) - 1];
    float height = reached[1];
    if (dtStatusSucceed(query.getPolyHeight(polygon, reached.data(), &height))) {
        reached[1] = height;
    }

    return state->locate(polygon, reached);
}

std::optional<NavMesh::WallHit> NavMesh::findDistanceToWall(math::Vec3 point, float radius, const QueryOptions& options) const {
    if (!std::isfinite(radius) || radius <= 0.0F) {
        throw std::invalid_argument("The radius of a search for walls must be a finite number of meters above zero.");
    }

    const std::optional<Location> from = findNearest(point, options);
    if (!from) {
        return std::nullopt;
    }

    dtNavMeshQuery& query = state->getQuery(options.maxNodes);
    const AreaFilter filter(options.filter != nullptr ? *options.filter : QueryFilter{});
    const std::array<float, 3> center = DetourVector::of(from->position);
    float distance = 0.0F;
    std::array<float, 3> position{};
    std::array<float, 3> normal{};
    if (dtStatusFailed(query.findDistanceToWall(from->polygon, center.data(), radius, &filter, &distance, position.data(), normal.data())) || distance >= radius) {
        return std::nullopt;
    }

    return WallHit{.distance = distance, .position = DetourVector::toVec3(position.data()), .normal = DetourVector::toVec3(normal.data())};
}

std::optional<NavMesh::Location> NavMesh::findRandomPoint(math::Random& random, const QueryOptions& options) const {
    const dtNavMeshQuery& query = state->getQuery(options.maxNodes);
    const AreaFilter filter(options.filter != nullptr ? *options.filter : QueryFilter{});
    const std::optional<RandomPoints::Point> point = RandomPoints::onMesh(*state->mesh, query, filter, random);
    if (!point) {
        return std::nullopt;
    }

    return state->locate(point->polygon, point->position);
}

std::optional<NavMesh::Location> NavMesh::findRandomPointAround(math::Vec3 center, float radius, math::Random& random, const QueryOptions& options) const {
    if (!std::isfinite(radius) || radius <= 0.0F) {
        throw std::invalid_argument("The radius of a random point around a center must be a finite number of meters above zero.");
    }

    const std::optional<Location> from = findNearest(center, options);
    if (!from) {
        return std::nullopt;
    }

    const dtNavMeshQuery& query = state->getQuery(options.maxNodes);
    const AreaFilter filter(options.filter != nullptr ? *options.filter : QueryFilter{});
    const std::optional<RandomPoints::Point> point = RandomPoints::around(*state->mesh, query, filter, from->polygon, DetourVector::of(from->position), radius, random, state->randomBuffers);
    if (!point) {
        return std::nullopt;
    }

    return state->locate(point->polygon, point->position);
}

void NavMesh::requireObstacle(const Obstacle& obstacle) const {
    if (!state->settings.obstacles) {
        throw std::invalid_argument("This navigation mesh was baked without obstacles. Bake it with the setting \"obstacles\" to add them.");
    }
    if (!obstacle.position.isFinite() || !std::isfinite(obstacle.yaw)) {
        throw std::invalid_argument("The position and the yaw of an obstacle must be finite numbers.");
    }

    const bool cylinder = obstacle.shape == Obstacle::Shape::Cylinder;
    const bool sized = cylinder ? std::isfinite(obstacle.radius) && obstacle.radius > 0.0F && std::isfinite(obstacle.height) && obstacle.height > 0.0F : obstacle.halfExtents.isFinite() && obstacle.halfExtents.x > 0.0F && obstacle.halfExtents.y > 0.0F && obstacle.halfExtents.z > 0.0F;
    if (!sized) {
        throw std::invalid_argument(cylinder ? "The radius and the height of a cylinder obstacle must be finite numbers above zero." : "The half extents of a box obstacle must be finite numbers above zero.");
    }
}

std::uint32_t NavMesh::addObstacle(const Obstacle& obstacle) {
    requireObstacle(obstacle);
    if (state->obstacles.size() >= state->settings.maxObstacles) {
        throw std::invalid_argument("The navigation mesh holds at most " + std::to_string(state->settings.maxObstacles) + " obstacles, the \"maxObstacles\" it was baked with.");
    }

    const std::uint32_t id = state->nextObstacle++;
    state->obstacles.emplace(id, obstacle);
    state->markColumns(state->getObstacleBounds(obstacle));
    return id;
}

void NavMesh::setObstacle(std::uint32_t id, const Obstacle& obstacle) {
    requireObstacle(obstacle);
    const auto found = state->obstacles.find(id);
    if (found == state->obstacles.end()) {
        throw std::invalid_argument("The navigation mesh has no obstacle " + std::to_string(id) + ".");
    }

    state->markColumns(state->getObstacleBounds(found->second));
    found->second = obstacle;
    state->markColumns(state->getObstacleBounds(obstacle));
}

bool NavMesh::removeObstacle(std::uint32_t id) {
    const auto found = state->obstacles.find(id);
    if (found == state->obstacles.end()) {
        return false;
    }

    state->markColumns(state->getObstacleBounds(found->second));
    state->obstacles.erase(found);
    return true;
}

std::optional<Obstacle> NavMesh::getObstacle(std::uint32_t id) const {
    const auto found = state->obstacles.find(id);
    return found != state->obstacles.end() ? std::optional(found->second) : std::nullopt;
}

std::size_t NavMesh::getObstacleCount() const noexcept {
    return state->obstacles.size();
}

std::uint32_t NavMesh::addLink(const Link& link) {
    SourceGeometry::requireLink(link);

    const std::uint32_t id = state->nextLink++;
    state->links.emplace(id, MeshLink{.id = id, .link = link, .area = state->getArea(link.area)});
    state->markLinkColumn(link);
    return id;
}

bool NavMesh::removeLink(std::uint32_t id) {
    const auto found = state->links.find(id);
    if (found == state->links.end()) {
        return false;
    }

    state->markLinkColumn(found->second.link);
    state->links.erase(found);
    return true;
}

std::optional<Link> NavMesh::getLink(std::uint32_t id) const {
    const auto found = state->links.find(id);
    return found != state->links.end() ? std::optional(found->second.link) : std::nullopt;
}

std::size_t NavMesh::getLinkCount() const noexcept {
    return state->links.size();
}

void NavMesh::rebuild(std::shared_ptr<const SourceGeometry> source, const math::Box& area) {
    const math::Box& bounds = state->grid.bounds;
    if (area.isEmpty() || area.max.x < bounds.min.x || area.min.x > bounds.max.x || area.max.z < bounds.min.z || area.min.z > bounds.max.z) {
        throw std::invalid_argument("The area of a rebuild must touch the bounds that the navigation mesh was baked for.");
    }

    state->markColumns(area, std::move(source));
}

void NavMesh::update(core::JobSystem* jobs) {
    const std::vector<std::size_t> dirty = std::exchange(state->dirtyColumns, {});
    for (const std::size_t index : dirty) {
        State::Column& column = state->columns[index];
        if (!column.dirty) {
            continue;
        }
        if (column.building) {
            state->dirtyColumns.push_back(index);
            continue;
        }

        State::Work work = state->prepare(index);
        if (jobs == nullptr) {
            core::JobSystem::Result<State::Built> result;
            try {
                result.value.emplace(State::buildColumn(state->settings, state->grid, std::move(work)));
            } catch (const std::exception& exception) {
                result.error = exception.what();
            }
            state->finish(index, std::move(result));
            continue;
        }

        // clang-format off
        jobs->run([settings = state->settings, grid = state->grid, work = std::move(work)]() mutable {
            return State::buildColumn(settings, grid, std::move(work));
        }, [weak = std::weak_ptr<State>(state), index](core::JobSystem::Result<State::Built> result) {
            if (const std::shared_ptr<State> owner = weak.lock()) {
                owner->finish(index, std::move(result));
            }
        });
        // clang-format on
    }
}

void NavMesh::whenUpToDate(std::function<void(std::string)> callback) {
    if (state->pending == 0) {
        callback({});
        return;
    }

    state->waiters.push_back(std::move(callback));
}

bool NavMesh::isUpToDate() const noexcept {
    return state->pending == 0;
}

std::size_t NavMesh::getPendingTileCount() const noexcept {
    return state->pending;
}

NavMesh::Triangles NavMesh::getTriangles() const {
    Triangles triangles;
    const dtNavMesh& detour = *state->mesh;
    for (int index = 0; index < detour.getMaxTiles(); ++index) {
        const dtMeshTile* tile = detour.getTile(index);
        if (tile->header == nullptr) {
            continue;
        }

        for (int polygon = 0; polygon < tile->header->polyCount; ++polygon) {
            const dtPoly& poly = tile->polys[polygon];
            if (poly.getType() == DT_POLYTYPE_OFFMESH_CONNECTION) {
                continue;
            }

            // Each detail triangle names the corners of its polygon first and then the detail vertices of the polygon.
            const dtPolyDetail& detail = tile->detailMeshes[polygon];
            for (int triangle = 0; triangle < detail.triCount; ++triangle) {
                const unsigned char* corners = &tile->detailTris[(detail.triBase + static_cast<unsigned int>(triangle)) * 4];
                for (int corner = 0; corner < 3; ++corner) {
                    const unsigned char vertex = corners[corner];
                    const float* point = vertex < poly.vertCount ? &tile->verts[poly.verts[vertex] * 3] : &tile->detailVerts[(detail.vertBase + vertex - poly.vertCount) * 3];
                    triangles.corners.push_back(DetourVector::toVec3(point));
                }
                triangles.areas.push_back(poly.getArea());
            }
        }
    }

    return triangles;
}

} // namespace haylen::navigation3d
