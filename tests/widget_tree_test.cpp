#include "widget_tree.h"
#include <cstdlib>
#include <iostream>
#include <source_location>
#include <vector>

void check(bool value, std::source_location at = std::source_location::current()) {
  if (!value) { std::cerr << "Widget tree check failed at " << at.line() << '\n'; std::exit(1); }
}
int main() {
  using namespace pvz::guest;
  std::vector<uint8_t> memory(8192);
  auto* base = memory.data();
  const uint32_t manager = 256, parent = 1024, child = 2048, replacement = 4096;
  auto attach = [&](uint32_t container, uint32_t head, uint32_t node, uint32_t widget) {
    Write(base, container + 0x18, head); Write(base, head, node);
    Write(base, node, head); Write(base, node + 8, widget);
    base[widget + 0x70] = 1;
  };
  attach(manager, 512, 544, parent); attach(parent, 1536, 1568, child);
  check(LiveWidget(base, manager, parent) && LiveWidget(base, manager, child));
  check(!LiveWidget(base, manager, replacement) && !LiveWidget(base, manager, 0));
  base[parent + 0x70] = 0;
  int visible = 0; VisitWidgets(base, manager, [&](uint32_t) { ++visible; return true; }, true);
  check(visible == 0 && LiveWidget(base, manager, child)); // Hidden capture can still be cancelled.
  Write(base, 1568 + 8, replacement);
  check(!LiveWidget(base, manager, child) && LiveWidget(base, manager, replacement));
  // Empty cyclic node must consume the traversal budget too.
  Write(base, 1568 + 8, 0); Write(base, 1568, 1568);
  check(!LiveWidget(base, manager, child));
  // Unaligned big-endian access is valid on the host.
  Write(base, 7, 0x12345678); check(Read(base, 7) == 0x12345678 && base[7] == 0x12);
  // Back follows all 94 ChallengeScreen entries and must remain reachable.
  const uint32_t list_head = 5000, first_node = 5100;
  Write(base, parent + 0x18, list_head); Write(base, list_head, first_node);
  for (int i = 0; i < 95; ++i) {
    const uint32_t node = first_node + i * 16;
    Write(base, node, i == 94 ? list_head : node + 16);
    Write(base, node + 8, i == 94 ? replacement : 0);
  }
  check(LiveWidget(base, manager, replacement));
  std::cout << "Live widget, hidden ancestor, removal and bounded traversal checks passed\n";
}
