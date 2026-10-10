#pragma once
#include <array>
#include <cstdint>
#include <limits>

namespace pvz {
// Stable physical IDs survive re-enumeration and disconnection. Never promote
// the other player when a controller is unplugged.
class LocalPlayers {
 public:
  using Key = uint64_t;
  static constexpr Key none = 0, mouse = std::numeric_limits<Key>::max();
  void Reset() { active_ = false; players_ = {}; }
  void Begin(Key first) { active_ = true; players_ = {first ? first : mouse, none}; }
  bool Active() const { return active_; }
  int User(Key key) const {
    for (int i = 0; i < 2; ++i) if (key && players_[i] == key) return i;
    return -1;
  }
  int Observe(Key key) {
    last_ = key;
    if (!active_) return 0;
    if (const auto user = User(key); user >= 0) return user;
    if (!players_[1] && key && key != players_[0]) { players_[1] = key; return 1; }
    return -1;
  }
  bool Accepts(int user, Key key) const {
    if (user < 0 || user > 1 || !key) return false;
    if (!active_) return user == 0;
    return players_[user] ? players_[user] == key : user == 1 && key != players_[0];
  }
  Key FirstDevice() const { return last_; }
  Key Player(int user) const { return players_[user]; }
 private:
  bool active_ = false;
  std::array<Key, 2> players_{};
  Key last_ = none;
};
}
