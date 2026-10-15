#include "input_backend.h"
#include "bridge_mnk_driver.h"
#include "input_activity.h"
#include "mouse_bridge.h"
#include "controller_assets.h"
#include "local_players.h"
#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/input/input_system.h>
#include <rex/input/sdl/sdl_input_driver.h>
#include <rex/input/xinput/xinput_input_driver.h>
#include <rex/input/mnk/mnk_input_driver.h>
#include <rex/input/nop/nop_input_driver.h>
#include <unordered_map>
#include <algorithm>
#include <atomic>
#include <mutex>

using rex::X_STATUS;
using rex::X_RESULT;

REXCVAR_DEFINE_BOOL(pvz_replay_controller, false, "Diagnostics", "Use a diagnostic pad state source during an explicit input replay");
namespace {
std::mutex replay_pad_mutex;
std::array<pvz::PadSample, 2> replay_pad{};
std::array<uint32_t, 2> replay_packet{};
std::mutex players_mutex;
pvz::LocalPlayers players;
// PlayStation pads by device, for the glyphs of each local player.
std::unordered_map<uint64_t, bool> sony_devices;
bool SonyDevice(const rex::input::DeviceInfo& device) {
  auto name = device.name;
  std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return char(std::tolower(c)); });
  return (device.guid.size() >= 12 && device.guid.substr(8, 4) == "4c05") ||
      name.find("dualshock") != std::string::npos || name.find("dualsense") != std::string::npos ||
      name.find("ps3") != std::string::npos || name.find("ps4") != std::string::npos || name.find("ps5") != std::string::npos;
}
constexpr uint64_t replay_id = 0x50565A52504C;
using namespace rex::input;
class ReplayPad final : public InputDriver {
 public:
  ReplayPad() : InputDriver(nullptr, 0) {}
  X_STATUS Setup() override { return X_STATUS_SUCCESS; }
  void EnumerateDevices(std::vector<DeviceInfo>& out) override {
    for (int i = 0; i < 2; ++i) {
      DeviceInfo info; info.id = static_cast<DeviceId>(replay_id + i); info.name = "PvZ regression pad " + std::to_string(i + 1);
      out.push_back(info);
    }
  }
  X_RESULT GetDeviceState(DeviceId id, X_INPUT_STATE* out) override {
    if (!out) return X_ERROR_EMPTY;
    std::lock_guard lock(replay_pad_mutex);
    const auto index = uint64_t(id) - replay_id;
    if (index >= 2) return X_ERROR_DEVICE_NOT_CONNECTED;
    *out = {}; out->packet_number = replay_packet[index];
    if (!is_active()) return X_ERROR_SUCCESS;
    auto& g = out->gamepad; const auto& sample = replay_pad[index]; g.buttons = sample.buttons;
    g.thumb_lx = int16_t(sample.axes[0]); g.thumb_ly = int16_t(sample.axes[1]);
    g.thumb_rx = int16_t(sample.axes[2]); g.thumb_ry = int16_t(sample.axes[3]);
    g.left_trigger = uint8_t(sample.axes[4]); g.right_trigger = uint8_t(sample.axes[5]);
    return X_ERROR_SUCCESS;
  }
  X_RESULT GetDeviceCapabilities(DeviceId, uint32_t, X_INPUT_CAPABILITIES* out) override {
    if (out) { *out = {}; out->type = 1; out->sub_type = 1; out->gamepad.buttons = 0xFFFF; }
    return X_ERROR_SUCCESS;
  }
  X_RESULT SetDeviceVibration(DeviceId, X_INPUT_VIBRATION*) override { return X_ERROR_SUCCESS; }
  X_RESULT GetDeviceKeystroke(DeviceId, uint32_t, X_INPUT_KEYSTROKE*) override { return X_ERROR_EMPTY; }
};
struct PrimaryPad { std::atomic<DeviceId> id{DeviceId::kInvalid}; };
class Assignment final : public DeviceAssignment {
 public:
  explicit Assignment(std::shared_ptr<PrimaryPad> pad) : pad_(std::move(pad)) {}
  void OnDevicesChanged(const std::vector<DeviceInfo>& devices) override {
    std::lock_guard lock(players_mutex);
    devices_ = devices;
    pad_->id = DeviceId::kInvalid;
    pvz::PublishSonyController(false);
    sony_devices.clear();
    for (const auto& device : devices) {
      if (device.synthetic) continue;
      sony_devices[uint64_t(device.id)] = SonyDevice(device);
      if (device.ordinal == 0) {
        pad_->id = device.id;
        pvz::PublishSonyController(SonyDevice(device));
      }
    }
  }
  void DevicesForUser(uint32_t user, std::vector<DeviceId>& out) const override {
    std::lock_guard lock(players_mutex);
    out.clear();
    for (const auto& device : devices_) {
      const auto key = device.synthetic ? pvz::LocalPlayers::mouse : uint64_t(device.id);
      if (players.Accepts(int(user), key)) out.push_back(device.id);
    }
  }
 private:
  std::vector<DeviceInfo> devices_;
  std::shared_ptr<PrimaryPad> pad_;
};

class ObservedDriver final : public InputDriver {
 public:
  ObservedDriver(std::unique_ptr<InputDriver> inner, std::shared_ptr<PrimaryPad> pad)
      : InputDriver(nullptr, 0), inner_(std::move(inner)), pad_(std::move(pad)) {
    inner_->set_is_active_callback([this] { return is_active(); });
  }
  X_STATUS Setup() override { return inner_->Setup(); }
  void OnWindowAvailable(rex::ui::Window* w) override { inner_->OnWindowAvailable(w); }
  void EnumerateDevices(std::vector<DeviceInfo>& out) override {
    inner_->EnumerateDevices(out);
    std::lock_guard lock(activity_mutex_);
    std::erase_if(activity_, [&](const auto& entry) {
      return std::none_of(out.begin(), out.end(), [&](const auto& d) { return d.id == entry.first; });
    });
  }
  X_RESULT GetDeviceState(DeviceId id, X_INPUT_STATE* out) override {
    const auto result = inner_->GetDeviceState(id, out);
    if (result == X_ERROR_SUCCESS && out && is_active()) {
      const auto& g = out->gamepad;
      pvz::PadSample sample{uint16_t(g.buttons), {int16_t(g.thumb_lx), int16_t(g.thumb_ly),
          int16_t(g.thumb_rx), int16_t(g.thumb_ry), g.left_trigger, g.right_trigger}};
      std::lock_guard lock(activity_mutex_);
      const bool activity = activity_[id].Observe(sample);
      bool may_switch = true, may_rumble = false, accepted = true;
      {
        std::lock_guard players_lock(players_mutex);
        if (activity) players.Observe(uint64_t(id));
        may_switch = !players.Active() || players.User(pvz::LocalPlayers::mouse) < 0;
        accepted = !players.Active() || players.User(uint64_t(id)) >= 0 || !players.Player(1);
        may_rumble = players.Active() ? players.User(uint64_t(id)) >= 0 :
            !pvz::MouseOwnsInput() && players.FirstDevice() == uint64_t(id);
      }
      if (activity && may_switch) pvz::PublishControllerActivity();
      if (!may_rumble && !quiet_[id]) {
        X_INPUT_VIBRATION quiet{};
        inner_->SetDeviceVibration(id, &quiet);
        quiet_[id] = true;
      } else if (may_rumble) quiet_[id] = false;
      if (!accepted) { *out = {}; return X_ERROR_DEVICE_NOT_CONNECTED; }
    }
    return result;
  }
  X_RESULT GetDeviceCapabilities(DeviceId id, uint32_t flags, X_INPUT_CAPABILITIES* out) override {
    return inner_->GetDeviceCapabilities(id, flags, out);
  }
  X_RESULT SetDeviceVibration(DeviceId id, X_INPUT_VIBRATION* v) override {
    bool allowed;
    { std::lock_guard lock(players_mutex); allowed = players.Active() ? players.User(uint64_t(id)) >= 0 :
        !pvz::MouseOwnsInput() && players.FirstDevice() == uint64_t(id); }
    if (!allowed) {
      X_INPUT_VIBRATION quiet{};
      return inner_->SetDeviceVibration(id, &quiet);
    }
    return inner_->SetDeviceVibration(id, v);
  }
  X_RESULT GetDeviceKeystroke(DeviceId id, uint32_t flags, X_INPUT_KEYSTROKE* out) override {
    X_INPUT_STATE state{};
    if (GetDeviceState(id, &state) != X_ERROR_SUCCESS) return X_ERROR_DEVICE_NOT_CONNECTED;
    return inner_->GetDeviceKeystroke(id, flags, out);
  }
 private:
  std::unique_ptr<InputDriver> inner_;
  std::shared_ptr<PrimaryPad> pad_;
  std::unordered_map<DeviceId, pvz::PadActivity> activity_;
  std::unordered_map<DeviceId, bool> quiet_;
  std::mutex activity_mutex_;
};
}

std::unique_ptr<rex::system::IInputSystem> CreatePvzInputSystem(bool tool_mode) {
  using namespace rex::input;
  auto input = std::make_unique<InputSystem>(nullptr);
  auto pad = std::make_shared<PrimaryPad>();
  if (!tool_mode) {
    std::unique_ptr<InputDriver> driver;
    if (REXCVAR_GET(pvz_replay_controller) && !rex::cvar::Query<std::string>("pvz_input_replay").empty())
      driver = std::make_unique<ReplayPad>();
    else if (rex::cvar::Query<std::string>("input_backend") == "xinput")
      driver = std::make_unique<xinput::XinputInputDriver>(nullptr, 0);
    else driver = std::make_unique<sdl::SDLInputDriver>(nullptr, 0);
    auto observed = std::make_unique<ObservedDriver>(std::move(driver), pad);
    if (observed->Setup() == X_STATUS_SUCCESS) input->AddDriver(std::move(observed));
    auto mnk = std::make_unique<pvz::BridgeMnkDriver>();
    if (mnk->Setup() == X_STATUS_SUCCESS) input->AddDriver(std::move(mnk));
  }
  input->AddDriver(std::make_unique<nop::NopInputDriver>(nullptr, tool_mode ? 0 : 1));
  input->SetDeviceAssignment(std::make_unique<Assignment>(pad));
  return input;
}

void pvz::PublishReplayPad(PadSample sample, uint32_t device) {
  if (!REXCVAR_GET(pvz_replay_controller) || rex::cvar::Query<std::string>("pvz_input_replay").empty()) return;
  if (device >= 2) return;
  std::lock_guard lock(replay_pad_mutex); replay_pad[device] = sample; ++replay_packet[device];
}
void pvz::BeginLocalPlayers(bool mouse_first) {
  std::lock_guard lock(players_mutex);
  if (players.Active()) return;
  players.Begin(mouse_first ? LocalPlayers::mouse : players.FirstDevice());
  REXLOG_INFO("PvZ local players started: player 1 device {}", players.Player(0));
}
void pvz::ResetLocalPlayers() { std::lock_guard lock(players_mutex); players.Reset(); }
bool pvz::LocalPlayersActive() { std::lock_guard lock(players_mutex); return players.Active(); }
bool pvz::LocalMouseAssigned() { std::lock_guard lock(players_mutex); return players.Active() && players.User(LocalPlayers::mouse) >= 0; }
int pvz::MouseUserIndex() {
  std::lock_guard lock(players_mutex);
  if (!players.Active()) return 0;
  const auto user = players.User(LocalPlayers::mouse);
  return user < 0 && !players.Player(1) ? 1 : user;
}
void pvz::ObserveLocalMouse() {
  std::lock_guard lock(players_mutex);
  if (players.Active() && players.User(LocalPlayers::mouse) < 0) {
    const auto user = players.Observe(LocalPlayers::mouse);
    if (user >= 0) REXLOG_INFO("PvZ local mouse joined player {}", user + 1);
  }
}
pvz::PadKind pvz::LocalPlayerPad(int user) {
  std::lock_guard lock(players_mutex);
  if (!players.Active() || user < 0 || user > 1) return PadKind::None;
  const auto key = players.Player(user);
  if (!key) return PadKind::None;
  if (key == LocalPlayers::mouse) return PadKind::Mouse;
  const auto found = sony_devices.find(key);
  return found != sony_devices.end() && found->second ? PadKind::PlayStation : PadKind::Xbox;
}
