#pragma once

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/wrappers/canvaswrapper.h"

#include "BallPredictor.h"

#include <memory>

class BounceChecker : public BakkesMod::Plugin::BakkesModPlugin {
public:
    void onLoad() override;
    void onUnload() override;

private:
    void Render(CanvasWrapper canvas);
    bool CanDraw() const;

    std::shared_ptr<bool> enabled_ = std::make_shared<bool>(true);
    std::shared_ptr<float> predictSeconds_ = std::make_shared<float>(3.0f);
    std::shared_ptr<int> maxBounces_ = std::make_shared<int>(3);
    std::shared_ptr<float> lineWidth_ = std::make_shared<float>(3.0f);
    std::shared_ptr<bool> showTimer_ = std::make_shared<bool>(true);
};
