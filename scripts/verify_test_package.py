"""Verify a tester archive against its manifest and the current build."""
import argparse
import hashlib
import json
import zipfile
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument("archive", type=Path)
a = p.parse_args()
with zipfile.ZipFile(a.archive) as z:
    names = [i.filename for i in z.infolist() if not i.is_dir()]
    roots = {n.split("/", 1)[0] for n in names}
    assert len(roots) == 1, "Expected one package root"
    root = roots.pop() + "/"
    manifest = json.loads(z.read(root + "build-info.json").decode("utf-8-sig"))
    files = manifest["files"]
    assert len(names) == len(set(names)), "Duplicate ZIP entries"
    assert set(names) == {root + f["path"] for f in files} | {root + "build-info.json"}, "Unlisted or missing file"
    assert not manifest["includes_user_profile"], "User profile in package"
    includes_test_profile = manifest.get("includes_minigames_test_profile", False)
    fresh_mouse_validation = manifest.get("mouse_validation_fresh", True)
    unvalidated = manifest.get("validation_skipped", False)
    if unvalidated:
        assert not any(n.startswith(root + "validation/") for n in names), "Historical reports in manual-test package"
        assert not any(manifest.get(k) for k in ("mouse_validation_fresh", "services_feedback_validation_fresh", "expanded_feedback_validation_fresh")), "Manual-test package claims fresh validation"
        assert b"MANUAL TEST" in z.read(root + "TEST-RESULTS.md"), "Missing manual-test limitations"
    if not fresh_mouse_validation and not unvalidated:
        assert manifest.get("test_unlock_enabled"), "Historical reports need explicit test-unlock packaging"
    for f in files:
        path = f["path"]
        assert not path.startswith("logs/"), "Runtime logs in package"
        if path.startswith("runtime-data/"):
            assert includes_test_profile and path.startswith(
                "runtime-data/minigames-test/user/"), "Unexpected runtime data in package"
        data = z.read(root + f["path"])
        assert len(data) == f["bytes"], f["path"]
        assert hashlib.sha256(data).hexdigest() == f["sha256"], f["path"]
    assert z.read(root + "build/pvz-console-edition.exe") == Path("build/pvz-console-edition.exe").read_bytes(), "Stale executable"
    if includes_test_profile:
        assert root + "Start-Minigames.bat" in names, "Missing minigames launcher"
        profile = z.read(root + "runtime-data/minigames-test/user/584109FF/profile/User/63E83FFF")
        assert len(profile) == 850 and int.from_bytes(profile[:4], "big") == 19, "Invalid test profile"
        wins = [int.from_bytes(profile[0x10 + i * 4:0x14 + i * 4], "big", signed=True)
                for i in range(15, 35)]
        assert sum(w > 0 for w in wins) >= 17, "Not all minigames unlocked"
        assert z.read(root + "runtime-data/minigames-test/user/pvz.toml") == z.read(
            root + "build/pc_defaults.toml"), "Nonstandard test settings"
        if manifest.get("test_unlock_enabled"):
            assert b"--pvz_test_unlock=true" in z.read(root + "Start-Minigames.bat"), "Test launcher lacks unlock switch"
        if not unvalidated:
            reports = ["minigames-mouse-validation.json", "heavy-weapon-fire-buy-latest.json"]
            if manifest.get("heavy_weapon_control_model") == "deluxe":
                reports += ["heavy-weapon-deluxe-validation.json", "heavy-weapon-latest.json"]
            if manifest.get("mouse_item_control_model") == "drag-release":
                reports += ["item-drag-validation.json"]
                assert root + "docs/item-drag.md" in names, "Missing drag instructions"
            if manifest.get("mouse_services_control_model") == "native-tools-store-almanac":
                reports += ["services-mouse-validation.json"]
                assert root + "docs/services-mouse.md" in names, "Missing services instructions"
            if manifest.get("mouse_input_audit_model") == "xbox-rtti-android-vita":
                reports += ["input-coverage-validation.json"]
                assert root + "docs/input-coverage.md" in names, "Missing input audit checklist"
                audit = json.loads(z.read(root + "validation/input-coverage-validation.json").decode("utf-8-sig"))
                if fresh_mouse_validation:
                    assert audit["built_executable_sha256"] == hashlib.sha256(z.read(root + "build/pvz-console-edition.exe")).hexdigest(), "Stale input audit"
                assert not audit["unreviewed_classes"] and audit["manual_pending"], "Missing coverage limitations"
                inventory = json.loads(z.read(root + "docs/input-inventory/handlers.json").decode("utf-8-sig"))
                assert inventory["widget_count"] == audit["inventory"]["widget_count"] == 58, "Wrong widget inventory"
                assert inventory["controls_count"] == 4 and inventory["android_handler_count"] == 224, "Wrong reference counts"
            if manifest.get("mouse_event_transport_model") == "ordered-events-generation-cancel":
                reports += ["mouse-transport-validation.json"]
                assert root + "docs/mouse-hook-implementation-2026-10-09.md" in names, "Missing mouse hook test results"
                transport = json.loads(z.read(root + "validation/mouse-transport-validation.json").decode("utf-8-sig"))
                if fresh_mouse_validation:
                    assert transport["built_executable_sha256"] == hashlib.sha256(z.read(root + "build/pvz-console-edition.exe")).hexdigest(), "Stale transport validation"
                assert not transport["evidence_errors"], "Invalid transport evidence"
            if manifest.get("test_unlock_enabled"):
                reports += ["test-unlock-validation.json"]
                unlock = json.loads(z.read(root + "validation/test-unlock-validation.json").decode("utf-8-sig"))
                assert unlock["built_executable_sha256"] == hashlib.sha256(z.read(root + "build/pvz-console-edition.exe")).hexdigest(), "Stale test-unlock validation"
                assert b"--pvz_test_unlock=true" in z.read(root + "Start-Minigames.bat"), "Test launcher lacks unlock switch"
            if manifest.get("mouse_shop_control_model") == "native-hover-focus-house":
                reports += ["shop-regression-validation.json"]
                shop = json.loads(z.read(root + "validation/shop-regression-validation.json").decode("utf-8-sig"))
                assert shop["built_executable_sha256"] == hashlib.sha256(z.read(root + "build/pvz-console-edition.exe")).hexdigest(), "Stale shop validation"
                assert not shop["evidence_errors"] and shop["checks"]["two_repeats_per_mode"], "Incomplete shop validation"
            if manifest.get("pc_menu_control_model") == "host-achievements-native-cheats-confirm-quit":
                reports += ["services-feedback-validation.json"]
                services = json.loads(z.read(root + "validation/services-feedback-validation.json").decode("utf-8-sig"))
                assert services["built_executable_sha256"] == hashlib.sha256(z.read(root + "build/pvz-console-edition.exe")).hexdigest(), "Stale services feedback validation"
                assert not services["evidence_errors"], "Invalid services feedback evidence"
            if manifest.get("expanded_feedback_validation_fresh"):
                for report in ("feedback-regression-validation.json", "multiplayer-regression-validation.json"):
                    reports.append(report)
                    feedback = json.loads(z.read(root + "validation/" + report).decode("utf-8-sig"))
                    assert feedback["built_executable_sha256"] == hashlib.sha256(z.read(root + "build/pvz-console-edition.exe")).hexdigest(), "Stale expanded feedback validation"
                    assert not feedback["evidence_errors"], "Invalid expanded feedback evidence"
                assert root + "docs/input-feedback-2026-10-09.md" in names, "Missing feedback checklist"
            for report in reports:
                result = json.loads(z.read(root + "validation/" + report).decode("utf-8-sig"))
                assert result["passed"] == result["total"] > 0, "Failed validation report"
print(json.dumps({"archive": str(a.archive.resolve()), "files_verified": len(files),
    "current_executable": True, "includes_user_profile": False,
    "includes_minigames_test_profile": includes_test_profile}, indent=2))
