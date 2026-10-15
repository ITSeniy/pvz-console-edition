#include "replay_event.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <source_location>

void check(bool value, std::source_location at = std::source_location::current()) {
  if (!value) { std::cerr << "Replay check failed at " << at.line() << '\n'; std::exit(1); }
}
int main(int argc, char** argv) {
  pvz::ReplayEvent event;
  for (const auto& action : pvz::kReplayActions) {
    const int x = action.action == pvz::ReplayAction::FixtureMode ? 51 :
        action.action == pvz::ReplayAction::FixtureGardenCoin ? 200 :
        action.action == pvz::ReplayAction::FixtureLevel || action.action == pvz::ReplayAction::Capture ? 1 :
        action.action == pvz::ReplayAction::FixtureCoin ? 8 : 0;
    const int y = action.action == pvz::ReplayAction::FixtureGardenCoin ? 100 : 0;
    check(pvz::ParseReplayRow("1," + std::to_string(x) + "," + std::to_string(y) + "," + std::string(action.name), event));
    check(event.info.action == action.action);
  }
  for (const auto* row : {"1,0,0,unknown", "-1,0,0,move", "1,nan,0,move",
      "1,0,0,move trailing", "1,51,0,fixture_level", "1,1.5,0,fixture_level",
      "1,999999999999,0,controller", "1,3,0,fixture_mode", "1,0,4,controller", "1,0,0,fixture_garden_coin",
      "1,0,0,fixture_coin", "1,31,0,fixture_coin", "1,0,0,capture", "1,33,0,capture"})
    check(!pvz::ParseReplayRow(row, event));
  auto parse = [](std::initializer_list<const char*> rows) {
    std::vector<pvz::ReplayEvent> result;
    for (auto row : rows) { pvz::ReplayEvent e; check(pvz::ParseReplayRow(row, e)); result.push_back(e); }
    return result;
  };
  check(pvz::ValidateReplay(parse({"1,0,0,batch_begin", "1,10,10,left_down", "1,10,10,left_up", "1,0,0,batch_end"})));
  check(!pvz::ValidateReplay(parse({"1,0,0,batch_begin", "2,0,0,move", "2,0,0,batch_end"})));
  check(!pvz::ValidateReplay(parse({"1,0,0,batch_begin", "1,0,0,wait_menu", "1,0,0,batch_end"})));
  check(!pvz::ValidateReplay(parse({"1,0,0,batch_end"})));
  check(!pvz::ValidateReplay(parse({"1,0,0,batch_begin"})));
  check(!pvz::ValidateReplay(parse({"2,0,0,move", "1,0,0,move"})));
  int checked = 0;
  if (argc == 2) for (const auto& file : std::filesystem::directory_iterator(argv[1])) {
    if (file.path().extension() != ".csv") continue;
    std::ifstream input(file.path()); std::string line;
    std::vector<pvz::ReplayEvent> events;
    while (std::getline(input, line)) {
      const auto first = line.find_first_not_of(" \t\r");
      if (first == std::string::npos || line[first] == '#') continue;
      if (!pvz::ParseReplayRow(line, event)) {
        std::cerr << file.path() << ": " << line << '\n'; return 1;
      }
      events.push_back(event);
    }
    check(pvz::ValidateReplay(events)); ++checked;
  }
  std::cout << "Replay validation checks passed; " << checked << " existing CSV files accepted\n";
}
