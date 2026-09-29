#include "haylen/2d/physics/Fracture.hpp"

#include <box2d/box2d.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "2d/physics/Box2DConverter.hpp"
#include "haylen/2d/physics/World.hpp"
#include "haylen/2d/procedural/Region.hpp"
#include "haylen/2d/procedural/Voronoi.hpp"
#include "haylen/math/Geometry.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/math/Polygon.hpp"
#include "haylen/math/Random.hpp"

namespace haylen::physics2d {

std::vector<std::vector<std::vector<math::Vec2>>> Fracture::split(std::span<const std::vector<math::Vec2>> shape, const Options& options) {
    if (options.pieces < 1) {
        throw std::invalid_argument("A fracture needs at least one piece.");
    }
    const std::vector<std::vector<math::Vec2>> outlines = math::Polygon::unite(shape);
    if (outlines.empty()) {
        return {};
    }

    // Sites near the impact point come from a distance that shrinks quadratically, and sites that miss the shape fall back to a uniform point inside it.
    const procedural2d::Region region = procedural2d::Region::polygon(outlines);
    const math::Rect bounds = region.getBounds();
    const float reach = std::max(bounds.width, bounds.height) * 0.5F;
    math::Random random(options.seed);
    std::vector<math::Vec2> sites;
    for (int piece = 0; piece < options.pieces; ++piece) {
        math::Vec2 site = region.getRandomPoint(random);
        if (options.impact) {
            const float distance = reach * random.nextFloat() * random.nextFloat();
            const math::Vec2 near = *options.impact + math::Vec2::fromAngle(random.range(0.0F, math::Math::kTau), distance);
            site = region.contains(near) ? near : site;
        }
        sites.push_back(site);
    }

    // A cell can cut a concave shape into separate islands, and every island becomes a piece with the holes inside it.
    const procedural2d::Voronoi voronoi(sites, bounds.expanded(1.0F));
    std::vector<std::vector<std::vector<math::Vec2>>> pieces;
    for (const std::vector<math::Vec2>& cell : voronoi.getCells()) {
        if (cell.empty()) {
            continue;
        }
        const std::vector<std::vector<math::Vec2>> cells{cell};
        const std::vector<std::vector<math::Vec2>> parts = math::Polygon::intersect(outlines, cells);
        for (const std::vector<math::Vec2>& outer : parts) {
            if (math::Geometry::signedArea(outer) <= 0.0F) {
                continue;
            }
            std::vector<std::vector<math::Vec2>> piece{outer};
            for (const std::vector<math::Vec2>& hole : parts) {
                if (math::Geometry::signedArea(hole) < 0.0F && math::Geometry::contains(outer, hole.front())) {
                    piece.push_back(hole);
                }
            }
            if (math::Polygon::getArea(piece) >= options.minimumArea) {
                pieces.push_back(std::move(piece));
            }
        }
    }
    return pieces;
}

std::vector<Body> Fracture::shatter(Body body, const Options& options) {
    const std::vector<Shape> shapes = body.getShapes();
    World& world = *body.getWorld();
    const float scale = world.getPixelsPerMeter();
    const b2BodyId source = b2LoadBodyId(body.getId());

    // The polygons of the body, in its local frame, and the material of the first one.
    std::vector<std::vector<math::Vec2>> outlines;
    Shape::Options material;
    for (const Shape& shape : shapes) {
        const b2ShapeId id = b2LoadShapeId(shape.getId());
        if (b2Shape_GetType(id) != b2_polygonShape) {
            continue;
        }
        if (outlines.empty()) {
            material = {.density = b2Shape_GetDensity(id), .friction = b2Shape_GetFriction(id), .restitution = b2Shape_GetRestitution(id), .filter = shape.getFilter()};
        }
        const b2Polygon polygon = b2Shape_GetPolygon(id);
        std::vector<math::Vec2>& outline = outlines.emplace_back();
        for (int vertex = 0; vertex < polygon.count; ++vertex) {
            outline.push_back(Box2DConverter::toPixels(polygon.vertices[vertex], scale));
        }
    }
    if (outlines.empty()) {
        return {};
    }

    const math::Vec2 position = body.getPosition();
    const float rotation = body.getRotation();
    Options local = options;
    if (options.impact) {
        local.impact = (*options.impact - position).rotated(-rotation);
    }

    // Every piece moves like the point of the original body where it sits, spinning at the same rate.
    const math::Vec2 velocity = body.getVelocity();
    const float spin = body.getAngularVelocity();
    const math::Vec2 center = Box2DConverter::toPixels(b2Body_GetWorldCenterOfMass(source), scale);
    const Body::Options settings{.type = body.getType(), .position = position, .rotation = rotation, .angularVelocity = spin, .linearDamping = body.getLinearDamping(), .angularDamping = body.getAngularDamping(), .gravityScale = body.getGravityScale(), .bullet = body.isBullet()};

    std::vector<Body> fragments;
    for (const std::vector<std::vector<math::Vec2>>& piece : split(outlines, local)) {
        const math::Vec2 offset = position + math::Geometry::centroid(piece.front()).rotated(rotation) - center;
        Body::Options placed = settings;
        placed.velocity = velocity + math::Vec2{-spin * offset.y, spin * offset.x};
        Body fragment = world.createBody(placed);
        for (const std::vector<math::Vec2>& part : math::Polygon::decompose(piece)) {
            fragment.addPolygon(part, material);
        }
        fragments.push_back(fragment);
    }
    body.destroy();
    return fragments;
}

} // namespace haylen::physics2d
