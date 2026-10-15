"""Verify public contents, PE icons, and observed edition decisions."""
import argparse
import ctypes
import hashlib
import json
import struct
import zipfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument("archive", type=Path)
p.add_argument("trial_log", type=Path)
p.add_argument("full_log", type=Path)
p.add_argument("unconfirmed_log", type=Path)
args = p.parse_args()
checks = []

def check(label, condition):
    checks.append({"check": label, "passed": bool(condition)})
    if not condition:
        raise AssertionError(label)

exe = root / "build/pvz-console-edition.exe"
with zipfile.ZipFile(args.archive) as z:
    names = [n for n in z.namelist() if not n.endswith("/")]
    prefix = names[0].split("/", 1)[0] + "/"
    manifest = json.loads(z.read(prefix + "build-info.json").decode("utf-8-sig"))
    check("Trial default without game data/profile", manifest["default_edition"] == "trial" and
          not manifest["includes_game_assets"] and not manifest["includes_user_profile"])
    check("No game data, logs, or ownership receipt", all(not n.removeprefix(prefix).startswith(
          ("assets/", "runtime-data/", "logs/")) and ".pvz-import.toml" not in n for n in names))
    check("Exact manifest inventory", set(names) == {prefix + f["path"] for f in manifest["files"]} |
          {prefix + "build-info.json"} and len(names) == len(set(names)))
    for file in manifest["files"]:
        data = z.read(prefix + file["path"])
        check("Archive integrity: " + file["path"], len(data) == file["bytes"] and
              hashlib.sha256(data).hexdigest() == file["sha256"])
    check("Current branded executable", z.read(prefix + "build/pvz-console-edition.exe") == exe.read_bytes())
    check("Default config is Trial", b"pvz_full_version = false" in z.read(prefix + "build/pc_defaults.toml"))
    check("Import and launch tools included", all(prefix + n in names for n in
          ("Start.bat", "run.ps1", "Import-Game.ps1", "scripts/import_game.py", "scripts/extract_stfs.py")))

# Read Windows resources as data; do not execute or load the game's DLLs.
kernel = ctypes.WinDLL("kernel32", use_last_error=True)
kernel.LoadLibraryExW.argtypes = [ctypes.c_wchar_p, ctypes.c_void_p, ctypes.c_uint32]
kernel.LoadLibraryExW.restype = ctypes.c_void_p
kernel.FindResourceW.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]
kernel.FindResourceW.restype = ctypes.c_void_p
kernel.LoadResource.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
kernel.LoadResource.restype = ctypes.c_void_p
kernel.LockResource.argtypes = [ctypes.c_void_p]
kernel.LockResource.restype = ctypes.c_void_p
kernel.SizeofResource.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
kernel.SizeofResource.restype = ctypes.c_uint32
kernel.FreeLibrary.argtypes = [ctypes.c_void_p]
module = kernel.LoadLibraryExW(str(exe), None, 2)
check("PE resource section can be loaded", bool(module))
def resource(identifier, kind):
    info = kernel.FindResourceW(module, identifier, kind)
    if not info: return b""
    data = kernel.LockResource(kernel.LoadResource(module, info))
    return ctypes.string_at(data, kernel.SizeofResource(module, info))
try:
    group = resource(1, 14)
    check("Windows ICO group present", len(group) >= 6 and struct.unpack_from("<HHH", group) == (0, 1, 5))
    sizes = set()
    for at in range(6, len(group), 14):
        width, height, _, _, _, _, size, identifier = struct.unpack_from("<BBBBHHIH", group, at)
        sizes.add((width, height))
        check("Icon image resource " + str(width), len(resource(identifier, 3)) == size)
    check("Five native icon sizes", sizes == {(16,16),(24,24),(32,32),(48,48),(64,64)})
    version = resource(1, 16)
    check("Product name and version embedded", "pvz-console-edition".encode("utf-16le") in version and
          "0.7.0".encode("utf-16le") in version and
          "pvz-console-edition.exe".encode("utf-16le") in version)
finally:
    kernel.FreeLibrary(module)

for label, log, expected in [
    ("Legacy/CLI Full flag cannot replace import", args.trial_log, "PvZ edition: trial (license mask 0; no own-copy import)"),
    ("Confirmed supported import enables Full", args.full_log, "PvZ edition: full (license mask 1; validated import and owner's full-copy declaration)"),
    ("Unconfirmed import stays Trial", args.unconfirmed_log, "PvZ edition: trial (license mask 0; import has no full-copy ownership confirmation)"),
]:
    content = log.read_text(encoding="utf-8", errors="replace")
    check("Current build log: " + log.name, log.stat().st_mtime >= exe.stat().st_mtime)
    check(label, expected in content and "Direct3D 12 device and OS features" in content and "[critical]" not in content)
report = {"built_executable_sha256": hashlib.sha256(exe.read_bytes()).hexdigest(),
          "archive": str(args.archive.resolve()), "passed": sum(c["passed"] for c in checks),
          "total": len(checks), "checks": checks,
          "purchase_verified": False, "gameplay_regressions_retested": False}
(root / "runtime-data/distribution-validation.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
print(json.dumps({k: v for k,v in report.items() if k != "checks"}, indent=2))
