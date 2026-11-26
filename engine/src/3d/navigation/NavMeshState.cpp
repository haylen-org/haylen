#include "3d/navigation/NavMeshState.hpp"

#include <atomic>
#include <cmath>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <utility>

#include "3d/navigation/DetourVector.hpp"
#include "3d/navigation/TileBaker.hpp"
#include "3d/navigation/TileIndex.hpp"

namespace haylen::navigation3d {

std::uint8_t NavMesh::State::getArea(std::string_view name) const {
    if (name == SourceGeometry::kNone) {
        return SourceGeometry::kNoneArea;
    }

    for (std::size_t index = 0; index < areas.size(); ++index) {
        if (areas[index] == name) {
            return static_cast<std::uint8_t>(index + 1);
        }
    }

    throw std::invalid_argument("The navigation mesh has no area \"" + std::string(name) + "\". Name it in the geometry that the mesh bakes from.");
}

NavMesh::State::Built NavMesh::State::buildColumn(const BuildSettings& settings, const TileGrid& grid, Work work) {
    Built built;
    if (work.source) {
        std::vector<std::int32_t> triangles;
        TileIndex::collect(*work.source, grid, settings.cellSize, work.x, work.z, triangles);
        TileBaker baker(settings, grid);
        TileBaker::Column column = baker.bake(*work.source, triangles, work.x, work.z);
        built.baked = true;
        built.meshes = std::move(column.meshes);
        built.layers = std::move(column.layers);
    } else {
        built.meshes = std::move(work.meshes);
        built.layers = std::move(work.layers);
    }

    TileAssembler assembler(settings);
    for (const TileMesh& mesh : built.meshes) {
        built.tiles.push_back(assembler.assemble(mesh, work.links));
    }
    for (const TileLayer& layer : built.layers) {
        built.tiles.push_back(assembler.assemble(layer, work.obstacles, work.links));
    }

    return built;
}

std::unique_ptr<NavMesh::State> NavMesh::State::bake(const SourceGeometry& source, const BuildSettings& settings) {
    settings.validate();
    if (source.isEmpty()) {
        throw std::invalid_argument("The geometry of a navigation mesh holds no triangles, so there is nothing to bake.");
    }

    auto state = std::make_unique<State>();
    state->settings = settings;
    state->areas = source.getAreas();
    state->grid = TileGrid::make(settings, settings.bounds.value_or(source.getBounds()));
    state->columns.resize(state->grid.getColumnCount());
    state->addSourceLinks(source);

    const TileIndex index(source, state->grid, settings.cellSize);
    TileBaker baker(settings, state->grid);
    for (int z = 0; z < state->grid.rows; ++z) {
        for (int x = 0; x < state->grid.columns; ++x) {
            TileBaker::Column baked = baker.bake(source, index.getTriangles(x, z), x, z);
            Column& column = state->columns[state->grid.getIndex(x, z)];
            column.meshes = std::move(baked.meshes);
            column.layers = std::move(baked.layers);
        }
    }

    state->start();
    return state;
}

void NavMesh::State::bakeAsync(core::JobSystem& jobs, std::shared_ptr<const SourceGeometry> source, const BuildSettings& settings, BuildCallback callback) {
    settings.validate();
    if (source->isEmpty()) {
        throw std::invalid_argument("The geometry of a navigation mesh holds no triangles, so there is nothing to bake.");
    }

    auto bake = std::make_shared<Bake>();
    bake->jobs = &jobs;
    bake->source = std::move(source);
    bake->callback = std::move(callback);
    bake->state = std::make_unique<State>();
    bake->state->settings = settings;
    jobs.post([bake] { indexBake(bake); });
}

void NavMesh::State::indexBake(const std::shared_ptr<Bake>& bake) {
    try {
        State& state = *bake->state;
        state.areas = bake->source->getAreas();
        state.grid = TileGrid::make(state.settings, state.settings.bounds.value_or(bake->source->getBounds()));
        state.columns.resize(state.grid.getColumnCount());
        state.addSourceLinks(*bake->source);
        bake->index = std::make_unique<TileIndex>(*bake->source, state.grid, state.settings.cellSize);
    } catch (const std::exception& exception) {
        bake->error = exception.what();
        deliverBake(bake);
        return;
    }

    bake->remaining = bake->state->columns.size();
    for (std::size_t column = 0; column < bake->state->columns.size(); ++column) {
        bake->jobs->post([bake, column] { bakeColumn(bake, column); });
    }
}

void NavMesh::State::bakeColumn(const std::shared_ptr<Bake>& bake, std::size_t index) {
    State& state = *bake->state;
    const int x = static_cast<int>(index % static_cast<std::size_t>(state.grid.columns));
    const int z = static_cast<int>(index / static_cast<std::size_t>(state.grid.columns));
    try {
        TileBaker baker(state.settings, state.grid);
        TileBaker::Column baked = baker.bake(*bake->source, bake->index->getTriangles(x, z), x, z);
        state.columns[index].meshes = std::move(baked.meshes);
        state.columns[index].layers = std::move(baked.layers);
    } catch (const std::exception& exception) {
        const std::scoped_lock lock(bake->mutex);
        if (bake->error.empty()) {
            bake->error = exception.what();
        }
    }

    // The job of the last column to finish makes the mesh, once every column wrote what it baked.
    if (bake->remaining.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        deliverBake(bake);
    }
}

void NavMesh::State::deliverBake(const std::shared_ptr<Bake>& bake) {
    auto result = std::make_shared<core::JobSystem::Result<NavMesh>>();
    if (bake->error.empty()) {
        try {
            bake->state->start();
            result->value.emplace(NavMesh(std::move(bake->state)));
        } catch (const std::exception& exception) {
            result->error = exception.what();
        }
    } else {
        result->error = bake->error;
    }

    bake->jobs->postToFrame([bake, result] { bake->callback(std::move(*result)); });
}

void NavMesh::State::addSourceLinks(const SourceGeometry& source) {
    for (const Link& link : source.getLinks()) {
        const std::uint32_t id = nextLink++;
        links.emplace(id, MeshLink{.id = id, .link = link, .area = getArea(link.area)});
    }
}

std::unique_ptr<NavMesh::State> NavMesh::State::make(const NavMeshFile::Contents& contents) {
    auto state = std::make_unique<State>();
    state->settings = contents.settings;
    state->areas = contents.areas;
    state->grid = TileGrid::make(contents.settings, contents.bounds);
    state->columns.resize(state->grid.getColumnCount());
    for (const MeshLink& link : contents.links) {
        state->links.emplace(link.id, link);
        state->nextLink = std::max(state->nextLink, link.id + 1);
    }
    for (const TileMesh& mesh : contents.meshes) {
        state->columns[state->grid.getIndex(mesh.x, mesh.z)].meshes.push_back(mesh);
    }
    for (const TileLayer& layer : contents.layers) {
        state->columns[state->grid.getIndex(layer.x, layer.z)].layers.push_back(layer);
    }

    state->start();
    return state;
}

NavMeshFile::Contents NavMesh::State::getContents() const {
    NavMeshFile::Contents contents{.settings = settings, .bounds = grid.bounds, .areas = areas, .links = {}, .meshes = {}, .layers = {}};
    for (const auto& [id, link] : links) {
        contents.links.push_back(link);
    }
    for (const Column& column : columns) {
        contents.meshes.insert(contents.meshes.end(), column.meshes.begin(), column.meshes.end());
        contents.layers.insert(contents.layers.end(), column.layers.begin(), column.layers.end());
    }

    return contents;
}

void NavMesh::State::start() {
    std::size_t layers = 0;
    for (const Column& column : columns) {
        layers += column.layers.size();
    }

    // A mesh without obstacles keeps one tile in each column, and a mesh with obstacles keeps room for floors that rebuilds add.
    const std::size_t maxTiles = settings.obstacles ? layers + columns.size() * kLayerHeadroom : columns.size();
    dtNavMeshParams params{};
    params.orig[0] = grid.bounds.min.x;
    params.orig[1] = grid.bounds.min.y;
    params.orig[2] = grid.bounds.min.z;
    params.tileWidth = grid.tileWorld;
    params.tileHeight = grid.tileWorld;
    params.maxTiles = static_cast<int>(maxTiles);
    params.maxPolys = 0xFFFF;

    mesh.reset(dtAllocNavMesh());
    if (mesh == nullptr || dtStatusFailed(mesh->init(&params))) {
        throw std::runtime_error("A navigation mesh did not start, since the system ran out of memory.");
    }

    TileAssembler assembler(settings);
    for (std::size_t index = 0; index < columns.size(); ++index) {
        const int x = static_cast<int>(index % static_cast<std::size_t>(grid.columns));
        const int z = static_cast<int>(index / static_cast<std::size_t>(grid.columns));
        const std::vector<MeshLink> columnLinks = collectLinks(x, z);
        Built built;
        for (const TileMesh& tileMesh : columns[index].meshes) {
            built.tiles.push_back(assembler.assemble(tileMesh, columnLinks));
        }
        for (const TileLayer& layer : columns[index].layers) {
            built.tiles.push_back(assembler.assemble(layer, {}, columnLinks));
        }
        place(index, std::move(built));
    }
}

std::vector<MeshLink> NavMesh::State::collectLinks(int x, int z) const {
    std::vector<MeshLink> found;
    const math::Box ground = grid.getColumnBounds(x, z);
    for (const auto& [id, link] : links) {
        const math::Vec3 start = link.link.start;
        if (start.x >= ground.min.x && start.x < ground.max.x && start.z >= ground.min.z && start.z < ground.max.z) {
            found.push_back(link);
        }
    }

    return found;
}

std::vector<Obstacle> NavMesh::State::collectObstacles(int x, int z) const {
    std::vector<Obstacle> found;
    const math::Box reach = grid.getBakeBounds(x, z, settings.cellSize);
    for (const auto& [id, obstacle] : obstacles) {
        const math::Box bounds = getObstacleBounds(obstacle);
        if (bounds.min.x <= reach.max.x && bounds.max.x >= reach.min.x && bounds.min.z <= reach.max.z && bounds.max.z >= reach.min.z) {
            found.push_back(obstacle);
        }
    }

    return found;
}

NavMesh::State::Work NavMesh::State::prepare(std::size_t index) {
    Column& column = columns[index];
    Work work;
    work.x = static_cast<int>(index % static_cast<std::size_t>(grid.columns));
    work.z = static_cast<int>(index / static_cast<std::size_t>(grid.columns));
    work.source = std::move(column.source);
    if (!work.source) {
        work.meshes = column.meshes;
        work.layers = column.layers;
    }

    work.links = collectLinks(work.x, work.z);
    work.obstacles = collectObstacles(work.x, work.z);

    column.dirty = false;
    column.building = true;
    return work;
}

void NavMesh::State::place(std::size_t index, Built built) {
    Column& column = columns[index];
    if (built.baked) {
        column.meshes = std::move(built.meshes);
        column.layers = std::move(built.layers);
    }

    for (const dtTileRef tile : column.tiles) {
        mesh->removeTile(tile, nullptr, nullptr);
    }
    column.tiles.clear();

    for (TileAssembler::Tile& tile : built.tiles) {
        if (tile.data == nullptr) {
            continue;
        }

        dtTileRef reference = 0;
        if (dtStatusFailed(mesh->addTile(tile.data.get(), tile.size, DT_TILE_FREE_DATA, 0, &reference))) {
            const int x = static_cast<int>(index % static_cast<std::size_t>(grid.columns));
            const int z = static_cast<int>(index / static_cast<std::size_t>(grid.columns));
            throw std::runtime_error("The tile " + std::to_string(x) + ", " + std::to_string(z) + " of a navigation mesh has more floors than the mesh keeps room for. Bake the mesh again with the geometry that adds them.");
        }

        static_cast<void>(tile.data.release());
        column.tiles.push_back(reference);
    }
}

void NavMesh::State::finish(std::size_t index, core::JobSystem::Result<Built> result) {
    Column& column = columns[index];
    column.building = false;

    std::string error = std::move(result.error);
    if (result.value) {
        try {
            place(index, std::move(*result.value));
        } catch (const std::exception& exception) {
            error = exception.what();
        }
    }
    if (!column.dirty) {
        --pending;
    }

    if (!error.empty()) {
        if (waiters.empty()) {
            throw std::runtime_error(error);
        }

        const std::vector<std::function<void(std::string)>> waiting = std::exchange(waiters, {});
        for (const std::function<void(std::string)>& waiter : waiting) {
            waiter(error);
        }
        return;
    }

    if (pending == 0) {
        const std::vector<std::function<void(std::string)>> waiting = std::exchange(waiters, {});
        for (const std::function<void(std::string)>& waiter : waiting) {
            waiter({});
        }
    }
}

void NavMesh::State::markColumns(const math::Box& area, const std::shared_ptr<const SourceGeometry>& source) {
    // Every column whose border reaches the area changes, so the edges of neighboring tiles keep meeting.
    const float border = static_cast<float>(grid.borderCells) * settings.cellSize;
    const TileGrid::Range range = grid.getRange(area.expanded(border));
    for (int z = range.minZ; z <= range.maxZ; ++z) {
        for (int x = range.minX; x <= range.maxX; ++x) {
            const std::size_t index = grid.getIndex(x, z);
            if (source) {
                columns[index].source = source;
            }
            markColumn(index);
        }
    }
}

void NavMesh::State::markColumn(std::size_t index) {
    Column& column = columns[index];
    if (!column.dirty && !column.building) {
        ++pending;
    }
    if (!column.dirty) {
        column.dirty = true;
        dirtyColumns.push_back(index);
    }
}

void NavMesh::State::markLinkColumn(const Link& link) {
    const TileGrid::Range range = grid.getRange({link.start, link.start});
    if (range.minX > range.maxX || range.minZ > range.maxZ) {
        return;
    }

    markColumn(grid.getIndex(range.minX, range.minZ));
}

math::Box NavMesh::State::getObstacleBounds(const Obstacle& obstacle) const noexcept {
    const float reach = settings.agentRadius;
    if (obstacle.shape == Obstacle::Shape::Cylinder) {
        const float radius = obstacle.radius + reach;
        return {obstacle.position - math::Vec3{radius, settings.agentHeight, radius}, obstacle.position + math::Vec3{radius, obstacle.height, radius}};
    }

    // A box turned around the up axis reaches as far as the corners of its footprint at most.
    const float radius = std::hypot(obstacle.halfExtents.x, obstacle.halfExtents.z) + reach;
    const float half = std::fabs(obstacle.halfExtents.y);
    return {obstacle.position - math::Vec3{radius, half + settings.agentHeight, radius}, obstacle.position + math::Vec3{radius, half, radius}};
}

NavMesh::Location NavMesh::State::locate(dtPolyRef polygon, const std::array<float, 3>& position) const {
    const dtMeshTile* tile = nullptr;
    const dtPoly* poly = nullptr;
    mesh->getTileAndPolyByRefUnsafe(polygon, &tile, &poly);
    return {.position = DetourVector::toVec3(position.data()), .polygon = polygon, .area = poly->getArea()};
}

dtNavMeshQuery& NavMesh::State::getQuery(std::size_t maxNodes) {
    if (maxNodes == 0 || maxNodes > kMaxQueryNodes) {
        throw std::invalid_argument("A query of a navigation mesh visits from 1 to 65534 polygons, not " + std::to_string(maxNodes) + ".");
    }

    if (query == nullptr || queryNodes != maxNodes) {
        query.reset(dtAllocNavMeshQuery());
        if (query == nullptr || dtStatusFailed(query->init(mesh.get(), static_cast<int>(maxNodes)))) {
            query.reset();
            throw std::runtime_error("A query of a navigation mesh did not start, since the system ran out of memory.");
        }
        queryNodes = maxNodes;
    }

    return *query;
}

} // namespace haylen::navigation3d
