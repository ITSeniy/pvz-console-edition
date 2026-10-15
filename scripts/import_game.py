"""Import supported Xbox content; Full requires an explicit owner's declaration.

No Xbox purchase verification is performed. Never distribute the resulting
assets or .pvz-import.toml as part of the public binary release.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import shutil
import tempfile
from pathlib import Path
from extract_stfs import extract
from extract_xiso import entries as disc_entries, SECTOR as DISC_SECTOR


def disc_package(image: Path, out: Path) -> bool:
    """Copy the game's XBLA package out of an Xbox 360 disc image (XDVDFS)."""
    try:
        base, items = disc_entries(image)
    except ValueError:
        return False
    for path, sector, size, directory in items:
        if not directory and '/584109FF/000D0000/' in '/' + path.upper():
            with image.open('rb') as source, out.open('wb') as target:
                source.seek(base + sector * DISC_SECTOR)
                target.write(source.read(size))
            return True
    raise ValueError('This disc does not contain Plants vs. Zombies')


def import_game(package: Path, destination: Path, lock_path: Path, full: bool = False) -> dict:
    # A disc image carries the game's package; owning the disc implies Full.
    with tempfile.TemporaryDirectory(prefix="pvz-console-edition-disc-") as temporary:
        extracted = Path(temporary) / 'package'
        with package.open('rb') as file:
            magic = file.read(4)
        if magic not in (b'LIVE', b'PIRS') and disc_package(package, extracted):
            result = import_package(extracted, destination, lock_path, True, disc=True)
            return result
    return import_package(package, destination, lock_path, full)


def import_package(package: Path, destination: Path, lock_path: Path, full: bool = False, disc: bool = False) -> dict:
    lock = json.loads(lock_path.read_text(encoding="utf-8-sig"))
    expected = {"default.xex": lock["xex_sha256"], "main.pak": lock["main_pak_sha256"],
                "en.pak": lock["en_pak_sha256"]}
    # Reject unsupported/corrupt containers before touching the destination.
    with tempfile.TemporaryDirectory(prefix="pvz-console-edition-import-") as temporary:
        stage = Path(temporary) / "assets"
        report = extract(package, stage)
        files = {item["path"]: item["sha256"] for item in report["files"]}
        if report["title_id"] != lock["title_id"] or any(files.get(n) != h for n, h in expected.items()):
            raise ValueError("Unsupported Xbox game version; see config/game.lock.json")
        destination = destination.resolve()
        for item in report["files"]:
            target = (destination / item["path"]).resolve()
            if not target.is_relative_to(destination):
                raise ValueError("Destination contains a path escaping the game folder")
            if target.exists() and (not target.is_file() or
                    hashlib.sha256(target.read_bytes()).hexdigest() != item["sha256"]):
                raise FileExistsError(f"Refusing to replace different game file: {target}")
        destination.mkdir(parents=True, exist_ok=True)
        for item in report["files"]:
            target = destination / item["path"]
            if not target.exists():
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(stage / item["path"], target)
        # Commit last. Store hashes only: no account IDs or original private path.
        receipt = "schema_version = 1\n"
        receipt += f'title_id = "{lock["title_id"]}"\n'
        receipt += f'package_sha256 = "{report["package_sha256"]}"\n'
        if disc:
            receipt += 'source = "disc"\n'
        receipt += f'owner_confirmed_full_copy = {str(full).lower()}\n\n[files]\n'
        receipt += "".join(f'"{name}" = "{digest}"\n' for name, digest in expected.items())
        marker = destination / ".pvz-import.toml"
        if marker.is_symlink():
            raise ValueError("Import receipt must not be a symbolic link")
        pending = destination / ".pvz-import.toml.pending"
        # Exclusive creation also rejects an unexpected symlink at the temp path.
        with pending.open("x", encoding="utf-8") as output:
            output.write(receipt)
        pending.replace(marker)
    return {"edition": "full" if full else "trial", "source": "disc" if disc else "package", "destination": str(destination),
            "files_verified": len(report["files"]), "purchase_verified": False,
            "owner_confirmed_full_copy": full}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--lock", type=Path, default=Path(__file__).resolve().parents[1] / "config/game.lock.json")
    parser.add_argument("--confirm-owned-full-copy", action="store_true",
                        help="I confirm that this is my own purchased full Xbox copy")
    args = parser.parse_args()
    print(json.dumps(import_game(args.package, args.destination, args.lock,
                                 args.confirm_owned_full_copy), indent=2))
