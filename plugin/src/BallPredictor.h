// BallPredictor.h - standalone Rocket League ball trajectory predictor.
//
// No BakkesMod dependency, so it can be unit-tested on any platform.
// Units are Unreal units (uu), seconds, radians/sec - the same units the game uses.
//
// Physics model:
//   * gravity -650 uu/s^2, air drag 0.0305/s, speed cap 6000 uu/s
//   * standard Soccar arena: floor, ceiling, side walls, back walls,
//     45-degree corners and the goal boxes
//   * bounces use the community-measured RL bounce model
//     (restitution 0.6, friction 0.285, spin coupling)
// The arena's curved wall-to-floor ramps are approximated by flat planes,
// so predictions off the curved transitions are slightly less accurate.
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace bounce {

struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    float Length() const { return std::sqrt(x * x + y * y + z * z); }
};

inline float Dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 Cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline Vec3 ClampLength(const Vec3& v, float maxLen) {
    float len = v.Length();
    return (len > maxLen && len > 0.f) ? v * (maxLen / len) : v;
}

namespace constants {
constexpr float kBallRadius     = 91.25f;
constexpr float kGravity        = -650.f;
constexpr float kDrag           = 0.0305f;
constexpr float kMaxSpeed       = 6000.f;
constexpr float kMaxSpin        = 6.f;
constexpr float kRestitution    = 0.6f;
constexpr float kFriction       = 0.285f;
constexpr float kFrictionScale  = 2.0f;
constexpr float kSpinCoupling   = 0.0003f;

constexpr float kSideWallX      = 4096.f;
constexpr float kBackWallY      = 5120.f;
constexpr float kCeilingZ       = 2044.f;
constexpr float kCornerOffset   = 8064.f;   // |x| + |y| = 8064 on the corner planes
constexpr float kGoalHalfWidth  = 892.755f;
constexpr float kGoalHeight     = 642.775f;
constexpr float kGoalDepth      = 880.f;    // goal back wall at |y| = 6000
}  // namespace constants

enum class Surface { Floor, Ceiling, Wall, Corner, Goal };

struct BounceEvent {
    Vec3 position;      // ball centre at the moment of contact
    Vec3 contact;       // point where the ball touches the surface
    Vec3 normal;        // surface normal (pointing into the arena)
    float time = 0.f;   // seconds from now
    Surface surface = Surface::Floor;
};

struct Prediction {
    std::vector<Vec3> path;            // ball centre, one sample per step
    std::vector<BounceEvent> bounces;  // every significant contact
    float step = 1.f / 120.f;
};

struct BallState {
    Vec3 position;
    Vec3 velocity;
    Vec3 angularVelocity;
};

namespace detail {

struct Plane {
    Vec3 normal;     // unit, points into the playable space
    float distance;  // signed distance from ball centre to the plane
    Surface surface;
};

// Collects every arena plane that the ball centre is currently near.
inline void NearbyPlanes(const Vec3& p, std::vector<Plane>& out) {
    using namespace constants;
    out.clear();
    const float ax = std::fabs(p.x), ay = std::fabs(p.y);
    const float sx = p.x >= 0 ? 1.f : -1.f, sy = p.y >= 0 ? 1.f : -1.f;
    const float invSqrt2 = 0.70710678f;

    out.push_back({{0, 0, 1}, p.z, Surface::Floor});

    const bool inGoalMouth = ax < kGoalHalfWidth && p.z < kGoalHeight;
    if (ay > kBackWallY && inGoalMouth) {
        // Inside a goal: goal back wall, posts/side nets and crossbar/roof.
        out.push_back({{0, -sy, 0}, (kBackWallY + kGoalDepth) - ay, Surface::Goal});
        out.push_back({{-sx, 0, 0}, kGoalHalfWidth - ax, Surface::Goal});
        out.push_back({{0, 0, -1}, kGoalHeight - p.z, Surface::Goal});
        return;
    }

    out.push_back({{0, 0, -1}, kCeilingZ - p.z, Surface::Ceiling});
    out.push_back({{-sx, 0, 0}, kSideWallX - ax, Surface::Wall});
    out.push_back({{-sx * invSqrt2, -sy * invSqrt2, 0}, (kCornerOffset - ax - ay) * invSqrt2, Surface::Corner});

    // Back wall - open where the goal mouth is (with ball radius of clearance).
    const bool fitsThroughGoal = ax < kGoalHalfWidth - kBallRadius * 0.5f && p.z < kGoalHeight - kBallRadius * 0.5f;
    if (!fitsThroughGoal) {
        out.push_back({{0, -sy, 0}, kBackWallY - ay, Surface::Wall});
    }
}

}  // namespace detail

// Predicts the ball's flight for `seconds` and records every bounce.
inline Prediction PredictBall(BallState s, float seconds = 4.f, float step = 1.f / 120.f) {
    using namespace constants;
    Prediction result;
    result.step = step;
    const int steps = static_cast<int>(seconds / step);
    result.path.reserve(steps + 1);
    result.path.push_back(s.position);

    std::vector<detail::Plane> planes;
    planes.reserve(8);

    // Ignore tiny contacts so a ball rolling along the floor doesn't spam bounces.
    const float minBounceSpeed = 120.f;

    for (int i = 1; i <= steps; ++i) {
        s.velocity += Vec3(0, 0, kGravity * step);
        s.velocity = s.velocity * (1.f - kDrag * step);
        s.velocity = ClampLength(s.velocity, kMaxSpeed);
        s.position += s.velocity * step;

        detail::NearbyPlanes(s.position, planes);
        for (const auto& plane : planes) {
            if (plane.distance >= kBallRadius) continue;
            const Vec3& n = plane.normal;
            const float into = Dot(s.velocity, n);
            // Push the ball back out of the surface.
            s.position += n * (kBallRadius - plane.distance);
            if (into >= 0.f) continue;

            // RL bounce model.
            const Vec3 vPerp = n * into;
            const Vec3 vPara = s.velocity - vPerp;
            const Vec3 vSpin = Cross(n, s.angularVelocity) * kBallRadius;
            const Vec3 slip = vPara + vSpin;
            const float slipLen = slip.Length();
            const float ratio = slipLen > 1e-3f ? vPerp.Length() / slipLen : 0.f;

            const Vec3 dPerp = vPerp * -(1.f + kRestitution);
            const Vec3 dPara = slip * -(std::min(1.f, kFrictionScale * ratio) * kFriction);

            s.velocity += dPerp + dPara;
            s.angularVelocity += Cross(dPara, n) * (kSpinCoupling * kBallRadius);
            s.angularVelocity = ClampLength(s.angularVelocity, kMaxSpin);

            if (-into >= minBounceSpeed) {
                BounceEvent ev;
                ev.position = s.position;
                ev.contact = s.position - n * kBallRadius;
                ev.normal = n;
                ev.time = i * step;
                ev.surface = plane.surface;
                result.bounces.push_back(ev);
            }
        }
        result.path.push_back(s.position);
    }
    return result;
}

// Convenience: first floor bounce (the one you usually want to read), or nullptr.
inline const BounceEvent* FirstFloorBounce(const Prediction& p) {
    for (const auto& b : p.bounces)
        if (b.surface == Surface::Floor) return &b;
    return nullptr;
}

}  // namespace bounce
