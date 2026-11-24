#pragma once

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"
#include "bakkesmod/wrappers/canvaswrapper.h"

#include "BallPredictor.h"
#include "Scenarios.h"

#include <functional>
#include <memory>
#include <random>
#include <string>

class CarWrapper;

class BounceChecker : public BakkesMod::Plugin::BakkesModPlugin, public BakkesMod::Plugin::PluginSettingsWindow {
public:
    void onLoad() override;
    void onUnload() override;

    // F2 -> Plugins -> Bounce Checker
    void RenderSettings() override;
    std::string GetPluginName() override { return "Bounce Checker"; }
    void SetImGuiContext(uintptr_t ctx) override;

private:
    enum class Drill { None, BallReset, BallToCar, CarToBall, CustomLaunch, Musty, AirDribble, Ceiling };

    // Overlay
    void Render(CanvasWrapper canvas);
    bool CanDrawOverlay() const;

    // Training tools - only allowed in Freeplay / Custom Training.
    bool TrainingAllowed() const;
    void RunDrill(Drill drill);        // game thread only
    void QueueDrill(Drill drill);      // safe from any thread (menu buttons)
    void OnCarTick(CarWrapper car);    // unlimited boost
    void SavePreset(int slot);
    void LoadPreset(int slot);
    bounce::LaunchSettings CurrentLaunch() const;

    // Overlay settings
    std::shared_ptr<bool> enabled_ = std::make_shared<bool>(true);
    std::shared_ptr<float> predictSeconds_ = std::make_shared<float>(3.0f);
    std::shared_ptr<int> maxBounces_ = std::make_shared<int>(3);
    std::shared_ptr<float> lineWidth_ = std::make_shared<float>(3.0f);
    std::shared_ptr<bool> showTimer_ = std::make_shared<bool>(true);

    // Training settings
    std::shared_ptr<bool> unlimitedBoost_ = std::make_shared<bool>(false);
    std::shared_ptr<float> variance_ = std::make_shared<float>(0.f);
    std::shared_ptr<float> launchX_ = std::make_shared<float>(800.f);
    std::shared_ptr<float> launchY_ = std::make_shared<float>(0.f);
    std::shared_ptr<float> launchZ_ = std::make_shared<float>(300.f);
    std::shared_ptr<float> launchSpeed_ = std::make_shared<float>(1200.f);
    std::shared_ptr<float> launchYaw_ = std::make_shared<float>(180.f);
    std::shared_ptr<float> launchPitch_ = std::make_shared<float>(35.f);
    std::shared_ptr<bool> launchRelative_ = std::make_shared<bool>(true);

    Drill lastDrill_ = Drill::None;
    std::mt19937 rng_{std::random_device{}()};
    std::string status_;  // last message shown in the menu
};
