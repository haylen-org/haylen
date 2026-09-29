#include "haylen/2d/procedural/Delaunay.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <utility>

namespace haylen::procedural2d {

// Runs Delaunator on the points mirrored vertically. Delaunator winds its triangles clockwise in the coordinates it sees, so the mirror makes them counter-clockwise, with a positive signed area, in the coordinates of the engine.
class Delaunay::Builder final {
  public:
    explicit Builder(const std::vector<math::Vec2>& sites) : count(sites.size()), xs(count), ys(count), hullPrev(count), hullNext(count), hullTri(count), ids(count), distances(count) {
        for (std::size_t index = 0; index < count; ++index) {
            xs[index] = static_cast<double>(sites[index].x);
            ys[index] = -static_cast<double>(sites[index].y);
        }
        const std::size_t maxTriangles = count > 2 ? count * 2 - 5 : 0;
        triangles.reserve(maxTriangles * 3);
        halfedges.reserve(maxTriangles * 3);
        hashSize = std::max<std::size_t>(1, static_cast<std::size_t>(std::ceil(std::sqrt(static_cast<double>(count)))));
        hullHash.assign(hashSize, -1);
    }

    void run() {
        if (count == 0) {
            return;
        }

        double minX = std::numeric_limits<double>::infinity();
        double minY = minX;
        double maxX = -minX;
        double maxY = -minX;
        for (std::size_t index = 0; index < count; ++index) {
            minX = std::min(minX, xs[index]);
            minY = std::min(minY, ys[index]);
            maxX = std::max(maxX, xs[index]);
            maxY = std::max(maxY, ys[index]);
            ids[index] = static_cast<std::uint32_t>(index);
        }
        const double middleX = (minX + maxX) / 2.0;
        const double middleY = (minY + maxY) / 2.0;

        // The seed triangle starts at the point nearest the center, adds its nearest point and then the point with the smallest circumcircle.
        std::size_t i0 = 0;
        double best = std::numeric_limits<double>::infinity();
        for (std::size_t index = 0; index < count; ++index) {
            const double distance = distanceSquared(middleX, middleY, xs[index], ys[index]);
            if (distance < best) {
                i0 = index;
                best = distance;
            }
        }

        std::size_t i1 = count;
        best = std::numeric_limits<double>::infinity();
        for (std::size_t index = 0; index < count; ++index) {
            const double distance = distanceSquared(xs[i0], ys[i0], xs[index], ys[index]);
            if (index != i0 && distance < best && distance > 0.0) {
                i1 = index;
                best = distance;
            }
        }

        std::size_t i2 = count;
        double smallest = std::numeric_limits<double>::infinity();
        for (std::size_t index = 0; i1 < count && index < count; ++index) {
            if (index == i0 || index == i1) {
                continue;
            }
            const double radius = circumradius(i0, i1, index);
            if (radius < smallest) {
                i2 = index;
                smallest = radius;
            }
        }

        if (i2 == count || !std::isfinite(smallest)) {
            buildLine();
            return;
        }
        if (orient(i0, i1, i2) < 0.0) {
            std::swap(i1, i2);
        }
        sweep(i0, i1, i2);
    }

    std::vector<std::uint32_t> triangles;
    std::vector<std::int32_t> halfedges;
    std::vector<std::uint32_t> hull;

  private:
    [[nodiscard]] static double distanceSquared(double ax, double ay, double bx, double by) noexcept {
        const double dx = ax - bx;
        const double dy = ay - by;
        return dx * dx + dy * dy;
    }

    // Negative when a, b and c turn counter-clockwise in the mirrored coordinates, as Delaunator's orient predicate.
    [[nodiscard]] double orient(double px, double py, double qx, double qy, double rx, double ry) const noexcept {
        return (qy - py) * (rx - qx) - (qx - px) * (ry - qy);
    }
    [[nodiscard]] double orient(std::size_t p, std::size_t q, std::size_t r) const noexcept {
        return orient(xs[p], ys[p], xs[q], ys[q], xs[r], ys[r]);
    }

    [[nodiscard]] double circumradius(std::size_t a, std::size_t b, std::size_t c) const noexcept {
        const double dx = xs[b] - xs[a];
        const double dy = ys[b] - ys[a];
        const double ex = xs[c] - xs[a];
        const double ey = ys[c] - ys[a];
        const double bl = dx * dx + dy * dy;
        const double cl = ex * ex + ey * ey;
        const double d = 0.5 / (dx * ey - dy * ex);
        const double x = (ey * bl - dy * cl) * d;
        const double y = (dx * cl - ex * bl) * d;
        return x * x + y * y;
    }

    [[nodiscard]] bool isInCircle(std::size_t a, std::size_t b, std::size_t c, std::size_t p) const noexcept {
        const double dx = xs[a] - xs[p];
        const double dy = ys[a] - ys[p];
        const double ex = xs[b] - xs[p];
        const double ey = ys[b] - ys[p];
        const double fx = xs[c] - xs[p];
        const double fy = ys[c] - ys[p];
        const double ap = dx * dx + dy * dy;
        const double bp = ex * ex + ey * ey;
        const double cp = fx * fx + fy * fy;
        return dx * (ey * cp - bp * fy) - dy * (ex * cp - bp * fx) + ap * (ex * fy - ey * fx) < 0.0;
    }

    // Grows monotonically with the angle around the center without any trigonometry.
    [[nodiscard]] std::size_t hashKey(double x, double y) const noexcept {
        const double dx = x - centerX;
        const double dy = y - centerY;
        const double p = dx / (std::fabs(dx) + std::fabs(dy));
        const double angle = (dy > 0.0 ? 3.0 - p : 1.0 + p) / 4.0;
        return static_cast<std::size_t>(std::floor(angle * static_cast<double>(hashSize))) % hashSize;
    }

    // Collinear points have no triangles, and their hull lists every distinct point along the line.
    void buildLine() {
        for (std::size_t index = 0; index < count; ++index) {
            const double along = xs[index] - xs[0];
            distances[index] = along != 0.0 ? along : ys[index] - ys[0];
        }
        sortByDistance();
        double previous = -std::numeric_limits<double>::infinity();
        for (const std::uint32_t id : ids) {
            if (distances[id] > previous) {
                hull.push_back(id);
                previous = distances[id];
            }
        }
    }

    void sortByDistance() {
        std::sort(ids.begin(), ids.end(), [this](std::uint32_t lhs, std::uint32_t rhs) { return distances[lhs] < distances[rhs] || (distances[lhs] == distances[rhs] && lhs < rhs); });
    }

    void link(std::size_t edge, std::int32_t other) noexcept {
        halfedges[edge] = other;
        if (other != -1) {
            halfedges[static_cast<std::size_t>(other)] = static_cast<std::int32_t>(edge);
        }
    }

    std::size_t addTriangle(std::size_t a, std::size_t b, std::size_t c, std::int32_t edgeA, std::int32_t edgeB, std::int32_t edgeC) {
        const std::size_t triangle = triangles.size();
        triangles.insert(triangles.end(), {static_cast<std::uint32_t>(a), static_cast<std::uint32_t>(b), static_cast<std::uint32_t>(c)});
        halfedges.insert(halfedges.end(), {-1, -1, -1});
        link(triangle, edgeA);
        link(triangle + 1, edgeB);
        link(triangle + 2, edgeC);
        return triangle;
    }

    // Flips edges until the triangles around the new point satisfy the Delaunay condition, with an explicit stack instead of recursion.
    std::size_t legalize(std::size_t start) {
        std::size_t a = start;
        std::size_t ar = 0;
        edgeStack.clear();
        while (true) {
            const std::int32_t b = halfedges[a];
            const std::size_t a0 = a - a % 3;
            ar = a0 + (a + 2) % 3;

            if (b == -1) {
                if (edgeStack.empty()) {
                    break;
                }
                a = edgeStack.back();
                edgeStack.pop_back();
                continue;
            }

            const auto opposite = static_cast<std::size_t>(b);
            const std::size_t b0 = opposite - opposite % 3;
            const std::size_t al = a0 + (a + 1) % 3;
            const std::size_t bl = b0 + (opposite + 2) % 3;
            const std::uint32_t p0 = triangles[ar];
            const std::uint32_t pr = triangles[a];
            const std::uint32_t pl = triangles[al];
            const std::uint32_t p1 = triangles[bl];

            if (!isInCircle(p0, pr, pl, p1)) {
                if (edgeStack.empty()) {
                    break;
                }
                a = edgeStack.back();
                edgeStack.pop_back();
                continue;
            }

            triangles[a] = p1;
            triangles[opposite] = p0;
            const std::int32_t hbl = halfedges[bl];

            // An edge swapped on the other side of the hull leaves a stale hull triangle, which is fixed here.
            if (hbl == -1) {
                std::size_t edge = hullStart;
                do {
                    if (hullTri[edge] == bl) {
                        hullTri[edge] = a;
                        break;
                    }
                    edge = hullPrev[edge];
                } while (edge != hullStart);
            }
            link(a, hbl);
            link(opposite, halfedges[ar]);
            link(ar, static_cast<std::int32_t>(bl));
            edgeStack.push_back(b0 + (opposite + 1) % 3);
        }
        return ar;
    }

    void sweep(std::size_t i0, std::size_t i1, std::size_t i2) {
        // The circumcenter of the seed triangle orders the points and centers the hull hash.
        const double dx = xs[i1] - xs[i0];
        const double dy = ys[i1] - ys[i0];
        const double ex = xs[i2] - xs[i0];
        const double ey = ys[i2] - ys[i0];
        const double bl = dx * dx + dy * dy;
        const double cl = ex * ex + ey * ey;
        const double d = 0.5 / (dx * ey - dy * ex);
        centerX = xs[i0] + (ey * bl - dy * cl) * d;
        centerY = ys[i0] + (dx * cl - ex * bl) * d;
        for (std::size_t index = 0; index < count; ++index) {
            distances[index] = distanceSquared(xs[index], ys[index], centerX, centerY);
        }
        sortByDistance();

        hullStart = i0;
        std::size_t hullSize = 3;
        hullNext[i0] = hullPrev[i2] = i1;
        hullNext[i1] = hullPrev[i0] = i2;
        hullNext[i2] = hullPrev[i1] = i0;
        hullTri[i0] = 0;
        hullTri[i1] = 1;
        hullTri[i2] = 2;
        hullHash[hashKey(xs[i0], ys[i0])] = static_cast<std::int64_t>(i0);
        hullHash[hashKey(xs[i1], ys[i1])] = static_cast<std::int64_t>(i1);
        hullHash[hashKey(xs[i2], ys[i2])] = static_cast<std::int64_t>(i2);
        addTriangle(i0, i1, i2, -1, -1, -1);

        double previousX = 0.0;
        double previousY = 0.0;
        for (std::size_t k = 0; k < count; ++k) {
            const std::size_t i = ids[k];
            const double x = xs[i];
            const double y = ys[i];

            // Near-duplicate points and the seed points are skipped.
            if (k > 0 && std::fabs(x - previousX) <= kEpsilon && std::fabs(y - previousY) <= kEpsilon) {
                continue;
            }
            previousX = x;
            previousY = y;
            if (i == i0 || i == i1 || i == i2) {
                continue;
            }

            // The hull hash finds a hull edge the point can see.
            std::size_t start = 0;
            const std::size_t key = hashKey(x, y);
            for (std::size_t j = 0; j < hashSize; ++j) {
                const std::int64_t candidate = hullHash[(key + j) % hashSize];
                if (candidate != -1 && static_cast<std::size_t>(candidate) != hullNext[static_cast<std::size_t>(candidate)]) {
                    start = static_cast<std::size_t>(candidate);
                    break;
                }
            }
            start = hullPrev[start];
            std::size_t e = start;
            bool visible = true;
            while (orient(x, y, xs[e], ys[e], xs[hullNext[e]], ys[hullNext[e]]) >= 0.0) {
                e = hullNext[e];
                if (e == start) {
                    visible = false;
                    break;
                }
            }
            if (!visible) {
                continue;
            }

            std::size_t t = addTriangle(e, i, hullNext[e], -1, -1, static_cast<std::int32_t>(hullTri[e]));
            hullTri[i] = legalize(t + 2);
            hullTri[e] = t;
            ++hullSize;

            // Walks forward through the hull, adding triangles and flipping them.
            std::size_t n = hullNext[e];
            while (orient(x, y, xs[n], ys[n], xs[hullNext[n]], ys[hullNext[n]]) < 0.0) {
                const std::size_t q = hullNext[n];
                t = addTriangle(n, i, q, static_cast<std::int32_t>(hullTri[i]), -1, static_cast<std::int32_t>(hullTri[n]));
                hullTri[i] = legalize(t + 2);
                hullNext[n] = n;
                --hullSize;
                n = q;
            }

            // Walks backward from the other side, adding triangles and flipping them.
            if (e == start) {
                while (orient(x, y, xs[hullPrev[e]], ys[hullPrev[e]], xs[e], ys[e]) < 0.0) {
                    const std::size_t q = hullPrev[e];
                    t = addTriangle(q, i, e, -1, static_cast<std::int32_t>(hullTri[e]), static_cast<std::int32_t>(hullTri[q]));
                    legalize(t + 2);
                    hullTri[q] = t;
                    hullNext[e] = e;
                    --hullSize;
                    e = q;
                }
            }

            hullStart = hullPrev[i] = e;
            hullNext[e] = hullPrev[n] = i;
            hullNext[i] = n;
            hullHash[hashKey(x, y)] = static_cast<std::int64_t>(i);
            hullHash[hashKey(xs[e], ys[e])] = static_cast<std::int64_t>(e);
        }

        hull.reserve(hullSize);
        for (std::size_t index = 0, edge = hullStart; index < hullSize; ++index, edge = hullNext[edge]) {
            hull.push_back(static_cast<std::uint32_t>(edge));
        }
    }

    static constexpr double kEpsilon = 2.220446049250313e-16;

    std::size_t count;
    std::vector<double> xs;
    std::vector<double> ys;
    std::vector<std::size_t> hullPrev;
    std::vector<std::size_t> hullNext;
    std::vector<std::size_t> hullTri;
    std::vector<std::int64_t> hullHash;
    std::vector<std::uint32_t> ids;
    std::vector<double> distances;
    std::vector<std::size_t> edgeStack;
    std::size_t hashSize = 1;
    std::size_t hullStart = 0;
    double centerX = 0.0;
    double centerY = 0.0;
};

Delaunay::Delaunay(std::vector<math::Vec2> sites) : points(std::move(sites)) {
    Builder builder(points);
    builder.run();
    triangles = std::move(builder.triangles);
    halfedges = std::move(builder.halfedges);
    hull = std::move(builder.hull);
    collectNeighbors();
}

void Delaunay::collectNeighbors() {
    neighbors.assign(points.size(), {});
    for (std::size_t edge = 0; edge < triangles.size(); ++edge) {
        const std::uint32_t from = triangles[edge];
        const std::uint32_t to = triangles[edge - edge % 3 + (edge + 1) % 3];
        neighbors[from].push_back(to);
        neighbors[to].push_back(from);
    }
    if (triangles.empty()) {
        for (std::size_t index = 1; index < hull.size(); ++index) {
            neighbors[hull[index - 1]].push_back(hull[index]);
            neighbors[hull[index]].push_back(hull[index - 1]);
        }
    }
    for (std::vector<std::uint32_t>& list : neighbors) {
        std::sort(list.begin(), list.end());
        list.erase(std::unique(list.begin(), list.end()), list.end());
    }
}

math::Vec2 Delaunay::getCircumcenter(std::size_t triangle) const noexcept {
    const math::Vec2 a = points[triangles[triangle * 3]];
    const math::Vec2 b = points[triangles[triangle * 3 + 1]];
    const math::Vec2 c = points[triangles[triangle * 3 + 2]];
    const double dx = static_cast<double>(b.x - a.x);
    const double dy = static_cast<double>(b.y - a.y);
    const double ex = static_cast<double>(c.x - a.x);
    const double ey = static_cast<double>(c.y - a.y);
    const double bl = dx * dx + dy * dy;
    const double cl = ex * ex + ey * ey;
    const double d = 0.5 / (dx * ey - dy * ex);
    return {a.x + static_cast<float>((ey * bl - dy * cl) * d), a.y + static_cast<float>((dx * cl - ex * bl) * d)};
}

// Greedy steps along Delaunay edges always reach the nearest point, because the triangulation contains the nearest neighbor graph.
std::uint32_t Delaunay::findNearest(math::Vec2 position, std::uint32_t start) const noexcept {
    std::uint32_t current = start;
    float best = math::Vec2::distanceSquared(points[current], position);
    while (true) {
        std::uint32_t next = current;
        for (const std::uint32_t neighbor : neighbors[current]) {
            const float distance = math::Vec2::distanceSquared(points[neighbor], position);
            if (distance < best) {
                best = distance;
                next = neighbor;
            }
        }
        if (next == current) {
            return current;
        }
        current = next;
    }
}

std::uint32_t Delaunay::findNearest(math::Vec2 position) const noexcept {
    return findNearest(position, hull.empty() ? 0U : hull.front());
}

} // namespace haylen::procedural2d
