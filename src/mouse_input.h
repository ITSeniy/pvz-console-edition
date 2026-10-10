#pragma once
#include "pointer_mapping.h"
#include <algorithm>
#include <cstdint>
#include <deque>
#include <optional>

namespace pvz {
enum class MouseEventKind { Move, Button, Wheel, Cancel };
enum class MouseCancelReason { PointerLeft, FocusLost, Controller, ModeChanged, Disabled, Overflow };
struct MouseInputEvent {
  MouseEventKind kind = MouseEventKind::Move;
  std::optional<Point> point;
  int click = 0;
  bool down = false;
  int delta = 0;
  MouseCancelReason reason = MouseCancelReason::PointerLeft;
  uint64_t sequence = 0, generation = 0;
};

// The publisher and consumer serialize access externally. A cancellation
// invalidates queued input, including a batch being consumed by a nested UI loop.
// Move events are retained: Beghouled and drag thresholds depend on the path.
class MouseInputStream {
 public:
  static constexpr size_t kCapacity = 256;
  void SetMode(bool pc) {
    if (pc_ && *pc_ != pc) Cancel(MouseCancelReason::ModeChanged);
    pc_ = pc;
  }
  bool Move(std::optional<Point> point) {
    if (!point) { Cancel(MouseCancelReason::PointerLeft); return false; }
    if (!Valid(*point)) return false;
    const bool changed = !published_ || published_->x != point->x || published_->y != point->y;
    published_ = point;
    if (changed) Push({MouseEventKind::Move, point});
    return changed;
  }
  void Button(Point point, int click, bool down) {
    if (!Valid(point) || (click != 1 && click != -1 && click != 2)) return;
    published_ = point;
    Push({MouseEventKind::Button, point, click, down});
  }
  void Wheel(Point point, int delta) {
    if (!Valid(point) || !delta) return;
    published_ = point;
    MouseInputEvent event{MouseEventKind::Wheel, point};
    event.delta = std::clamp(delta, -1200, 1200);
    Push(event);
  }
  void Cancel(MouseCancelReason reason) {
    ++generation_;
    events_.clear();
    if (reason == MouseCancelReason::PointerLeft || reason == MouseCancelReason::FocusLost ||
        reason == MouseCancelReason::Disabled) published_.reset();
    // A subsequent owner/mode cancellation must not swallow focus-loss pause.
    focus_lost_ |= reason == MouseCancelReason::FocusLost;
    MouseInputEvent event{MouseEventKind::Cancel, published_};
    event.reason = reason;
    Stamp(event);
    events_.push_back(event);
  }
  std::optional<MouseInputEvent> Take() {
    if (events_.empty()) return std::nullopt;
    auto event = events_.front();
    events_.pop_front();
    return event;
  }
  bool Current(const MouseInputEvent& event) const { return event.generation == generation_; }
  bool Pending() const { return !events_.empty(); }
  bool TakeFocusLost() { const bool lost = focus_lost_; focus_lost_ = false; return lost; }
  std::optional<Point> PublishedPoint() const { return published_; }
 private:
  static bool Valid(Point point) { return std::isfinite(point.x) && std::isfinite(point.y); }
  void Stamp(MouseInputEvent& event) { event.sequence = ++sequence_; event.generation = generation_; }
  void Push(MouseInputEvent event) {
    if (events_.size() >= kCapacity) { Cancel(MouseCancelReason::Overflow); return; }
    Stamp(event);
    events_.push_back(event);
  }
  std::deque<MouseInputEvent> events_;
  std::optional<Point> published_;
  std::optional<bool> pc_;
  uint64_t sequence_ = 0, generation_ = 0;
  bool focus_lost_ = false;
};

// Only a matching press can release a custom gesture. This also blocks an
// orphan Up after cancellation/overflow from reaching console gameplay actions.
class MouseButtons {
 public:
  bool Accept(int click, bool down) {
    const unsigned mask = click == 1 ? 1 : click == -1 ? 2 : click == 2 ? 4 : 0;
    if (!mask) return false;
    if (down) {
      if (held_ & mask) return false;
      held_ |= mask;
      return true;
    }
    if (!(held_ & mask)) return false;
    held_ &= ~mask;
    return true;
  }
  void Cancel() { held_ = 0; }
 private:
  unsigned held_ = 0;
};

class MouseWheel {
 public:
  int Take(int delta, uint32_t target) {
    if (target_ != target) { remainder_ = 0; target_ = target; }
    const int total = remainder_ + std::clamp(delta, -1200, 1200);
    remainder_ = total % 120;
    return total / 120;
  }
  void Cancel() { remainder_ = 0; target_ = 0; }
 private:
  int remainder_ = 0;
  uint32_t target_ = 0;
};
}
