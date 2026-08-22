#!/usr/bin/env python3
"""Verify every land-isolated site on the shipped map still carries its authored flag.

WHY THIS EXISTS
---------------
`m_bLandIsolated` on OVT_BaseControllerComponent / OVT_TownControllerComponent is a statement about
the MAP that the campaign cannot derive for itself. The engine has no A-to-B reachability query:
NavmeshWorldComponent offers only tile predicates and GetReachablePoint(origin, distance, out),
which answers "some reachable point" and not "is THAT one reachable", and AIPathfindingComponent's
RayTrace is a straight line that would also reject any target behind a hill with a perfectly good
road around it. So the flag is authored, and an authored attribute has exactly one failure mode that
nothing else in this repo can see: Workbench re-saves the layer and drops it.

Nothing catches that. compile-check.sh compiles scripts, not layers. The autotests run in
OVT_Campaign_Test.ent, which has one nameless base and no island, so they can assert the GATE but
never the AUTHORING - OVT_TEST_Init_Objectives_LandIsolatedTargetsAreNeverCandidates plants the flag
on a live record for exactly that reason.

The failure is silent and slow: the campaign compiles, loads and plays, and an hour later the
occupying faction picks Erquy as its objective, drives a truck at a strait, strands it on the beach,
opens the doors, and marches a fireteam into the sea.

WHAT IT CHECKS
--------------
Each expected site below must appear in its layer with `m_bLandIsolated 1` inside its own component
block. Matching is by entity name (the layer's block header), so moving a base does not break this
check but renaming or un-flagging one does.

Add a site here whenever a new map ships water-cut-off ground.
"""

import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

# (layer file, entity block name, human description)
EXPECTED = [
    ("Worlds/MP/OVT_Campaign_Eden_Layers/bases.layer", "OVT_Base_Erquy",
     "Erquy Harbour base - on an island off the north-east coast of Everon"),
    ("Worlds/MP/OVT_Campaign_Eden_Layers/towns.layer", "Town_Erquy",
     "Erquy village - same island (a village is already excluded from objectives by size, "
     "so this is belt-and-braces against a future size change)"),
]


def block_for(text, name):
    """Return the entity block's text, or None when the entity is absent."""
    match = re.search(r"^(\s*)%s \{$" % re.escape(name), text, re.M)
    if not match:
        return None

    indent = len(match.group(1))
    lines = text[match.end():].split("\n")

    out = []
    for line in lines:
        if line.strip() and (len(line) - len(line.lstrip())) <= indent and line.strip() == "}":
            break
        out.append(line)

    return "\n".join(out)


def main():
    errors = 0

    for rel, name, description in EXPECTED:
        path = REPO_ROOT / rel
        if not path.is_file():
            print("  MISSING LAYER  %s (expected to hold %s)" % (rel, name))
            errors += 1
            continue

        block = block_for(path.read_text(encoding="utf-8", errors="replace"), name)
        if block is None:
            print("  MISSING ENTITY %s in %s" % (name, rel))
            print("                 %s" % description)
            print("                 If the site was deliberately removed, drop it from EXPECTED in this script.")
            errors += 1
            continue

        if not re.search(r"^\s*m_bLandIsolated 1\s*$", block, re.M):
            print("  NOT FLAGGED    %s in %s has no 'm_bLandIsolated 1'" % (name, rel))
            print("                 %s" % description)
            print("                 A Workbench re-save drops authored attributes silently. Re-add it, or")
            print("                 the occupying faction will make this an objective and march an")
            print("                 insertion at open water.")
            errors += 1

    if errors:
        print("check-land-isolated: FAILED (%d problem(s) across %d site(s))" % (errors, len(EXPECTED)))
        return 1

    print("check-land-isolated: OK (%d site(s) flagged)" % len(EXPECTED))
    return 0


if __name__ == "__main__":
    sys.exit(main())
