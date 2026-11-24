// BounceChecker - BakkesMod training plugin for Rocket League.
//
//  * Overlay: red predicted-ball line with red boxes on every bounce.
//  * Training menu (F2 -> Plugins -> Bounce Checker): unlimited boost, ball reset /
//    teleport, custom ball launches with presets, and drills for musty flicks,
//    ceiling shots and air dribbles.
//
// Everything is limited to Freeplay and Custom Training (the overlay also runs
// in offline matches). Nothing runs in online matches: BakkesMod doesn't allow it
// and changing the ball or boost there would affect other players.

#include "BounceChecker.h"

#include "bakkesmod/wrappers/GameEvent/ServerWrapper.h"
#include "bakkesmod/wrappers/GameObject/BallWrapper.h"
#include "bakkesmod/wrappers/GameObject/CameraWrapper.h"
#include "bakkesmod/wrappers/GameObject/CarWrapper.h"
#include "bakkesmod/wrappers/GameObject/CarComponent/BoostWrapper.h"
#include "bakkesmod/wrappers/wrapperstructs.h"

#include "imgui/imgui.h"

#include <cmath>
#include <cstdio>
#include <mutex>
#include <sstream>
#include <string>

BAKKESMOD_PLUGIN(BounceChecker, "Bounce Checker - bounce prediction & training", "2.0.0", PLUGINTYPE_FREEPLAY | PLUGINTYPE_CUSTOM_TRAINING)

namespace {

bounce::Vec3 ToVec(const Vector& v) { return {v.X, v.Y, v.Z}; }
Vector V(const bounce::Vec3& v) { return Vector(v.x, v.y, v.z); }

std::mutex g_statusMutex;

constexpr int kPresetSlots = 5;

}  // namespace

// --------------------------------------------------------------------------------------
// Setup
// --------------------------------------------------------------------------------------

void BounceChecker::onLoad() {
    // Overlay
    cvarManager->registerCvar("bounce_enabled", "1", "Show the bounce prediction line", true, true, 0, true, 1).bindTo(enabled_);
    cvarManager->registerCvar("bounce_seconds", "3.0", "How many seconds ahead to predict", true, true, 0.5f, true, 8.f).bindTo(predictSeconds_);
    cvarManager->registerCvar("bounce_max_bounces", "3", "How many bounce boxes to draw", true, true, 1, true, 10).bindTo(maxBounces_);
    cvarManager->registerCvar("bounce_line_width", "3.0", "Thickness of the red line", true, true, 1.f, true, 10.f).bindTo(lineWidth_);
    cvarManager->registerCvar("bounce_show_timer", "1", "Show time-until-bounce above each box", true, true, 0, true, 1).bindTo(showTimer_);

    // Training
    cvarManager->registerCvar("bounce_unlimited_boost", "0", "Unlimited boost (Freeplay/Training only)", true, true, 0, true, 1).bindTo(unlimitedBoost_);
    cvarManager->registerCvar("bounce_variance", "0", "Random spread on drills and launches (0-1)", true, true, 0, true, 1).bindTo(variance_);
    cvarManager->registerCvar("bounce_launch_x", "800", "Launch position X (forward if relative)", true, true, -5000, true, 5000).bindTo(launchX_);
    cvarManager->registerCvar("bounce_launch_y", "0", "Launch position Y (right if relative)", true, true, -5000, true, 5000).bindTo(launchY_);
    cvarManager->registerCvar("bounce_launch_z", "300", "Launch height", true, true, 100, true, 1950).bindTo(launchZ_);
    cvarManager->registerCvar("bounce_launch_speed", "1200", "Launch speed", true, true, 0, true, 6000).bindTo(launchSpeed_);
    cvarManager->registerCvar("bounce_launch_yaw", "180", "Launch direction in degrees (180 = toward you if relative)", true, true, -180, true, 180).bindTo(launchYaw_);
    cvarManager->registerCvar("bounce_launch_pitch", "35", "Launch angle upward in degrees", true, true, -89, true, 89).bindTo(launchPitch_);
    cvarManager->registerCvar("bounce_launch_relative", "1", "Launch relative to your car", true, true, 0, true, 1).bindTo(launchRelative_);
    for (int i = 1; i <= kPresetSlots; ++i)
        cvarManager->registerCvar("bounce_preset_" + std::to_string(i), "", "Saved launch preset", false);

    // Commands - all bindable, e.g.  bind F3 bounce_toggle
    auto notifier = [this](const std::string& name, const std::string& desc, std::function<void()> fn) {
        cvarManager->registerNotifier(name, [fn](std::vector<std::string>) { fn(); }, desc, PERMISSION_ALL);
    };
    notifier("bounce_toggle", "Toggle the bounce prediction overlay", [this] {
        auto cv = cvarManager->getCvar("bounce_enabled");
        cv.setValue(!cv.getBoolValue());
    });
    notifier("bounce_boost_toggle", "Toggle unlimited boost", [this] {
        auto cv = cvarManager->getCvar("bounce_unlimited_boost");
        cv.setValue(!cv.getBoolValue());
    });
    notifier("bounce_reset_ball", "Ball back to centre", [this] { RunDrill(Drill::BallReset); });
    notifier("bounce_ball_to_car", "Teleport ball in front of your car", [this] { RunDrill(Drill::BallToCar); });
    notifier("bounce_car_to_ball", "Teleport your car behind the ball", [this] { RunDrill(Drill::CarToBall); });
    notifier("bounce_launch", "Launch the ball using the custom launch settings", [this] { RunDrill(Drill::CustomLaunch); });
    notifier("bounce_musty", "Musty flick drill", [this] { RunDrill(Drill::Musty); });
    notifier("bounce_air_dribble", "Air dribble drill", [this] { RunDrill(Drill::AirDribble); });
    notifier("bounce_ceiling", "Ceiling shot drill", [this] { RunDrill(Drill::Ceiling); });
    notifier("bounce_repeat", "Repeat the last drill", [this] { RunDrill(lastDrill_); });

    gameWrapper->RegisterDrawable([this](CanvasWrapper canvas) { Render(canvas); });
    gameWrapper->HookEventWithCaller<CarWrapper>("Function TAGame.Car_TA.SetVehicleInput",
                                                 [this](CarWrapper car, void*, std::string) { OnCarTick(car); });

    cvarManager->log("BounceChecker loaded. Open the menu with F2 -> Plugins -> Bounce Checker.");
}

void BounceChecker::onUnload() {
    gameWrapper->UnregisterDrawables();
    gameWrapper->UnhookEvent("Function TAGame.Car_TA.SetVehicleInput");
}

void BounceChecker::SetImGuiContext(uintptr_t ctx) {
    ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}

// --------------------------------------------------------------------------------------
// Where things are allowed
// --------------------------------------------------------------------------------------

bool BounceChecker::CanDrawOverlay() const {
    if (!*enabled_) return false;
    if (gameWrapper->IsInOnlineGame()) return false;
    return gameWrapper->IsInFreeplay() || gameWrapper->IsInCustomTraining() || gameWrapper->IsInGame();
}

bool BounceChecker::TrainingAllowed() const {
    if (gameWrapper->IsInOnlineGame()) return false;
    return gameWrapper->IsInFreeplay() || gameWrapper->IsInCustomTraining();
}

// --------------------------------------------------------------------------------------
// Training tools
// --------------------------------------------------------------------------------------

void BounceChecker::OnCarTick(CarWrapper car) {
    if (!*unlimitedBoost_ || car.IsNull() || !TrainingAllowed()) return;
    BoostWrapper boost = car.GetBoostComponent();
    if (boost.IsNull()) return;
    const float max = boost.GetMaxBoostAmount() > 0.f ? boost.GetMaxBoostAmount() : 1.f;
    boost.SetCurrentBoostAmount(max);
}

bounce::LaunchSettings BounceChecker::CurrentLaunch() const {
    bounce::LaunchSettings ls;
    ls.position = {*launchX_, *launchY_, *launchZ_};
    ls.speed = *launchSpeed_;
    ls.yawDeg = *launchYaw_;
    ls.pitchDeg = *launchPitch_;
    ls.relativeToCar = *launchRelative_;
    return ls;
}

void BounceChecker::QueueDrill(Drill drill) {
    gameWrapper->Execute([this, drill](GameWrapper*) { RunDrill(drill); });
}

void BounceChecker::RunDrill(Drill drill) {
    auto setStatus = [this](const std::string& s) {
        std::lock_guard<std::mutex> lock(g_statusMutex);
        status_ = s;
    };
    if (drill == Drill::None) return;
    if (!TrainingAllowed()) {
        setStatus("Training tools only work in Freeplay or Custom Training.");
        return;
    }
    ServerWrapper server = gameWrapper->GetGameEventAsServer();
    if (server.IsNull()) return;
    BallWrapper ball = server.GetBall();
    CarWrapper car = gameWrapper->GetLocalCar();
    if (ball.IsNull() || car.IsNull()) {
        setStatus("No ball or car found.");
        return;
    }

    bounce::CarState carState{ToVec(car.GetLocation()), car.GetRotation().Yaw};
    bounce::Scenario s;
    const char* name = "";
    switch (drill) {
        case Drill::BallReset:    s = bounce::BallReset(); name = "Ball reset"; break;
        case Drill::BallToCar:    s = bounce::BallToCar(carState); name = "Ball teleported to you"; break;
        case Drill::CarToBall:    s = bounce::CarToBall(ToVec(ball.GetLocation()), ToVec(ball.GetVelocity())); name = "You teleported to the ball"; break;
        case Drill::CustomLaunch: s = bounce::CustomLaunch(carState, CurrentLaunch()); name = "Ball launched"; break;
        case Drill::Musty:        s = bounce::MustyFlick(carState); name = "Musty flick drill"; break;
        case Drill::AirDribble:   s = bounce::AirDribble(carState); name = "Air dribble drill"; break;
        case Drill::Ceiling:      s = bounce::CeilingShot(carState); name = "Ceiling shot drill"; break;
        default: return;
    }
    if (drill == Drill::CustomLaunch || drill == Drill::Musty || drill == Drill::AirDribble || drill == Drill::Ceiling)
        bounce::ApplyVariance(s, *variance_, rng_);

    if (s.moveCar) {
        car.SetLocation(V(s.car.position));
        car.SetCarRotation(Rotator(s.car.pitch, s.car.yaw, s.car.roll));
        car.SetVelocity(V(s.car.velocity));
        car.SetAngularVelocity(Vector(0, 0, 0), false);
        BoostWrapper boost = car.GetBoostComponent();
        if (!boost.IsNull()) boost.SetCurrentBoostAmount(boost.GetMaxBoostAmount() > 0.f ? boost.GetMaxBoostAmount() : 1.f);
    }
    ball.SetLocation(V(s.ballPosition));
    ball.SetVelocity(V(s.ballVelocity));
    ball.SetAngularVelocity(V(s.ballAngularVelocity), false);

    if (drill != Drill::BallReset && drill != Drill::BallToCar && drill != Drill::CarToBall) lastDrill_ = drill;
    setStatus(name);
}

void BounceChecker::SavePreset(int slot) {
    std::ostringstream out;
    out << *launchX_ << ',' << *launchY_ << ',' << *launchZ_ << ',' << *launchSpeed_ << ',' << *launchYaw_ << ','
        << *launchPitch_ << ',' << (*launchRelative_ ? 1 : 0);
    cvarManager->getCvar("bounce_preset_" + std::to_string(slot)).setValue(out.str());
}

void BounceChecker::LoadPreset(int slot) {
    const std::string raw = cvarManager->getCvar("bounce_preset_" + std::to_string(slot)).getStringValue();
    std::istringstream in(raw);
    float vals[7];
    char comma;
    for (int i = 0; i < 7; ++i) {
        if (!(in >> vals[i])) return;  // empty or broken slot
        if (i < 6) in >> comma;
    }
    const char* names[] = {"bounce_launch_x", "bounce_launch_y", "bounce_launch_z", "bounce_launch_speed",
                           "bounce_launch_yaw", "bounce_launch_pitch", "bounce_launch_relative"};
    for (int i = 0; i < 6; ++i) cvarManager->getCvar(names[i]).setValue(vals[i]);
    cvarManager->getCvar(names[6]).setValue(vals[6] != 0.f ? 1 : 0);
}

// --------------------------------------------------------------------------------------
// Menu  (render thread: game calls go through gameWrapper->Execute)
// --------------------------------------------------------------------------------------

void BounceChecker::RenderSettings() {
    auto checkbox = [this](const char* label, const char* cvar, const std::shared_ptr<bool>& value) {
        bool v = *value;
        if (ImGui::Checkbox(label, &v)) cvarManager->getCvar(cvar).setValue(v ? 1 : 0);
    };
    auto sliderF = [this](const char* label, const char* cvar, const std::shared_ptr<float>& value, float lo, float hi, const char* fmt) {
        float v = *value;
        if (ImGui::SliderFloat(label, &v, lo, hi, fmt)) cvarManager->getCvar(cvar).setValue(v);
    };
    auto sliderI = [this](const char* label, const char* cvar, const std::shared_ptr<int>& value, int lo, int hi) {
        int v = *value;
        if (ImGui::SliderInt(label, &v, lo, hi)) cvarManager->getCvar(cvar).setValue(v);
    };
    auto drillButton = [this](const char* label, Drill d, const char* command, const char* tip) {
        if (ImGui::Button(label, ImVec2(170, 0))) QueueDrill(d);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s\nBind a key: bind <key> %s", tip, command);
    };

    ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "Works in Freeplay and Custom Training. Disabled in online matches.");
    {
        std::lock_guard<std::mutex> lock(g_statusMutex);
        if (!status_.empty()) ImGui::Text("Last: %s", status_.c_str());
    }
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Bounce prediction overlay", ImGuiTreeNodeFlags_DefaultOpen)) {
        checkbox("Show red line and bounce boxes", "bounce_enabled", enabled_);
        sliderF("Predict ahead (seconds)", "bounce_seconds", predictSeconds_, 0.5f, 8.f, "%.1f s");
        sliderI("Bounce boxes", "bounce_max_bounces", maxBounces_, 1, 10);
        sliderF("Line width", "bounce_line_width", lineWidth_, 1.f, 10.f, "%.1f");
        checkbox("Show time-to-bounce", "bounce_show_timer", showTimer_);
    }

    if (ImGui::CollapsingHeader("Unlimited boost", ImGuiTreeNodeFlags_DefaultOpen)) {
        checkbox("Unlimited boost", "bounce_unlimited_boost", unlimitedBoost_);
        ImGui::SameLine();
        ImGui::TextDisabled("(bind <key> bounce_boost_toggle)");
    }

    if (ImGui::CollapsingHeader("Ball reset & teleport", ImGuiTreeNodeFlags_DefaultOpen)) {
        drillButton("Reset ball to centre", Drill::BallReset, "bounce_reset_ball", "Ball goes back to the kickoff spot and stops.");
        ImGui::SameLine();
        drillButton("Ball to me", Drill::BallToCar, "bounce_ball_to_car", "Ball appears floating in front of your car.");
        ImGui::SameLine();
        drillButton("Me to ball", Drill::CarToBall, "bounce_car_to_ball", "Your car appears behind the ball, facing where it's going.");
    }

    if (ImGui::CollapsingHeader("Custom ball launch", ImGuiTreeNodeFlags_DefaultOpen)) {
        checkbox("Relative to my car", "bounce_launch_relative", launchRelative_);
        ImGui::TextDisabled(*launchRelative_ ? "X = forward, Y = right, direction 180 = toward you"
                                             : "World coordinates, direction 90 = toward orange goal");
        sliderF("X", "bounce_launch_x", launchX_, -5000.f, 5000.f, "%.0f");
        sliderF("Y", "bounce_launch_y", launchY_, -5000.f, 5000.f, "%.0f");
        sliderF("Height", "bounce_launch_z", launchZ_, 100.f, 1950.f, "%.0f");
        sliderF("Speed", "bounce_launch_speed", launchSpeed_, 0.f, 6000.f, "%.0f uu/s");
        sliderF("Direction", "bounce_launch_yaw", launchYaw_, -180.f, 180.f, "%.0f deg");
        sliderF("Upward angle", "bounce_launch_pitch", launchPitch_, -89.f, 89.f, "%.0f deg");
        drillButton("LAUNCH", Drill::CustomLaunch, "bounce_launch", "Fire the ball with these settings.");

        ImGui::Text("Presets:");
        for (int i = 1; i <= kPresetSlots; ++i) {
            ImGui::PushID(i);
            const bool saved = !cvarManager->getCvar("bounce_preset_" + std::to_string(i)).getStringValue().empty();
            ImGui::Text("%d", i);
            ImGui::SameLine();
            if (ImGui::Button("Save")) SavePreset(i);
            ImGui::SameLine();
            if (!saved) ImGui::TextDisabled("(empty)");
            else if (ImGui::Button("Load")) LoadPreset(i);
            if (i < kPresetSlots) ImGui::SameLine(0, 30);
            ImGui::PopID();
        }
    }

    if (ImGui::CollapsingHeader("Mechanic drills", ImGuiTreeNodeFlags_DefaultOpen)) {
        drillButton("Musty flick", Drill::Musty, "bounce_musty",
                    "Ball on your roof while driving. Jump, tilt back, then flip backward to flick it.");
        ImGui::SameLine();
        drillButton("Air dribble", Drill::AirDribble, "bounce_air_dribble",
                    "Ball popped up in front of you. Jump, boost, keep it on your nose.");
        ImGui::SameLine();
        drillButton("Ceiling shot", Drill::Ceiling, "bounce_ceiling",
                    "You drive up the nearest wall. Get on the ceiling, drop off (you keep your flip), hit the ball.");
        if (ImGui::Button("Repeat last drill", ImVec2(170, 0))) QueueDrill(lastDrill_);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Bind a key: bind <key> bounce_repeat");
        sliderF("Random variance", "bounce_variance", variance_, 0.f, 1.f, "%.2f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Randomises ball speed and direction a little each rep.");
    }
}

// --------------------------------------------------------------------------------------
// Overlay drawing
// --------------------------------------------------------------------------------------

void BounceChecker::Render(CanvasWrapper canvas) {
    if (!CanDrawOverlay()) return;

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
    auto inFront = [&](const bounce::Vec3& p) { return Vector::dot(V(p) - camLoc, camFwd) > 10.f; };

    const LinearColor red{255.f, 0.f, 0.f, 255.f};
    const LinearColor redFill{255.f, 0.f, 0.f, 70.f};
    const float width = *lineWidth_;

    // 1. red trajectory line
    canvas.SetColor(red);
    const size_t stride = 2;
    for (size_t i = stride; i < pred.path.size(); i += stride) {
        const auto& a = pred.path[i - stride];
        const auto& b = pred.path[i];
        if (!inFront(a) || !inFront(b)) continue;
        canvas.DrawLine(canvas.ProjectF(V(a)), canvas.ProjectF(V(b)), width);
    }

    // 2. red box on each bounce spot
    int drawn = 0;
    for (const auto& bnc : pred.bounces) {
        if (drawn >= *maxBounces_) break;
        ++drawn;

        const bounce::Vec3& n = bnc.normal;
        bounce::Vec3 t1 = std::fabs(n.z) > 0.9f ? bounce::Vec3(1, 0, 0) : bounce::Vec3(0, 0, 1);
        t1 = bounce::Cross(n, t1);
        t1 = t1 * (1.f / t1.Length());
        const bounce::Vec3 t2 = bounce::Cross(n, t1);

        const float half = bounce::constants::kBallRadius;
        const bounce::Vec3 c = bnc.contact + n * 2.f;
        const bounce::Vec3 corners[4] = {c + t1 * half + t2 * half, c - t1 * half + t2 * half,
                                         c - t1 * half - t2 * half, c + t1 * half - t2 * half};
        bool visible = true;
        for (const auto& corner : corners) visible = visible && inFront(corner);
        if (!visible) continue;

        Vector2F sc[4];
        for (int k = 0; k < 4; ++k) sc[k] = canvas.ProjectF(V(corners[k]));

        canvas.SetColor(redFill);
        canvas.FillTriangle(sc[0], sc[1], sc[2]);
        canvas.FillTriangle(sc[0], sc[2], sc[3]);

        canvas.SetColor(red);
        const float boxWidth = drawn == 1 ? width + 2.f : width;
        for (int k = 0; k < 4; ++k) canvas.DrawLine(sc[k], sc[(k + 1) % 4], boxWidth);
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
