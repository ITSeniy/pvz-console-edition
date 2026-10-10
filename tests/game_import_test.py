"""Integration checks using a caller-provided Xbox container in temporary folders."""
import argparse
import hashlib
import json
import sys
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / "scripts"))
from import_game import import_game

p = argparse.ArgumentParser(description=__doc__)
p.add_argument("package", type=Path)
args = p.parse_args()
lock = root / "config/game.lock.json"
passed = []
with tempfile.TemporaryDirectory(prefix="pvz-import-test-") as folder:
    base = Path(folder)
    dest = base / "assets"
    result = import_game(args.package, dest, lock)
    assert result["edition"] == "trial" and 'owner_confirmed_full_copy = false' in (dest / '.pvz-import.toml').read_text()
    passed.append("Import without declaration is Trial")
    result = import_game(args.package, dest, lock, full=True)
    assert result["edition"] == "full" and 'owner_confirmed_full_copy = true' in (dest / '.pvz-import.toml').read_text()
    passed.append("Validated reimport with declaration is Full")
    receipt = (dest / '.pvz-import.toml').read_bytes()
    with (dest / 'main.pak').open('ab') as f: f.write(b'changed')
    modified = hashlib.sha256((dest / 'main.pak').read_bytes()).digest()
    try:
        import_game(args.package, dest, lock)
        raise AssertionError("Modified destination was accepted")
    except FileExistsError:
        assert modified == hashlib.sha256((dest / 'main.pak').read_bytes()).digest()
        assert receipt == (dest / '.pvz-import.toml').read_bytes()
    passed.append("Different destination data is preserved; no new receipt committed")
    bad = base / "corrupt-container"
    bad.write_bytes(b"LIVE" + b"\0" * 4096)
    empty = base / "rejected"
    try:
        import_game(bad, empty, lock, full=True)
        raise AssertionError("Corrupt container was accepted")
    except ValueError:
        assert not empty.exists()
    passed.append("Bad container rejected before destination is created")
print(json.dumps({"passed": len(passed), "total": 4, "checks": passed}, indent=2))
