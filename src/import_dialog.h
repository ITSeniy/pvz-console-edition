#pragma once
#include "game_edition.h"
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/windowed_app_context.h>
#include <rex/logging.h>
#include <imgui.h>
#include <algorithm>
#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

// First launch without game data: import the player's own XBLA package
// before the runtime loads default.xex (ReXApp::OnFinalizePaths). The
// package is checked and extracted on a worker thread; the player then says
// whether it is a purchased full copy, which only selects Trial or Full.
class ImportDialog final : public rex::ui::ImGuiDialog {
 public:
  using ChooseFile = std::function<std::filesystem::path()>;
  ImportDialog(rex::ui::ImGuiDrawer* drawer, rex::ui::WindowedAppContext& app,
      std::filesystem::path game_data_root, ChooseFile choose,
      std::function<void()> finish, std::function<void()> quit, std::string auto_answer = {})
      : ImGuiDialog(drawer), app_(app), root_(std::move(game_data_root)), choose_(std::move(choose)),
        finish_(std::move(finish)), quit_(std::move(quit)), auto_answer_(std::move(auto_answer)) {
    imgui_drawer()->AddDialog(this);
  }
  ~ImportDialog() override { *alive_ = false; }
  void Hide() { imgui_drawer()->RemoveDialog(this); }

 private:
  enum class Step { Intro, Working, Edition, Done };
  struct State {
    std::mutex mutex;
    Step step = Step::Intro;
    std::string error, package_sha256;
    bool disc = false; // owning the disc implies the full game
  };

  void Choose() {
    // The Win32 file dialog runs its own message loop; leave ImGui's frame first.
    app_.CallInUIThreadDeferred([this, alive = alive_] {
      if (!*alive) return;
      const auto package = choose_();
      if (package.empty()) return;
      { std::lock_guard lock(state_->mutex); state_->step = Step::Working; state_->error.clear(); }
      REXLOG_INFO("PvZ first-run import: checking game package");
      std::thread([state = state_, package, root = root_] {
        const auto result = pvz::ImportGamePackage(package, root);
        REXLOG_INFO("PvZ first-run import: {}", result.ok ? "game files extracted" : result.error);
        std::lock_guard lock(state->mutex);
        state->step = result.ok ? Step::Edition : Step::Intro;
        state->error = result.error;
        state->package_sha256 = result.package_sha256;
        state->disc = result.disc;
      }).detach();
    });
  }
  void Record(bool full) {
    std::string sha;
    { std::lock_guard lock(state_->mutex); sha = state_->package_sha256; state_->step = Step::Done; }
    const bool disc = [&] { std::lock_guard lock(state_->mutex); return state_->disc; }();
    const auto edition = pvz::RecordImport(root_, sha, full, disc);
    REXLOG_INFO("PvZ first-run import: {} ({})", edition.full ? "full" : "trial", edition.reason);
    if (full != edition.full) {
      std::lock_guard lock(state_->mutex);
      state_->step = Step::Edition;
      state_->error = "The import receipt cannot be written to the game folder.";
      return;
    }
    // finish destroys this dialog; call a copy, not the member.
    app_.CallInUIThreadDeferred([finish = finish_, alive = alive_] { if (*alive) finish(); });
  }

  void OnDraw(ImGuiIO& io) override {
    Step step;
    std::string error;
    { std::lock_guard lock(state_->mutex); step = state_->step; error = state_->error; }
    if (step == Step::Done) return;
    // Replay-only (pvz_import_answer): choose the file and answer unattended.
    if (!auto_answer_.empty() && step == Step::Intro && !auto_started_) { auto_started_ = true; Choose(); }
    bool disc = false;
    { std::lock_guard lock(state_->mutex); disc = state_->disc; }
    if (step == Step::Edition && disc && error.empty()) { Record(true); return; }
    if (!auto_answer_.empty() && step == Step::Edition && error.empty()) { Record(auto_answer_ == "full"); return; }
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x / 2, io.DisplaySize.y / 2), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    // Readable on large displays: scale with the window height (720p = 1).
    const float k = std::clamp(io.DisplaySize.y / 720.f, 1.f, 3.f);
    ImGui::SetNextWindowSize(ImVec2(560 * k, 0), ImGuiCond_Always);
    constexpr auto flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize;
    if (ImGui::Begin("Plants vs. Zombies - Game Files", nullptr, flags)) {
      ImGui::SetWindowFontScale(k);
      if (step == Step::Intro) {
        ImGui::TextWrapped("This port needs the game files from your own copy of Plants vs. Zombies "
            "for Xbox 360 (Xbox LIVE Arcade, TitleID 584109FF).");
        ImGui::Spacing();
        ImGui::TextWrapped("Choose the game package (the file inside the 584109FF\\000D0000 folder "
            "of your Xbox storage) or an image (.iso) of an Xbox 360 disc with the game. "
            "Its files are checked and copied once. A disc unlocks the full game.");
        if (!error.empty()) {
          ImGui::Spacing();
          ImGui::TextColored(ImVec4(1.f, 0.45f, 0.4f, 1.f), "%s", error.c_str());
        }
        ImGui::Spacing();
        if (ImGui::Button("Choose game package...", ImVec2(220 * k, 32 * k))) Choose();
        ImGui::SameLine();
        if (ImGui::Button("Quit", ImVec2(100 * k, 32 * k))) quit_();
      } else if (step == Step::Working) {
        ImGui::TextWrapped("Checking and copying the game files...");
      } else {
        ImGui::TextWrapped("The game files are ready.");
        ImGui::Spacing();
        ImGui::TextWrapped("Is this the full version that you bought? The trial and the full "
            "version use the same package, so only your answer tells them apart.");
        if (!error.empty()) {
          ImGui::Spacing();
          ImGui::TextColored(ImVec4(1.f, 0.45f, 0.4f, 1.f), "%s", error.c_str());
        }
        ImGui::Spacing();
        if (ImGui::Button("Yes, my full version", ImVec2(200 * k, 32 * k))) Record(true);
        ImGui::SameLine();
        if (ImGui::Button("No, play the trial", ImVec2(200 * k, 32 * k))) Record(false);
        ImGui::TextDisabled("You can unlock the full version later with $ in the main menu.");
      }
    }
    ImGui::End();
  }

  rex::ui::WindowedAppContext& app_;
  std::filesystem::path root_;
  ChooseFile choose_;
  std::function<void()> finish_, quit_;
  std::string auto_answer_;
  bool auto_started_ = false;
  std::shared_ptr<State> state_ = std::make_shared<State>();
  std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};
