#!/usr/bin/env python3
"""Gate: every prefab carrying OVT_StructureDestructionComponent must author hit-zone
health the engine can actually hold.

The engine clamps a hit zone's max health to a 16-bit value (65 535), while vanilla's
ruin line is computed from the AUTHORED m_fBaseHealth + m_fPhaseHealth. Author more
than 65 535 in total and the structure spawns already past its ruin line, so the first
round that registers destroys it (core/damage BD31). A missing DamageThreshold means
every stray small-arms hit registers at all (BD31/BD33).

Run from the repo root. Exit 0 = clean, 1 = a prefab would die to a stray shot.
"""
import re
import sys
from pathlib import Path

CAP = 65535
FLOOR_MARGIN = 1535  # keep authored total comfortably under the cap
COMPONENT = "OVT_StructureDestructionComponent"

# Deliberately fragile by design (core/damage BD33) - a few rifle rounds SHOULD set it off.
EXEMPT_THRESHOLD = {"Prefabs/Structures/Military/FOB/OVT_FuelDepot.et"}


def check(path: Path, root: Path) -> list[str]:
    text = path.read_text(encoding="utf-8", errors="replace")
    if COMPONENT not in text:
        return []

    rel = path.relative_to(root).as_posix()
    problems = []

    base = [float(m) for m in re.findall(r"^\s*m_fBaseHealth\s+([\d.]+)", text, re.M)]
    phases = [float(m) for m in re.findall(r"^\s*m_fPhaseHealth\s+([\d.]+)", text, re.M)]

    if not base:
        problems.append(f"{rel}: no m_fBaseHealth authored")
    if not phases:
        problems.append(f"{rel}: no m_fPhaseHealth authored")

    total = (base[0] if base else 0) + sum(phases)
    if total > CAP - FLOOR_MARGIN:
        problems.append(
            f"{rel}: base + phase health = {total:.0f}, over the {CAP} 16-bit hit-zone cap "
            f"(minus a {FLOOR_MARGIN} margin) - this structure spawns past its ruin line "
            f"and dies to the first registering hit (BD31). Use 32000 + 32000."
        )

    if rel not in EXEMPT_THRESHOLD and not re.search(r"^\s*DamageThreshold\s+", text, re.M):
        problems.append(
            f"{rel}: hit zone has no DamageThreshold - small-arms fire will register on it. "
            f"Add 'DamageThreshold 50' inside the SCR_HitZone Default block (BD31)."
        )

    return problems


def main() -> int:
    root = Path.cwd()
    problems = []
    scanned = 0
    for folder in ("Prefabs", "PrefabsEditable"):
        for path in sorted((root / folder).rglob("*.et")):
            found = check(path, root)
            if COMPONENT in path.read_text(encoding="utf-8", errors="replace"):
                scanned += 1
            problems.extend(found)

    if problems:
        print(f"FAIL - {len(problems)} problem(s) across {scanned} destructible prefab(s):\n")
        for p in problems:
            print(f"  - {p}")
        return 1

    print(f"OK - {scanned} destructible prefab(s) author survivable hit-zone health.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
