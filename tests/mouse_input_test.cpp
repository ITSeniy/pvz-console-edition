#include "mouse_input.h"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <source_location>
#include <vector>

void check(bool value, std::source_location at = std::source_location::current()) {
  if (!value) { std::cerr << "Mouse input check failed at " << at.line() << '\n'; std::exit(1); }
}

struct Consumer {
  pvz::MouseButtons buttons;
  pvz::MouseWheel wheel;
  std::optional<pvz::Point> point;
  bool dragging = false;
  std::vector<int> drag_path, steps;
  int releases = 0;
  void Consume(const pvz::MouseInputEvent& event) {
    using enum pvz::MouseEventKind;
    point = event.point;
    if (event.kind == Cancel) { buttons.Cancel(); wheel.Cancel(); dragging = false; }
    if (event.kind == Move && dragging) drag_path.push_back(int(point->x));
    if (event.kind == Button && buttons.Accept(event.click, event.down)) {
      dragging = event.down;
      if (!event.down) ++releases;
    }
    if (event.kind == Wheel) steps.push_back(wheel.Take(event.delta, 42));
  }
};
void drain(pvz::MouseInputStream& stream, Consumer& consumer) {
  while (auto event = stream.Take()) consumer.Consume(*event);
}

int main() {
  using namespace pvz;
  // Every partition of the same event stream must produce the same path,
  // final pointer and release count, including movement AFTER release.
  for (unsigned partition = 0; partition < 32; ++partition) {
    MouseInputStream stream; Consumer consumer;
    stream.Button({10, 10}, 1, true); if (partition & 1) drain(stream, consumer);
    stream.Move(Point{20, 10}); if (partition & 2) drain(stream, consumer);
    stream.Move(Point{30, 10}); if (partition & 4) drain(stream, consumer);
    stream.Button({30, 10}, 1, false); if (partition & 8) drain(stream, consumer);
    stream.Move(Point{90, 10}); if (partition & 16) drain(stream, consumer);
    drain(stream, consumer);
    check(consumer.drag_path == std::vector<int>({20, 30}));
    check(consumer.point && consumer.point->x == 90 && !consumer.dragging && consumer.releases == 1);
  }
  for (auto reason : {MouseCancelReason::PointerLeft, MouseCancelReason::FocusLost,
      MouseCancelReason::Controller, MouseCancelReason::ModeChanged, MouseCancelReason::Disabled,
      MouseCancelReason::Overflow}) {
    MouseInputStream stream; Consumer consumer;
    stream.Button({10, 10}, 1, true); drain(stream, consumer);
    stream.Wheel({10, 10}, 60); drain(stream, consumer);
    stream.Button({10, 10}, -1, true); // Old queued press must disappear.
    stream.Cancel(reason);
    stream.Button({90, 10}, 1, false); // Orphan release must not act.
    stream.Wheel({90, 10}, 60);
    drain(stream, consumer);
    check(!consumer.dragging && consumer.releases == 0);
    check(consumer.steps == std::vector<int>({0, 0}));
    stream.Button({20, 10}, 1, true); stream.Button({20, 10}, 1, false);
    drain(stream, consumer); check(consumer.releases == 1);
  }
  {
    MouseInputStream stream; stream.SetMode(true);
    stream.Button({10, 10}, 1, true); auto old = stream.Take();
    stream.Wheel({10, 10}, 120); stream.SetMode(false);
    check(old && !stream.Current(*old));
    auto cancelled = stream.Take(); check(cancelled && cancelled->kind == MouseEventKind::Cancel);
    check(!stream.Take());
    stream.SetMode(false); check(!stream.Take());
    stream.Cancel(MouseCancelReason::FocusLost); stream.Cancel(MouseCancelReason::Controller);
    check(stream.TakeFocusLost() && !stream.TakeFocusLost());
  }
  {
    MouseInputStream stream; Consumer consumer;
    stream.Button({10, 10}, 1, true); drain(stream, consumer);
    for (size_t i = 0; i < MouseInputStream::kCapacity + 1; ++i) stream.Move(Point{float(i), 10});
    stream.Button({300, 10}, 1, false); drain(stream, consumer);
    check(!consumer.dragging && consumer.releases == 0 && consumer.drag_path.empty());
  }
  {
    MouseInputStream stream;
    stream.Wheel({10, 10}, 120); stream.Move(Point{20, 10});
    auto wheel = stream.Take(), move = stream.Take();
    check(wheel && move && wheel->kind == MouseEventKind::Wheel && wheel->point->x == 10);
    check(move->sequence > wheel->sequence && move->point->x == 20);
    stream.Wheel({20, 10}, -120); check(stream.Take()->delta == -120);
    MouseWheel accumulator;
    check(accumulator.Take(60, 1) == 0 && accumulator.Take(60, 2) == 0);
    check(accumulator.Take(60, 2) == 1);
    accumulator.Cancel(); check(accumulator.Take(-60, 2) == 0);
    check(accumulator.Take(-60, 2) == -1);
  }
  {
    MouseInputStream stream; MouseButtons buttons;
    stream.Move(Point{std::numeric_limits<float>::quiet_NaN(), 0});
    stream.Button({0, 0}, 3, true); check(!stream.Take());
    check(buttons.Accept(1, true) && !buttons.Accept(1, true));
    check(buttons.Accept(-1, true) && buttons.Accept(-1, false));
    check(buttons.Accept(2, true) && !buttons.Accept(2, true));
    check(buttons.Accept(1, false) && !buttons.Accept(1, false));
    check(buttons.Accept(2, false) && !buttons.Accept(2, false));
    stream.Button({0, 0}, 2, true);
    check(stream.Take()->click == 2);
    check(buttons.Accept(2, true)); buttons.Cancel();
    check(!buttons.Accept(2, false));
  }
  std::cout << "Mouse event ordering, frame partitions, cancellation and wheel checks passed\n";
}
