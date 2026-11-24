// BounceChecker - BakkesMod plugin that draws a red predicted-ball line with
// red boxes marking every spot the ball will bounce.
//
// Only active in Freeplay, Custom Training and offline matches. BakkesMod
// does not give plugins the ball's state in online games, and this plugin
// checks that it is offline before drawing anything.

#include "BounceChecker.h"

#include "bakkesmod/wrappers/GameEvent/ServerWrapper.h"
#include "bakkesmod/wrappers/GameObject/BallWrapper.h"
#include "bakkesmod/wrappers/GameObject/CameraWrapper.h"
#include "bakkesmod/wrappers/wrapperstructs.h"

#include <cmath>
#include <cstdio>
#include <string>

BAKKESMOD_PLUGIN(BounceChecker, "Bounce Checker - ball bounce prediction", "1.0.0", PLUGINTYPE_FREEPLAY | PLUGINTYPE_CUSTOM_TRAINING)

namespace {

bounce::Vec3 ToVec(const Vector& v) { return {v.X, v.Y, v.Z}; }

}  // namespace

// Small helper because the SDK Vector uses capitalised members.
static Vector V(const bounce::Vec3& v) { return Vector(v.x, v.y, v.z); }

void BounceChecker::onLoad() {
    cvarManager->registerCvar("bounce_enabled", "1", "Show the bounce prediction line", true, true, 0, true, 1)
        .bindTo(enabled_);
    cvarManager->registerCvar("bounce_seconds", "3.0", "How many seconds ahead to predict", true, true, 0.5f, true, 8.f)
        .bindTo(predictSeconds_);
    cvarManager->registerCvar("bounce_max_bounces", "3", "How many bounce boxes to draw", true, true, 1, true, 10)
        .bindTo(maxBounces_);
    cvarManager->registerCvar("bounce_line_width", "3.0", "Thickness of the red line", true, true, 1.f, true, 10.f)
        .bindTo(lineWidth_);
    cvarManager->registerCvar("bounce_show_timer", "1", "Show time-until-bounce above each box", true, true, 0, true, 1)
        .bindTo(showTimer_);

    cvarManager->registerNotifier(
        "bounce_toggle",
        [this](std::vector<std::string>) {
            auto cv = cvarManager->getCvar("bounce_enabled");
            cv.setValue(!cv.getBoolValue());
        },
        "Toggle the bounce prediction overlay", PERMISSION_ALL);

    gameWrapper->RegisterDrawable([this](CanvasWrapper canvas) { Render(canvas); });
    cvarManager->log("BounceChecker loaded. Bind a key with: bind F3 bounce_toggle");
}

void BounceChecker::onUnload() {
    gameWrapper->UnregisterDrawables();
}

bool BounceChecker::CanDraw() const {
    if (!*enabled_) return false;
    if (gameWrapper->IsInOnlineGame()) return false;  // never in online matches
    return gameWrapper->IsInFreeplay() || gameWrapper->IsInCustomTraining() || gameWrapper->IsInGame();
}

void BounceChecker::Render(CanvasWrapper canvas) {
    if (!CanDraw()) return;

    ServerWrapper server = gameWrapper->GetGameEventAsServer();
    if (server.IsNull()) return;
    BallWrapper ball = server.GetBall();
    if (ball.IsNull()) return;
    CameraWrapper camera = gameWrapper->GetCamera();
    if (camera.IsNull()) return;

    bounce::BallState state;
    state.position = ToVec(ball.GetLocation());
    state.velocity = ToVec(ball.GetVelocity());
    state.angularVelocity = ToVec(ball.GetAngularVelocity());

    const bounce::Prediction pred = bounce::PredictBall(state, *predictSeconds_);

    // Points behind the camera project to garbage, so skip them.
    const Vector camLoc = camera.GetLocation();
    const Vector camFwd = RotatorToVector(camera.GetRotation());
    auto inFront = [&](const bounce::Vec3& p) {
        Vector d = V(p) - camLoc;
        return Vector::dot(d, camFwd) > 10.f;
    };

    const LinearColor red{255.f, 0.f, 0.f, 255.f};
    const LinearColor redFill{255.f, 0.f, 0.f, 70.f};
    const float width = *lineWidth_;

    // ---- 1. red trajectory line (ball centre path) ----
    canvas.SetColor(red);
    const int stride = 2;  // every 2nd sample is plenty smooth at 120Hz
    for (size_t i = stride; i < pred.path.size(); i += stride) {
        const auto& a = pred.path[i - stride];
        const auto& b = pred.path[i];
        if (!inFront(a) || !inFront(b)) continue;
        canvas.DrawLine(canvas.ProjectF(V(a)), canvas.ProjectF(V(b)), width);
    }

    // ---- 2. red box at each bounce spot ----
    int drawn = 0;
    for (const auto& bnc : pred.bounces) {
        if (drawn >= *maxBounces_) break;
        ++drawn;

        // Build a square lying flat on the surface the ball hits, centred on the contact point.
        const bounce::Vec3& n = bnc.normal;
        bounce::Vec3 t1 = std::fabs(n.z) > 0.9f ? bounce::Vec3(1, 0, 0) : bounce::Vec3(0, 0, 1);
        t1 = bounce::Cross(n, t1);
        t1 = t1 * (1.f / t1.Length());
        const bounce::Vec3 t2 = bounce::Cross(n, t1);

        const float half = bounce::constants::kBallRadius;  // box is one ball wide
        const bounce::Vec3 c = bnc.contact + n * 2.f;       // lift slightly so it isn't hidden by the floor
        const bounce::Vec3 corners[4] = {
            c + t1 * half + t2 * half,
            c - t1 * half + t2 * half,
            c - t1 * half - t2 * half,
            c + t1 * half - t2 * half,
        };

        bool visible = true;
        for (const auto& corner : corners) visible = visible && inFront(corner);
        if (!visible) continue;

        Vector2F sc[4];
        for (int k = 0; k < 4; ++k) sc[k] = canvas.ProjectF(V(corners[k]));

        // translucent fill
        canvas.SetColor(redFill);
        canvas.FillTriangle(sc[0], sc[1], sc[2]);
        canvas.FillTriangle(sc[0], sc[2], sc[3]);

        // solid outline - the first (next) bounce is drawn thicker
        canvas.SetColor(red);
        const float boxWidth = drawn == 1 ? width + 2.f : width;
        for (int k = 0; k < 4; ++k) canvas.DrawLine(sc[k], sc[(k + 1) % 4], boxWidth);

        // vertical drop line from the ball's centre at contact down to the box, helps depth perception
        if (inFront(bnc.position)) canvas.DrawLine(canvas.ProjectF(V(bnc.position)), canvas.ProjectF(V(c)), 1.f);

        if (*showTimer_) {
            char label[16];
            std::snprintf(label, sizeof(label), "%.2fs", bnc.time);
            Vector2F lp = canvas.ProjectF(V(c + n * 30.f));
            canvas.SetPosition(Vector2F{lp.X - 20.f, lp.Y - 20.f});
            canvas.DrawString(label, 1.2f, 1.2f, true);
        }
    }
}
