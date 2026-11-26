#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>

#include "haylen/3d/physics/Body.hpp"
#include "haylen/math/Quat.hpp"
#include "haylen/math/Spline3D.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::physics3d {

class World;

// Handle to a joint between two bodies of a 3D world, or between a body and the world, in meters, radians and newtons. It reports `false` from `isValid` once the joint, one of its bodies or its world is destroyed, and throws `std::logic_error` when used after that, as it does for a member that the type of the joint lacks.
class Joint final {
  public:
    // A fixed joint glues two bodies, a point joint pins them at a point, a distance joint keeps them within a range of lengths, a hinge turns them around an axis, a slider moves them along an axis, a cone keeps an axis inside a cone, a swing twist joint is the shoulder of a ragdoll, a six degrees of freedom joint frees, limits or fixes each of its six axes, a path joint moves a body along a spline, a gear turns two hinges together, a rack and pinion joins a hinge and a slider, and a pulley joins the lengths of two ropes over two fixed points.
    enum class Type : std::uint8_t {
        Fixed,
        Point,
        Distance,
        Hinge,
        Slider,
        Cone,
        SwingTwist,
        SixDof,
        Path,
        Gear,
        RackAndPinion,
        Pulley,
    };

    // The axes of a six degrees of freedom joint, in the space the joint was made in: its axis is `x` and its normal is `y`.
    enum class Axis : std::uint8_t {
        X,
        Y,
        Z,
        RotationX,
        RotationY,
        RotationZ,
    };

    // A free axis moves without bound, a limited one between its lower and upper values, and a fixed one never moves.
    enum class AxisMode : std::uint8_t {
        Free,
        Limited,
        Fixed,
    };

    // A spring of frequency has its frequency in hertz and the ratio of its damping, where 1 stops it without a bounce, and a spring of stiffness has its stiffness in newtons per meter and its damping in newton seconds per meter. A frequency or a stiffness of 0 makes it rigid.
    enum class SpringMode : std::uint8_t {
        Frequency,
        Stiffness,
    };

    // A motor of velocity drives the joint at its target speed, and a motor of position drives it toward its target place, through its spring.
    enum class MotorState : std::uint8_t {
        Off,
        Velocity,
        Position,
    };

    // How a body on a path turns: freely, around the tangent, the normal or the binormal of the path alone, with the path, or never.
    enum class PathRotation : std::uint8_t {
        Free,
        AroundTangent,
        AroundNormal,
        AroundBinormal,
        WithPath,
        Fixed,
    };

    struct Spring {
        SpringMode mode = SpringMode::Frequency;
        float frequency = 0.0F;
        float stiffness = 0.0F;
        float damping = 0.0F;

        [[nodiscard]] friend bool operator==(const Spring&, const Spring&) noexcept = default;
    };

    // A motor drives at most its force along an axis and its torque around one. Its target velocity is in meters or radians per second, and its target position in meters, radians or the share of a path from 0 to 1. A swing twist joint and the turns of a six degrees of freedom joint drive toward the target rotation and at the target angular velocity of the joint instead.
    struct Motor {
        MotorState state = MotorState::Off;
        float targetVelocity = 0.0F;
        float targetPosition = 0.0F;
        float maxForce = std::numeric_limits<float>::max();
        float maxTorque = std::numeric_limits<float>::max();
        Spring spring{.mode = SpringMode::Frequency, .frequency = 2.0F, .damping = 1.0F};
    };

    // One axis of a six degrees of freedom joint. The spring softens the limits of the axes that move along, and the friction holds the axis with up to that force or torque.
    struct AxisSettings {
        AxisMode mode = AxisMode::Free;
        float lower = 0.0F;
        float upper = 0.0F;
        Spring limitSpring{};
        float friction = 0.0F;
        Motor motor{};
    };

    // The force and torque that the joint held its bodies with in the last step, in newtons and newton meters.
    struct Load {
        float force = 0.0F;
        float torque = 0.0F;
    };

    // The numbers of a joint that `Simulation` reads and writes. The angle of a hinge, the position of a slider, the length of a distance joint and the share of a path that the body reached only read.
    enum class Value : std::uint8_t {
        Lower,
        Upper,
        Friction,
        HalfConeAngle,
        PlaneHalfConeAngle,
        Ratio,
        Angle,
        Position,
        Length,
        Fraction,
    };

    // The motors of a joint: the one of its main axis, the swing of a swing twist joint, and one for each axis of a six degrees of freedom joint.
    enum class MotorSlot : std::uint8_t {
        Main,
        Swing,
        X,
        Y,
        Z,
        RotationX,
        RotationY,
        RotationZ,
    };

    struct Options;

    Joint() = default;
    // A handle names its world by the key of its slot and generation and its joint by the id of the simulation.
    Joint(World* owner, std::uint64_t worldKey, std::uint32_t jointId) noexcept : world(owner), key(worldKey), id(jointId) {}

    [[nodiscard]] static std::optional<Type> typeFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view typeName(Type value) noexcept;
    [[nodiscard]] static std::optional<AxisMode> axisModeFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view axisModeName(AxisMode value) noexcept;
    [[nodiscard]] static std::optional<Axis> axisFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view axisName(Axis value) noexcept;
    [[nodiscard]] static std::optional<SpringMode> springModeFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view springModeName(SpringMode value) noexcept;
    [[nodiscard]] static std::optional<MotorState> motorStateFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view motorStateName(MotorState value) noexcept;
    [[nodiscard]] static std::optional<PathRotation> pathRotationFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view pathRotationName(PathRotation value) noexcept;

    [[nodiscard]] bool isValid() const noexcept;

    // Destroys the joint. Destroying a destroyed joint does nothing.
    void destroy();

    [[nodiscard]] Type getType() const;
    [[nodiscard]] Body getFirst() const;

    // The second body, or nothing for a joint that holds its body to the world.
    [[nodiscard]] std::optional<Body> getSecond() const;

    // A joint that is not enabled holds nothing until it is enabled again.
    [[nodiscard]] bool isEnabled() const;
    void setEnabled(bool value);

    // The solver solves joints of a higher priority later in each pass, so they hold better against the others.
    [[nodiscard]] int getPriority() const;
    void setPriority(int value);

    [[nodiscard]] Load getLoad() const;

    // Throws `std::invalid_argument` for a limit that is negative or not a number.
    [[nodiscard]] std::optional<float> getBreakForce() const;
    void setBreakForce(std::optional<float> value);
    [[nodiscard]] std::optional<float> getBreakTorque() const;
    void setBreakTorque(std::optional<float> value);

    // The limits of a hinge in radians, of a slider in meters, of the lengths of a distance joint or a pulley in meters, and of the twist of a swing twist joint in radians. Throws `std::invalid_argument` for a lower limit above the upper one, and for the limits of a hinge or a twist beyond half a turn.
    [[nodiscard]] float getLower() const;
    [[nodiscard]] float getUpper() const;
    void setLimits(float lower, float upper);

    // The spring that softens the limits of a hinge, a slider or a distance joint.
    [[nodiscard]] Spring getLimitSpring() const;
    void setLimitSpring(const Spring& value);

    // The friction that holds a hinge, a swing twist joint or a path in newton meters or newtons.
    [[nodiscard]] float getFriction() const;
    void setFriction(float value);

    // The half angles of the cone of a cone joint, or of a swing twist joint around its normal and its plane, in radians from 0 to pi.
    [[nodiscard]] float getHalfConeAngle() const;
    void setHalfConeAngle(float value);
    [[nodiscard]] float getPlaneHalfConeAngle() const;
    void setPlaneHalfConeAngle(float value);

    // The ratio of a gear, a rack and pinion in radians for each meter, or a pulley, which stays as the joint was made.
    [[nodiscard]] float getRatio() const;

    // What the joint reached in the last step: the angle of a hinge, the position of a slider, the length of a distance joint and the share of a path from 0 to 1.
    [[nodiscard]] float getAngle() const;
    [[nodiscard]] float getPosition() const;
    [[nodiscard]] float getLength() const;
    [[nodiscard]] float getFraction() const;

    // The motor of a hinge, a slider, a path or the twist of a swing twist joint, the swing motor of a swing twist joint and the motor of each axis of a six degrees of freedom joint.
    [[nodiscard]] Motor getMotor(MotorSlot slot = MotorSlot::Main) const;
    void setMotor(const Motor& value, MotorSlot slot = MotorSlot::Main);

    // The rotation in the space of the joint that the motors of position of a swing twist joint and of the turns of a six degrees of freedom joint drive toward, and the angular velocity that the motors of velocity of a swing twist joint drive at.
    [[nodiscard]] math::Quat getTargetRotation() const;
    void setTargetRotation(const math::Quat& value);
    [[nodiscard]] math::Vec3 getTargetAngularVelocity() const;
    void setTargetAngularVelocity(math::Vec3 value);

    // The settings of an axis of a six degrees of freedom joint.
    [[nodiscard]] AxisSettings getAxis(Axis axis) const;
    void setAxis(Axis axis, const AxisSettings& value);

    [[nodiscard]] std::uint32_t getId() const noexcept {
        return id;
    }
    [[nodiscard]] std::uint64_t getWorldKey() const noexcept {
        return key;
    }
    [[nodiscard]] World* getWorld() const noexcept {
        return world;
    }
    [[nodiscard]] friend bool operator==(const Joint& lhs, const Joint& rhs) noexcept {
        return lhs.key == rhs.key && lhs.id == rhs.id;
    }

  private:
    static const std::array<std::pair<std::string_view, Type>, 12> kTypeNames;
    static const std::array<std::pair<std::string_view, AxisMode>, 3> kAxisModeNames;
    static const std::array<std::pair<std::string_view, Axis>, 6> kAxisNames;
    static const std::array<std::pair<std::string_view, SpringMode>, 2> kSpringModeNames;
    static const std::array<std::pair<std::string_view, MotorState>, 3> kMotorStateNames;
    static const std::array<std::pair<std::string_view, PathRotation>, 6> kPathRotationNames;

    template <typename Enum, std::size_t Count> [[nodiscard]] static std::optional<Enum> fromName(const std::array<std::pair<std::string_view, Enum>, Count>& names, std::string_view name) noexcept;
    template <typename Enum, std::size_t Count> [[nodiscard]] static std::string_view nameOf(const std::array<std::pair<std::string_view, Enum>, Count>& names, Enum value) noexcept;

    // Returns the world of a live joint. Throws `std::logic_error` once the world or the joint is destroyed.
    [[nodiscard]] World& checkedWorld() const;

    // Returns the world of a live joint of one of the types, and throws `std::logic_error` with the sentence that names `member` and the joints that have it otherwise.
    [[nodiscard]] World& checkedWorld(std::initializer_list<Type> types, std::string_view member) const;

    World* world = nullptr;
    std::uint64_t key = 0;
    std::uint32_t id = 0;
};

// Every point and axis is in world space as the bodies stand when the joint is made, and the second point of a joint that names none is the first one. The angles, positions, limits and motors measure the second body against the first, and the body against the world in a joint to the world. The axis is the axis of a hinge, a slider, the twist of a cone or a swing twist joint and the `x` of a six degrees of freedom joint, and the normal is perpendicular to it, the direction of the angle 0 of a hinge and the `y` of a six degrees of freedom joint. Unset limits leave a hinge, a slider and a twist free and keep a distance joint and a pulley at their length when made. A path joint moves the second body along a spline that moves with the first, starting at the share of the path `pathFraction`. A gear turns the first body around `axis` and the second around `secondAxis`, the first one `ratio` times for each turn of the second, and a rack and pinion turns its pinion, the first body, around `axis` and moves its rack, the second, along `secondAxis`, by `ratio` radians for each meter. Each takes its hinges, or its hinge and its slider, as `joints`, so it never drifts from them. A pulley runs from `point` on the first body over `fixedPoint` and `secondFixedPoint` to `secondPoint` on the second, and its `ratio` weighs the second rope against the first. A joint with a break force or torque is destroyed by the first step that loads it past one of them, and the world records the break.
struct Joint::Options {
    math::Vec3 point{};
    std::optional<math::Vec3> secondPoint{};
    math::Vec3 axis{0.0F, 1.0F, 0.0F};
    math::Vec3 normal{1.0F, 0.0F, 0.0F};
    math::Vec3 secondAxis{0.0F, 1.0F, 0.0F};
    std::optional<float> lower{};
    std::optional<float> upper{};
    Spring limitSpring{};
    float friction = 0.0F;
    float halfConeAngle = 0.0F;
    float planeHalfConeAngle = 0.0F;
    Motor motor{};
    Motor swingMotor{};
    math::Quat targetRotation = math::Quat::identity();
    math::Vec3 targetAngularVelocity{};
    std::array<AxisSettings, 6> axes{};
    std::optional<math::Spline3D> path{};
    PathRotation pathRotation = PathRotation::Free;
    float pathFraction = 0.0F;
    float ratio = 1.0F;
    std::array<Joint, 2> joints{};
    math::Vec3 fixedPoint{};
    math::Vec3 secondFixedPoint{};
    int priority = 0;
    int velocitySteps = 0;
    int positionSteps = 0;
    bool collideConnected = false;
    bool enabled = true;
    std::optional<float> breakForce{};
    std::optional<float> breakTorque{};
};

} // namespace haylen::physics3d
