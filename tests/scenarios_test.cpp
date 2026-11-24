// Sanity tests for Scenarios.h. Build: g++ -std=c++17 -I../plugin/src scenarios_test.cpp
#include "Scenarios.h"

#include <cstdio>

using namespace bounce;

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { std::printf("FAIL line %d: ", __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); ++failures; } } while (0)

static bool InArena(const Vec3& p) {
    using namespace constants;
    return std::fabs(p.x) <= kSideWallX && std::fabs(p.y) <= kBackWallY && p.z >= 0 && p.z <= kCeilingZ &&
           std::fabs(p.x) + std::fabs(p.y) <= kCornerOffset;
}

int main() {
    // Rotator conversion.
    CHECK(DegToUnreal(90) == 16384, "deg->unreal");
    CHECK(std::fabs(UnrealToDeg(32768) - 180.f) < 0.01f, "unreal->deg");
    CHECK(NormalizeYaw(32768) == -32768 && NormalizeYaw(-32769) == 32767 && NormalizeYaw(100) == 100, "normalize");
    CHECK(YawFromRadians(kPi) == -32768, "yaw from pi");

    // Ball to car lands in front of a car facing +Y.
    {
        Scenario s = BallToCar({{0, 0, 17}, 16384});
        CHECK(s.ballPosition.y > 300 && std::fabs(s.ballPosition.x) < 1, "ball-to-car: got (%.0f,%.0f)", s.ballPosition.x, s.ballPosition.y);
        CHECK(!s.moveCar, "ball-to-car moves car");
    }

    // Custom launch relative to a car facing +Y: forward offset goes to +Y, launch goes +Y and up.
    {
        LaunchSettings ls;
        ls.position = {500, 0, 200};
        ls.speed = 1000; ls.yawDeg = 0; ls.pitchDeg = 45; ls.relativeToCar = true;
        Scenario s = CustomLaunch({{0, 0, 17}, 16384}, ls);
        CHECK(std::fabs(s.ballPosition.y - 500) < 1 && std::fabs(s.ballPosition.x) < 1, "launch pos (%.0f,%.0f)", s.ballPosition.x, s.ballPosition.y);
        CHECK(s.ballVelocity.y > 600 && s.ballVelocity.z > 600 && std::fabs(s.ballVelocity.x) < 1, "launch vel");
        CHECK(std::fabs(s.ballVelocity.Length() - 1000) < 1, "launch speed");
    }

    // Every drill from many car spots spawns car and ball inside the arena,
    // and ground drills always have room to drive forward.
    for (float x = -4000; x <= 4000; x += 500)
        for (float y = -5000; y <= 5000; y += 500)
            for (int yaw = 0; yaw < 65536; yaw += 8192) {
                CarState c{{x, y, 17}, yaw};
                if (std::fabs(x) + std::fabs(y) > constants::kCornerOffset) continue;
                const Scenario drills[] = {MustyFlick(c), AirDribble(c), CeilingShot(c), BallToCar(c), CarToBall(c.position, {0, 0, 0})};
                const char* names[] = {"musty", "air dribble", "ceiling", "ball-to-car", "car-to-ball"};
                for (int d = 0; d < 5; ++d) {
                    CHECK(InArena(drills[d].ballPosition), "%s: ball out of arena from (%.0f,%.0f)", names[d], x, y);
                    if (drills[d].moveCar) {
                        CHECK(InArena(drills[d].car.position), "%s: car out of arena from (%.0f,%.0f)", names[d], x, y);
                        CHECK(drills[d].car.yaw >= -32768 && drills[d].car.yaw <= 32767, "%s: yaw out of range", names[d]);
                        CHECK(drills[d].car.pitch >= -16364 && drills[d].car.pitch <= 16340, "%s: pitch out of range", names[d]);
                    }
                }
                for (const Scenario& g : {MustyFlick(c), AirDribble(c)}) {
                    const float yr = UnrealToDeg(g.car.yaw) * kPi / 180.f;
                    CHECK(scenario_detail::HasRunway(g.car.position, yr, 2500.f), "no runway from (%.0f,%.0f)", x, y);
                }
            }

    // Musty: ball sits on the roof and moves with the car.
    {
        Scenario s = MustyFlick({{0, -3000, 17}, 16384});
        CHECK(s.ballPosition.z > 120 && std::fabs(s.ballPosition.y - s.car.position.y) < 50, "musty ball not on roof");
        CHECK(std::fabs(s.ballVelocity.y - s.car.velocity.y) < 1, "musty ball not moving with car");
    }

    // Ceiling: car on the nearest wall, nose up, ball near the ceiling.
    {
        Scenario s = CeilingShot({{-2000, 0, 17}, 0});
        CHECK(s.car.position.x < -4000 && s.car.velocity.z > 0, "ceiling car not on -x wall");
        CHECK(s.ballPosition.z > 1700, "ceiling ball too low");
    }

    // Variance keeps the launch roughly the same size.
    {
        std::mt19937 rng(7);
        for (int i = 0; i < 100; ++i) {
            Scenario s;
            s.ballVelocity = {1000, 0, 500};
            ApplyVariance(s, 1.f, rng);
            float len = s.ballVelocity.Length();
            CHECK(len > 1118 * 0.49f && len < 1118 * 1.51f, "variance speed %.0f", len);
        }
    }

    if (failures) { std::printf("%d failure(s)\n", failures); return 1; }
    std::printf("all scenario tests passed\n");
    return 0;
}
