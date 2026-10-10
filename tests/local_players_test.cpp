#include "local_players.h"
#include <cstdlib>
#include <iostream>
void check(bool x) { if (!x) std::exit(1); }
int main() {
  pvz::LocalPlayers players;
  constexpr auto mouse = pvz::LocalPlayers::mouse;
  // Pad arriving second can start the game and become P1.
  players.Observe(22); players.Begin(players.FirstDevice());
  check(players.Accepts(0, 22) && !players.Accepts(0, 11));
  check(players.Observe(11) == 1 && players.User(11) == 1);
  check(!players.Accepts(1, mouse) && players.Observe(mouse) == -1);
  // Repeated enumeration/neutral polls and disconnects leave ownership intact.
  check(players.User(22) == 0 && players.User(11) == 1);
  check(!players.Accepts(0, 11) && !players.Accepts(1, 33));
  players.Reset(); players.Begin(mouse);
  check(players.User(mouse) == 0 && players.Accepts(1, 11));
  check(players.Observe(11) == 1 && !players.Accepts(1, 22));
  players.Reset(); players.Observe(11); players.Begin(11);
  check(players.Observe(mouse) == 1 && players.User(mouse) == 1);
  check(!players.Accepts(0, mouse) && !players.Accepts(1, 22));
  players.Reset();
  check(players.Accepts(0, mouse) && players.Accepts(0, 22) && !players.Accepts(1, 22));
  std::cout << "First press, two pads, mixed input and retained player identity passed\n";
}
