// Sanity tests for BallPredictor.h. Build: g++ -std=c++17 -O2 -I../plugin/src predictor_test.cpp
#include "BallPredictor.h"

#include <cstdio>
#include <cstdlib>
#include <random>

using namespace bounce;

static int failures = 0;
#define CHECK(cond, ...)                                  \
    do {                                                  \
        if (!(cond)) {                                    \
            std::printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            std::printf(__VA_ARGS__);                     \
            std::printf("\n");                            \
            ++failures;                                   \
        }                                                 \
    } while (0)

int main() {
    // 1. Straight drop from 1000uu: first floor contact ~ sqrt(2*(1000-91.25)/650) = 1.672s.
    {
        BallState s{{0, 0, 1000}, {0, 0, 0}, {0, 0, 0}};
        Prediction p = PredictBall(s, 4.f);
        const BounceEvent* b = FirstFloorBounce(p);
        CHECK(b != nullptr, "drop: no floor bounce");
        if (b) {
            CHECK(std::fabs(b->time - 1.672f) < 0.03f, "drop: bounce at %.3fs", b->time);
            CHECK(std::fabs(b->contact.z) < 1.f, "drop: contact z %.2f", b->contact.z);
            std::printf("drop: first bounce at t=%.3fs\n", b->time);
        }
        // Second bounce should be lower: apex height scales with 0.6^2.
        CHECK(p.bounces.size() >= 2, "drop: expected multiple bounces, got %zu", p.bounces.size());
    }

    // 2. Lob forward: bounce lands ahead of the ball in +y.
    {
        BallState s{{0, -2000, 300}, {0, 1500, 800}, {0, 0, 0}};
        Prediction p = PredictBall(s, 4.f);
        const BounceEvent* b = FirstFloorBounce(p);
        CHECK(b && b->position.y > -2000.f, "lob: bounce not ahead");
        if (b) std::printf("lob: lands at (%.0f, %.0f) after %.2fs\n", b->position.x, b->position.y, b->time);
    }

    // 3. Shot into the side wall bounces off it.
    {
        BallState s{{3000, 0, 500}, {2500, 0, 0}, {0, 0, 0}};
        Prediction p = PredictBall(s, 2.f);
        CHECK(!p.bounces.empty() && p.bounces[0].surface == Surface::Wall, "wall: first contact not wall");
    }

    // 4. Fuzz: the ball never leaves the arena (with small tolerance) for random launches.
    {
        std::mt19937 rng(1234);
        std::uniform_real_distribution<float> px(-3500, 3500), py(-4500, 4500), pz(100, 1800), vel(-3000, 3000);
        for (int t = 0; t < 2000; ++t) {
            BallState s{{px(rng), py(rng), pz(rng)}, {vel(rng), vel(rng), vel(rng)}, {0, 0, 0}};
            Prediction p = PredictBall(s, 6.f);
            for (const Vec3& q : p.path) {
                bool ok = q.z > constants::kBallRadius - 5 && q.z < constants::kCeilingZ &&
                          std::fabs(q.x) < constants::kSideWallX && std::fabs(q.y) < 6000.f &&
                          std::fabs(q.x) + std::fabs(q.y) < constants::kCornerOffset;
                if (!ok) {
                    CHECK(false, "fuzz %d: escaped arena at (%.0f, %.0f, %.0f)", t, q.x, q.y, q.z);
                    t = 1 << 30;
                    break;
                }
            }
        }
    }

    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("all predictor tests passed\n");
    return 0;
}
