"""Check native item-drag runs: bowling=log shovel=log zombotany=log slot=log quarium=log."""
from pathlib import Path
import hashlib
import json
import re
import sys

names = {"bowling", "shovel", "zombotany", "slot", "quarium"}
paths = dict(arg.split("=", 1) for arg in sys.argv[1:])
if set(paths) != names:
    sys.exit("Provide bowling, shovel, zombotany, slot and quarium name=log arguments")
logs = {name: Path(path).read_text(encoding="utf-8") for name, path in paths.items()}
checks = {}
for name, log in logs.items():
    checks[name + "_native_run"] = "Direct3D 12 device and OS features" in log and not any(
        s in log.lower() for s in ("[critical]", "[fatal]", "timed out", "invalid input replay", "access violation"))

def between(log, first, last):
    start = log.find(first)
    end = log.find(last, start + len(first)) if start >= 0 else -1
    return log[start:end] if end > start else ""

b = logs["bowling"]
# Use distinct release/next-pick markers, including the first ordinary click.
down = b.find("PvZ replay left_down (225, 86)")
up = b.find("PvZ replay left_up (230, 210)", down)
next_pick = b.find("PvZ replay left_down (225, 86)", up)
checks["conveyor_plants_only_on_release"] = down >= 0 and up > down and next_pick > up and (
    "mouse used packet" not in b[down:up] and "item drag seed release planted true" in b[up:next_pick])
checks["conveyor_three_uses_no_duplicate_plant"] = b.count("mouse used packet 0") == 3 and b.count("mouse planted seed") == 3
checks["conveyor_drag_and_click_click"] = b.count("item drag seed release planted true") == 2
checks["selected_card_toggle"] = b.count("mouse toggle packet 0 off") >= 2
checks["right_click_cancels_drag"] = "item drag cancelled right click" in b
checks["window_mode_focus_controller_cancel"] = b.count("item drag cancelled input or screen changed") >= 4
boundary = between(b, "PvZ replay left_up (225, 120)", "PvZ replay left_down (225, 86)")
checks["bank_bottom_release_does_not_plant"] = bool(boundary) and "mouse planted seed" not in boundary
checks["quick_down_up_without_move"] = "PvZ replay left_up (285, 370)" in b and bool(re.search(
    r"item drag seed release planted true at 216,384", b))
checks["controller_cancel_has_no_late_plant"] = "PvZ replay pad_press" in b and "PvZ replay left_up (300, 370)" in b and (
    "mouse planted seed" not in b[b.find("PvZ replay pad_press"):])

s = logs["shovel"]
checks["shovel_drag_digs_once"] = s.count("item drag shovel dug true") == 1
checks["shovel_click_click_still_digs"] = s.count("PC shovel dug true") == 1
checks["shovel_outside_lawn_keeps_capture"] = "PvZ replay move (70, 120)" in s and "item drag cancelled" not in s
checks["console_timed_shovel_completes_lesson"] = bool(re.search(
    r"console shovel released after [\d.]+ dug true tutorial 17 -> 18", s))
checks["lesson_continues_with_dave"] = "mouse Dave cutscene click message" in s

z = logs["zombotany"]
checks["chooser_imitater_and_start"] = all(t in z for t in (
    "imitater copy seed 0", "chooser button 100 activated", "wait_board ready"))
checks["normal_card_drag_plant"] = "item drag seed release planted true" in z and "mouse planted seed" in z
normal_before_up = between(z, "PvZ replay left_down (262, 87)", "PvZ replay left_up (230, 260)")
checks["normal_card_no_early_plant"] = bool(normal_before_up) and "mouse planted seed" not in normal_before_up
checks["normal_card_preview"] = "mouse planting preview cell" in normal_before_up
panel = between(z, "PvZ replay left_down (262, 87)", "PvZ replay move (230, 260)")
checks["no_lawn_preview_above_bank"] = bool(panel) and "mouse planting preview cell" not in panel
checks["unaffordable_card_keeps_native_check"] = "mouse pick packet 0 allowed false" in z and "mouse sound buzzer" in z

slot = logs["slot"]
checks["slot_lever_and_dropped_packet"] = all(t in slot for t in (
    "hit 22 state 4", "mouse dropped seed picked true", "mouse dropped seed placed true"))
checks["dropped_packet_does_not_start_bank_drag"] = "item drag seed begin" not in slot
pickup = between(slot, "PvZ replay left_down (375, 160)", "PvZ replay left_down (285, 325)")
checks["dropped_packet_release_barrier"] = bool(pickup) and "mouse dropped seed placed" not in pickup

q = logs["quarium"]
checks["quarium_purchase_and_food"] = "minigame 24 buy packet 0 type 59 active 1 sun 400 -> 300" in q and (
    len(re.findall(r"minigame 24 field click .* hit 0 state 0", q)) >= 3)
checks["purchase_does_not_start_plant_drag"] = "item drag seed begin" not in q

report = {"logs": {n: str(Path(p).resolve()) for n, p in paths.items()},
          "executable_sha256": hashlib.sha256(Path("build/pvz-console-edition.exe").read_bytes()).hexdigest(),
          "passed": sum(checks.values()), "total": len(checks), "checks": checks}
Path("runtime-data/item-drag-validation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
print(json.dumps(report, indent=2))
sys.exit(0 if all(checks.values()) else 1)
