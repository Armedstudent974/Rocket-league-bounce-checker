// Scenarios.h - car/ball setups for the training drills.
//
// Pure math (no BakkesMod dependency) so it can be unit-tested.
// Angles use Unreal rotator units: 65536 = 360 degrees. Yaw 0 faces +X, 16384 faces +Y.
#pragma once

#include "BallPredictor.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace bounce {

constexpr float kPi = 3.14159265f;
constexpr float kUnrealPerDegree = 65536.f / 360.f;
constexpr float kCarRestHeight = 17.01f;  // Octane hitbox centre height when resting on the floor

inline int DegToUnreal(float deg) { return static_cast<int>(std::lround(deg * kUnrealPerDegree)); }
inline float UnrealToDeg(int u) { return static_cast<float>(u) / kUnrealPerDegree; }
// Wraps a yaw into the game's range [-32768, 32767].
inline int NormalizeYaw(int u) { return ((u + 32768) % 65536 + 65536) % 65536 - 32768; }
inline int YawFromRadians(float rad) { return NormalizeYaw(DegToUnreal(rad * 180.f / kPi)); }

struct CarPose {
    Vec3 position;
    int pitch = 0, yaw = 0, roll = 0;  // Unreal rotator units
    Vec3 velocity;
};

struct Scenario {
    bool moveCar = false;
    CarPose car;
    Vec3 ballPosition;
    Vec3 ballVelocity;
    Vec3 ballAngularVelocity;
};

// Current car state the scenarios start from.
struct CarState {
    Vec3 position;
    int yaw = 0;
};

// Settings for "custom ball launch".
struct LaunchSettings {
    Vec3 position{0, 0, 300};  // world position, or offset in car space (x fwd, y right, z up) when relative
    float speed = 1500.f;      // uu/s
    float yawDeg = 0.f;        // launch direction, relative to car facing when `relativeToCar`
    float pitchDeg = 30.f;     // upward angle of the launch
    bool relativeToCar = true;
};

namespace scenario_detail {

inline Vec3 Forward(float yawRad) { return {std::cos(yawRad), std::sin(yawRad), 0.f}; }
inline Vec3 Right(float yawRad) { return {-std::sin(yawRad), std::cos(yawRad), 0.f}; }

// Keeps a point inside the arena (with margin) so setups never spawn in a wall.
inline Vec3 ClampToArena(Vec3 p, float margin) {
    using namespace constants;
    p.x = std::clamp(p.x, -kSideWallX + margin, kSideWallX - margin);
    p.y = std::clamp(p.y, -kBackWallY + margin, kBackWallY - margin);
    p.z = std::clamp(p.z, kBallRadius, kCeilingZ - margin);
    const float over = std::fabs(p.x) + std::fabs(p.y) - (kCornerOffset - margin * 1.42f);
    if (over > 0.f) {
        p.x -= (p.x >= 0 ? 1.f : -1.f) * over * 0.5f;
        p.y -= (p.y >= 0 ? 1.f : -1.f) * over * 0.5f;
    }
    return p;
}

// True if `start` facing `yawRad` has `length` uu of open floor ahead.
inline bool HasRunway(const Vec3& start, float yawRad, float length) {
    const Vec3 end = start + Forward(yawRad) * length;
    const Vec3 clamped = ClampToArena(end, 300.f);
    return std::fabs(clamped.x - end.x) < 1.f && std::fabs(clamped.y - end.y) < 1.f;
}

// Picks a start spot for ground drills: the car's own spot if there's room ahead,
// otherwise a known-good spot in the blue half driving toward the orange goal.
inline void GroundStart(const CarState& car, float runway, Vec3& pos, float& yawRad) {
    pos = {car.position.x, car.position.y, kCarRestHeight};
    yawRad = UnrealToDeg(car.yaw) * kPi / 180.f;
    if (!HasRunway(pos, yawRad, runway)) {
        pos = {0.f, -3000.f, kCarRestHeight};
        yawRad = kPi / 2.f;
    }
}

}  // namespace scenario_detail

// Ball back to the kickoff spot, stopped.
inline Scenario BallReset() {
    Scenario s;
    s.ballPosition = {0.f, 0.f, constants::kBallRadius + 1.f};
    return s;
}

// Ball teleported just in front of the car, floating and stopped.
inline Scenario BallToCar(const CarState& car) {
    using namespace scenario_detail;
    const float yaw = UnrealToDeg(car.yaw) * kPi / 180.f;
    Scenario s;
    s.ballPosition = ClampToArena(car.position + Forward(yaw) * 350.f + Vec3(0, 0, 150.f), constants::kBallRadius + 10.f);
    return s;
}

// Car teleported behind the ball, facing it. Ball keeps moving.
inline Scenario CarToBall(const Vec3& ballPos, const Vec3& ballVel) {
    using namespace scenario_detail;
    // Face toward the orange goal from behind the ball, or along the ball's travel if it's moving.
    float yaw = kPi / 2.f;
    const float flatSpeed = std::sqrt(ballVel.x * ballVel.x + ballVel.y * ballVel.y);
    if (flatSpeed > 200.f) yaw = std::atan2(ballVel.y, ballVel.x);
    Scenario s;
    s.moveCar = true;
    s.car.position = ClampToArena(Vec3(ballPos.x, ballPos.y, kCarRestHeight) - Forward(yaw) * 600.f, 200.f);
    s.car.position.z = kCarRestHeight;
    s.car.yaw = YawFromRadians(yaw);
    s.ballPosition = ballPos;
    s.ballVelocity = ballVel;
    return s;
}

// Custom ball launch from the menu sliders.
inline Scenario CustomLaunch(const CarState& car, const LaunchSettings& ls) {
    using namespace scenario_detail;
    const float carYaw = UnrealToDeg(car.yaw) * kPi / 180.f;
    Scenario s;
    Vec3 pos = ls.position;
    float dirYaw = ls.yawDeg * kPi / 180.f;
    if (ls.relativeToCar) {
        pos = car.position + Forward(carYaw) * ls.position.x + Right(carYaw) * ls.position.y + Vec3(0, 0, ls.position.z);
        dirYaw += carYaw;
    }
    const float pitch = ls.pitchDeg * kPi / 180.f;
    s.ballPosition = ClampToArena(pos, constants::kBallRadius + 10.f);
    s.ballVelocity = Vec3(std::cos(dirYaw) * std::cos(pitch), std::sin(dirYaw) * std::cos(pitch), std::sin(pitch)) * ls.speed;
    return s;
}

// Musty flick: ball balanced on the car's roof while driving forward.
// Drive, jump, tilt back and flip backwards to flick the ball.
inline Scenario MustyFlick(const CarState& car) {
    using namespace scenario_detail;
    Vec3 start;
    float yaw;
    GroundStart(car, 2500.f, start, yaw);
    const float speed = 700.f;
    Scenario s;
    s.moveCar = true;
    s.car.position = start;
    s.car.yaw = YawFromRadians(yaw);
    s.car.velocity = Forward(yaw) * speed;
    // Sit the ball on the roof, slightly behind centre so it rests against the windshield.
    s.ballPosition = start + Forward(yaw) * -10.f + Vec3(0, 0, 130.f);
    s.ballVelocity = s.car.velocity;
    return s;
}

// Air dribble: ball popped up in front of a car driving at it.
// Jump, boost and keep the ball on your nose.
inline Scenario AirDribble(const CarState& car) {
    using namespace scenario_detail;
    Vec3 start;
    float yaw;
    GroundStart(car, 4000.f, start, yaw);
    const float speed = 1000.f;
    Scenario s;
    s.moveCar = true;
    s.car.position = start;
    s.car.yaw = YawFromRadians(yaw);
    s.car.velocity = Forward(yaw) * speed;
    s.ballPosition = start + Forward(yaw) * 220.f + Vec3(0, 0, 230.f);
    s.ballVelocity = Forward(yaw) * (speed * 0.95f) + Vec3(0, 0, 650.f);
    return s;
}

// Ceiling shot: car driving up the nearest side wall, ball dropping from near the ceiling.
// Drive onto the ceiling, drop off (you keep your flip), then hit the ball.
inline Scenario CeilingShot(const CarState& car) {
    using namespace constants;
    const float side = car.position.x >= 0 ? 1.f : -1.f;
    const float y = std::clamp(car.position.y, -3000.f, 3000.f);
    Scenario s;
    s.moveCar = true;
    // Wheels on the wall, nose pointing up. Yaw toward the wall then pitch up 88 degrees:
    // the roof then faces the middle of the field.
    s.car.position = {side * (kSideWallX - kCarRestHeight - 2.f), y, 700.f};
    s.car.yaw = side > 0 ? 0 : -32768;
    s.car.pitch = DegToUnreal(88.f);
    s.car.velocity = {0.f, 0.f, 1600.f};
    s.ballPosition = {side * (kSideWallX - 900.f), y, kCeilingZ - 250.f};
    s.ballVelocity = {-side * 350.f, 0.f, -150.f};
    return s;
}

// Adds random spread to a setup's ball velocity (0 = none, 1 = +/-50% speed and +/-30 degrees).
inline void ApplyVariance(Scenario& s, float amount, std::mt19937& rng) {
    if (amount <= 0.f) return;
    std::uniform_real_distribution<float> u(-1.f, 1.f);
    const float speedScale = 1.f + u(rng) * 0.5f * amount;
    const float turn = u(rng) * (30.f * kPi / 180.f) * amount;
    const Vec3 v = s.ballVelocity;
    const float c = std::cos(turn), sn = std::sin(turn);
    s.ballVelocity = Vec3(v.x * c - v.y * sn, v.x * sn + v.y * c, v.z) * speedScale;
}

}  // namespace bounce
