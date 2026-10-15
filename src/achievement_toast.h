#pragma once
#include <rex/ui/overlay/achievement_toast.h>
#include <rex/ui/windowed_app_context.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>

// The SDK toast stays registered with the ImGui drawer for the whole session.
// Any registered dialog makes the presenter paint from the UI thread on every
// display refresh, besides each game frame; in independent flip those extra
// presents take refreshes from game frames and the picture stutters. This
// toast registers only while a notification is on screen, so the rest of the
// time the presenter shows each game frame once, from the GPU thread.
class AchievementToast final : public rex::ui::AchievementNotificationDialog {
 public:
  AchievementToast(rex::ui::WindowedAppContext& app, rex::ui::ImGuiDrawer* drawer,
      rex::ui::ImmediateDrawer* immediate, rex::Runtime* runtime)
      : AchievementNotificationDialog(drawer), app_(app),
        toast_(std::make_unique<rex::ui::AchievementToastDialog>(drawer, immediate, runtime)) {
    drawer->RemoveDialog(toast_.get());
    drawer->RemoveDialog(this);
  }
  ~AchievementToast() override { alive_->store(false); }

  void Push(const rex::system::AchievementEvent& event) override {
    toast_->Push(event);
    app_.CallInUIThread([this, alive = alive_] {
      if (!alive->load()) return;
      // The SDK shows queued toasts one after another, 4.5 s each.
      until_ = std::max(until_, Clock::now()) + std::chrono::seconds(5);
      imgui_drawer()->AddDialog(this);
    });
  }

 private:
  using Clock = std::chrono::steady_clock;
  void OnDraw(ImGuiIO&) override {
    toast_->Draw();
    if (Clock::now() >= until_) imgui_drawer()->RemoveDialog(this);
  }
  rex::ui::WindowedAppContext& app_;
  std::unique_ptr<rex::ui::AchievementToastDialog> toast_;
  std::shared_ptr<std::atomic_bool> alive_ = std::make_shared<std::atomic_bool>(true);
  Clock::time_point until_{};
};
