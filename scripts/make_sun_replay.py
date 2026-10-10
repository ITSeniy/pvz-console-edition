"""Extend the basic mouse scenario with sun clicks and the level-1 seed reward."""
import argparse
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    rows = (root / "tests/replays/adventure_mouse.csv").read_text(encoding="utf-8").splitlines()

    def click(seconds, x, y):
        rows.append(f"{seconds:.3f},{x:.3f},{y:.3f},left_down")
        rows.append(f"{seconds + 0.025:.3f},{x:.3f},{y:.3f},left_up")

    # Suns spawn at random locations. Sweep with ordinary pointer events;
    # no guest-memory changes, spawn manipulation or price overrides.
    for start in (22, 36, 50, 64):
        index = 0
        for y in range(100, 401, 60):
            for x in range(40, 761, 60):
                click(start + index * 0.09, (x + 240) / 1.6, (y + 60) / 1.2)
                index += 1
        click(start + 9, 218, 80)
        click(start + 10, 500, 200)
        click(start + 11, 500, 320)
    # This fresh-profile scenario stays on level 1-1 until its seed reward.
    for seconds in (100, 103, 106):
        click(seconds, 528, 363)
    rows.append("110,400,300,move")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(rows) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
