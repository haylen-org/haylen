#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "haylen/math/Vec2.hpp"

namespace haylen::core {
class JobSystem;
}

namespace haylen::navigation2d {

// Moves many agents at once with optimal reciprocal collision avoidance, the ORCA of the RVO2 library. Every step, each agent takes the velocity closest to the one it prefers among those that keep it clear of its neighbors for a time horizon, sharing the effort with them, and clear of static obstacles for a shorter one. Flocking weights can bend the preferred velocities. Steps are deterministic and allocate nothing once the buffers have grown, and a job system spreads them over its workers.
class Crowd final {
  public:
    struct AgentOptions {
        math::Vec2 position{};
        math::Vec2 velocity{};
        float radius = 16.0F;
        float maxSpeed = 120.0F;
        float neighborDistance = 160.0F;
        std::size_t maxNeighbors = 10;
        float timeHorizon = 2.0F;
        float obstacleTimeHorizon = 1.0F;
    };

    // Weights of the flocking forces added to the preferred velocity of every agent, computed from the neighbors it avoids.
    struct Flocking {
        float separation = 0.0F;
        float alignment = 0.0F;
        float cohesion = 0.0F;
    };

    explicit Crowd(const Flocking& value = kDefaultFlocking);

    std::uint32_t addAgent(const AgentOptions& options);
    bool removeAgent(std::uint32_t id);
    [[nodiscard]] bool hasAgent(std::uint32_t id) const noexcept;
    [[nodiscard]] std::size_t getAgentCount() const noexcept {
        return activeAgents;
    }

    // Adds a solid polygon in either winding, or a wall seen from both sides when it has two points.
    void addObstacle(std::span<const math::Vec2> polygon);
    void clearObstacles() noexcept;

    [[nodiscard]] math::Vec2 getPosition(std::uint32_t id) const;
    void setPosition(std::uint32_t id, math::Vec2 value);
    [[nodiscard]] math::Vec2 getVelocity(std::uint32_t id) const;
    [[nodiscard]] float getRadius(std::uint32_t id) const;
    [[nodiscard]] float getMaxSpeed(std::uint32_t id) const;

    // Sets the velocity the agent wants, which also drops its target.
    void setPreferredVelocity(std::uint32_t id, math::Vec2 value);
    [[nodiscard]] math::Vec2 getPreferredVelocity(std::uint32_t id) const;

    // Makes the agent head for the target at full speed every step and slow down to stop on it. Clearing the target leaves the agent wanting to stand still.
    void setTarget(std::uint32_t id, math::Vec2 value);
    void clearTarget(std::uint32_t id);
    [[nodiscard]] std::optional<math::Vec2> getTarget(std::uint32_t id) const;

    [[nodiscard]] const Flocking& getFlocking() const noexcept {
        return flocking;
    }
    void setFlocking(const Flocking& value) noexcept {
        flocking = value;
    }

    // Picks the new velocity of every agent from the state before the step, then moves them all. Without a job system the step runs on the calling thread.
    void step(float deltaSeconds, core::JobSystem* jobs = nullptr);

  private:
    struct Agent {
        AgentOptions options;
        math::Vec2 preferred{};
        std::optional<math::Vec2> target;
        math::Vec2 next{};
        bool active = false;
    };

    // One vertex of an obstacle outline and the edge that leaves it, as RVO2 stores obstacles.
    struct Vertex {
        math::Vec2 point{};
        math::Vec2 direction{};
        std::size_t next = 0;
        std::size_t previous = 0;
        bool convex = true;
    };

    // The half-plane of allowed velocities lies to the left of the line.
    struct Line {
        math::Vec2 point{};
        math::Vec2 direction{};
    };

    // The buffers of one chunk of agents, so chunks running in parallel never share them.
    struct Scratch {
        std::vector<Line> lines;
        std::vector<Line> projected;
        std::vector<std::pair<float, std::uint32_t>> neighbors;
        std::vector<std::pair<float, std::size_t>> walls;
    };

    static const Flocking kDefaultFlocking;
    static constexpr float kEpsilon = 1e-5F;

    // Steps with a job system give each worker chunks of at least this many agents.
    static constexpr std::size_t kChunkAgents = 64;

    [[nodiscard]] static float leftOf(math::Vec2 a, math::Vec2 b, math::Vec2 c) noexcept;
    [[nodiscard]] static bool solveLine(std::span<const Line> lines, std::size_t lineIndex, float radius, math::Vec2 optimal, bool directionOptimal, math::Vec2& result) noexcept;
    [[nodiscard]] static std::size_t solvePlanes(std::span<const Line> lines, float radius, math::Vec2 optimal, bool directionOptimal, math::Vec2& result) noexcept;
    static void solveFallback(std::span<const Line> lines, std::size_t obstacleLines, std::size_t beginLine, float radius, std::vector<Line>& projected, math::Vec2& result);

    [[nodiscard]] Agent& agentAt(std::uint32_t id);
    [[nodiscard]] const Agent& agentAt(std::uint32_t id) const;

    // Sorts the agents into grid buckets as wide as the largest neighbor distance, which neighbor queries scan.
    void bucketAgents();
    void findNeighbors(std::uint32_t id, Scratch& scratch) const;
    void findWalls(std::uint32_t id, Scratch& scratch) const;
    [[nodiscard]] math::Vec2 preferredVelocity(std::uint32_t id, const Scratch& scratch, float deltaSeconds) const noexcept;
    void addWallLines(const Agent& agent, Scratch& scratch) const;
    void addAgentLines(std::uint32_t id, Scratch& scratch, float deltaSeconds) const;

    // Stores the new velocity of one agent, which only touches that agent, so agents compute in parallel.
    void computeVelocity(std::uint32_t id, Scratch& scratch, float deltaSeconds);

    Flocking flocking;
    std::vector<Agent> agents;
    std::vector<std::uint32_t> freeAgents;
    std::size_t activeAgents = 0;
    std::vector<Vertex> vertices;
    std::vector<Scratch> scratches;

    math::Vec2 bucketOrigin{};
    float bucketSize = 1.0F;
    int bucketsX = 0;
    int bucketsY = 0;
    std::vector<std::uint32_t> bucketStarts;
    std::vector<std::uint32_t> bucketFill;
    std::vector<std::uint32_t> bucketAgentIds;
};

} // namespace haylen::navigation2d
