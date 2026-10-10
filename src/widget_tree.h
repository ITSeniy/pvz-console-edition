#pragma once
#include "guest_memory.h"

namespace pvz::guest {
// Xbox WidgetContainer child list. Visitors never retain nodes across a guest
// callback. Count/depth budgets also bound malformed cycles and null children.
template <class Visitor>
bool VisitChildren(uint8_t* base, uint32_t container, int& budget, Visitor&& visitor) {
  const auto head = Read(base, container + 0x18);
  if (!head) return true;
  auto node = Read(base, head);
  // ChallengeScreen owns 94 level buttons before its navigation children.
  for (int i = 0; node && node != head && i < 128 && budget > 0; ++i) {
    --budget;
    const auto child = Read(base, node + 8);
    const auto next = Read(base, node);
    if (child && !visitor(child)) return false;
    node = next;
  }
  return true;
}
template <class Visitor>
void VisitWidgets(uint8_t* base, uint32_t manager, Visitor&& visitor, bool visible_only = false) {
  int budget = 256;
  auto visit = [&](auto&& self, uint32_t container, int depth) -> bool {
    if (depth > 16) return true;
    return VisitChildren(base, container, budget, [&](uint32_t child) {
      if (visible_only && !base[child + 0x70]) return true;
      if (!visitor(child)) return false;
      return self(self, child, depth + 1);
    });
  };
  visit(visit, manager, 0);
}
inline bool LiveWidget(uint8_t* base, uint32_t manager, uint32_t target) {
  if (!target) return false;
  bool live = false;
  VisitWidgets(base, manager, [&](uint32_t widget) {
    live = widget == target;
    return !live;
  });
  return live;
}
}
