"""Read-only control fingerprint for the supplied Heavy Weapon Deluxe executable.

The launcher contains an embedded game PE. Inspect it without executing or
modifying either image; addresses refer to the embedded game's image base.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument("exe", type=Path, nargs="?", default=Path("Heavy Weapon Deluxe/HeavyWeapon.exe"))
p.add_argument("--output", type=Path)
a = p.parse_args()
data = a.exe.read_bytes()
expected_sha = "a38f6932daf3758c0deb198c5972afc2a0a65b86802b9210b6d7266b050bb94b"
sha = hashlib.sha256(data).hexdigest()
if sha != expected_sha:
    raise SystemExit("This analysis is tied to the supplied executable; fingerprint differs.")
offset = 0x17D1A2
image = data[offset:]
pe = struct.unpack_from("<I", image, 0x3C)[0]
assert image[:2] == b"MZ" and image[pe:pe+4] == b"PE\0\0"
section_count = struct.unpack_from("<H", image, pe + 6)[0]
optional_size = struct.unpack_from("<H", image, pe + 20)[0]
base = struct.unpack_from("<I", image, pe + 24 + 28)[0]
assert base == 0x400000
sections = []
for i in range(section_count):
    s = pe + 24 + optional_size + 40 * i
    size, va, raw_size, raw = struct.unpack_from("<IIII", image, s + 8)
    sections.append((va, max(size, raw_size), raw))

def read(address, count):
    rva = address - base
    for va, size, raw in sections:
        if va <= rva and rva + count <= va + size:
            return image[raw + rva - va:raw + rva - va + count]
    raise ValueError(f"Unmapped address {address:08x}")

constants = {
    "stop_radius": (0x52A0B0, 40.0),
    "full_gain_radius": (0x52C658, 80.0),
    "distance_gain": (0x52C650, 0.025),
    "degrees_per_radian": (0x52C698, 57.29577951),
    "angle_min_degrees": (0x52C690, -90.0),
    "angle_max_degrees": (0x52C688, 90.0),
    "angle_frame_scale": (0x52C678, 1 / 9),
    "velocity_angle_scale": (0x52A0E8, 0.5),
    "velocity_center_frame": (0x52C568, 10.0),
    "base_speed_add": (0x52C660, 1.75),
    "speed_upgrade_scale": (0x52C668, 0.25),
}
for name, (address, value) in constants.items():
    assert struct.unpack("<d", read(address, 8))[0] == value, name
assert read(0x40FEB3, 13) == bytes.fromhex("8b45088b550c8941608951645d"), "Mouse move fields"
assert read(0x45EC82, 10) == bytes.fromhex("c786e40300000a000000"), "10 ms update period"
assert read(0x412EDC, 3) == bytes.fromhex("83e80c"), "12 px body/pivot offset"
assert read(0x412EE5, 2) == bytes.fromhex("d9f3"), "Angle uses FPATAN"
report = {
    "source": str(a.exe.resolve()), "sha256": sha,
    "embedded_pe_offset": hex(offset), "image_base": hex(base),
    "functions": {"mouse_move_and_drag": "0040FEB0", "mouse_down": "0041ACD0",
                  "mouse_up": "00417090", "tank_update": "00412DC0"},
    "constants": {name: {"address": f"{address:08X}", "value": value}
                  for name, (address, value) in constants.items()},
    "formula": {
        "aim": "clamp(degrees(atan2(cursor_x - tank_x, tank_y - 12 - cursor_y)), -90, 90)",
        "distance": "cursor above body: hypot(dx, tank_y - cursor_y); below: abs(int(dx))",
        "movement": "clamp(angle_degrees / 18 * clamp((distance-40)/40, 0, 1), -max_speed, max_speed)",
        "max_speed": "1.75 + 0.25 * (speed_upgrade+1)**(2/log(speed_upgrade+6)); base = 2",
        "update_ms": 10, "aim_before_movement": True,
        "primary_fire": "mouse button 0 held; down sets +128, up clears +128",
        "nuke": "mouse button 1; no equivalent nuke exists in PvZ Heavy Weapon",
    },
    "port": {"canvas_scale": 1.25, "stop_radius": 50, "full_gain_radius": 100,
             "max_step": 2.5, "simulation_hz": 100, "native_velocity_multiplier": 0.016,
             "note": "PvZ keeps its own weapons, bounds and pivot; no Deluxe speed upgrades."},
}
text = json.dumps(report, indent=2, ensure_ascii=False)
if a.output:
    a.output.parent.mkdir(parents=True, exist_ok=True)
    a.output.write_text(text + "\n", encoding="utf-8")
print(text)
