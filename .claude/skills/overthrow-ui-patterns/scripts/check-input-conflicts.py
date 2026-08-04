#!/usr/bin/env python3
"""Report keybind collisions inside each Overthrow ActionContext.

Two severities, because they are not the same bug:

  ERROR  Two actions listed in the SAME ActionContext share a physical input.
         Both listeners fire. On a gamepad the player presses one button and
         gets two behaviours. This is always a bug unless the two buttons are
         provably never on screen together.

  WARN   An action in this context shares an input with an always-active
         low-priority context (OverthrowGeneralContext, activated every frame
         by OVT_UIManagerComponent.EOnFrame). Context Priority is supposed to
         let the menu win, but it is worth knowing when a menu shortcut sits on
         top of a global one - test it on a pad before shipping.

Usage:
    check-input-conflicts.py [--mod-conf PATH] [--base-conf PATH] [--warnings] [--all]

Exit codes:
    0  no unacknowledged ERRORs
    1  ERRORs found
    2  a conf file could not be read/parsed
"""

import argparse
import re
import sys
from pathlib import Path

DEFAULT_MOD = "Configs/System/chimeraInputCommon.conf"
DEFAULT_BASE = "/mnt/n/Projects/Arma 4/ArmaReforger/Configs/System/chimeraInputCommon.conf"

# Activated every frame by OVT_UIManagerComponent.EOnFrame, so its actions are
# live even while a menu context is active.
ALWAYS_ACTIVE = ["OverthrowGeneralContext"]

# Collisions that already shipped. These ARE bugs - they are recorded so that a
# NEW conflict fails the check instead of drowning in known noise. They are
# reported as BASELINE and do not affect the exit code.
#
# When you touch one of these screens, fix its entry and delete the line. Never
# add to this set: a collision you are introducing is an ERROR.
BASELINE = {
    ("OverthrowJobsMenuContext", "gamepad0:a"),          # JobsAccept vs MenuSelect
    ("OverthrowJobsMenuContext", "gamepad0:b"),          # JobsDecline vs MenuBack
    ("OverthrowJobsMenuContext", "gamepad0:pad_up"),     # JobsShowOnMap vs MenuUp
    ("OverthrowLoadoutsContext", "gamepad0:a"),          # LoadoutsApply vs MenuSelect
    ("OverthrowMainMenuContext", "gamepad0:pad_down"),   # MainMenu vs MenuDown
    ("OverthrowRecruitsMenuContext", "gamepad0:pad_up"), # RecruitsShowOnMap vs MenuUp
    ("OverthrowRecruitsMenuContext", "keyboard:KC_D"),   # RecruitsDismiss vs MenuRight
    ("OverthrowResistanceMenuContext", "gamepad0:pad_left"),   # SendFunds vs MenuLeft
    ("OverthrowResistanceMenuContext", "gamepad0:pad_right"),  # SendMoney vs MenuRight
    ("OverthrowStartContext", "gamepad0:a"),             # StartGame vs MenuSelect
    ("OverthrowStartContext", "keyboard:KC_RETURN"),     # StartGame vs MenuSelect
    ("OverthrowVehicleMenuContext", "gamepad0:pad_down"),# VehicleMenu vs MenuDown
}

# Same-context collisions that are deliberate and safe. Each entry must carry
# the reason the double-fire cannot happen at runtime; a waiver without a
# mechanism is just a hidden bug. Key: (context, input) -> reason.
ACKNOWLEDGED = {
    ("OverthrowShopContext", "gamepad0:x"):
        "BuyButton and SellAllButton are never visible at the same time "
        "(OVT_ShopContext.RefreshActionButtons) and a hidden "
        "SCR_InputButtonComponent refuses its keybind.",
    ("OverthrowGeneralContext", "keyboard:KC_U"):
        "OverthrowVehicleMenu is a LSHIFT+U combo; plain U opens the main menu.",
    ("OverthrowGeneralContext", "gamepad0:pad_down"):
        "OverthrowVehicleMenu is a left_trigger+pad_down combo; plain pad_down "
        "opens the main menu.",
}


def parse_conf(path):
    """Return (actions, contexts) from a chimeraInputCommon.conf.

    actions:  name -> [input strings]
    contexts: name -> (priority, [action names])
    """
    text = Path(path).read_text(encoding="utf-8", errors="replace")

    if "\n Contexts {" not in text:
        raise ValueError(f"{path}: no Contexts block found")

    actions_part, contexts_part = text.split("\n Contexts {", 1)

    actions = {}
    for block in re.split(r"\n  Action ", actions_part)[1:]:
        name = block.split(" ", 1)[0].split("{")[0].strip()
        actions[name] = re.findall(r'Input "([^"]+)"', block)

    contexts = {}
    for block in re.split(r"\n  ActionContext ", contexts_part)[1:]:
        name = block.split(" ", 1)[0].split("{")[0].strip()
        priority = re.search(r"Priority (\d+)", block)
        refs = []
        if "ActionRefs {" in block:
            refs = re.findall(r'"(\w+)"', block.split("ActionRefs {", 1)[1].split("}", 1)[0])
        contexts[name] = (int(priority.group(1)) if priority else 0, refs)

    return actions, contexts


def index_by_input(refs, all_actions, unknown, context):
    """input string -> [action names], preserving order and de-duplicating."""
    by_input = {}
    for ref in refs:
        if ref not in all_actions:
            unknown.add(f"{context} -> {ref}")
            continue
        for inp in all_actions[ref]:
            by_input.setdefault(inp, [])
            if ref not in by_input[inp]:
                by_input[inp].append(ref)
    return by_input


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--mod-conf", default=DEFAULT_MOD)
    ap.add_argument("--base-conf", default=DEFAULT_BASE)
    ap.add_argument("--warnings", action="store_true",
                    help="also report overlaps with always-active contexts")
    ap.add_argument("--all", action="store_true",
                    help="imply --warnings and print acknowledged collisions too")
    args = ap.parse_args()

    show_warnings = args.warnings or args.all

    try:
        base_actions, _ = parse_conf(args.base_conf)
        mod_actions, mod_contexts = parse_conf(args.mod_conf)
    except (OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    # Mod definitions win, so a re-declared vanilla action reports its mod binding.
    all_actions = dict(base_actions)
    all_actions.update(mod_actions)

    unknown = set()
    errors = 0
    warnings = 0
    waived = 0
    baseline_hits = 0
    stale_baseline = set(BASELINE)

    for context in sorted(mod_contexts):
        priority, refs = mod_contexts[context]
        lines = []

        own = index_by_input(refs, all_actions, unknown, context)

        for inp, acts in sorted(own.items()):
            if len(acts) < 2:
                continue
            reason = ACKNOWLEDGED.get((context, inp))
            if reason:
                waived += 1
                if args.all:
                    lines.append(f"  OK    {inp:<30} {', '.join(acts)}")
                    lines.append(f"        acknowledged: {reason}")
                continue
            if (context, inp) in BASELINE:
                baseline_hits += 1
                stale_baseline.discard((context, inp))
                lines.append(f"  BASE  {inp:<30} {', '.join(acts)}")
                continue
            errors += 1
            lines.append(f"  ERROR {inp:<30} {', '.join(acts)}")

        if show_warnings and context not in ALWAYS_ACTIVE:
            for other in ALWAYS_ACTIVE:
                globals_by_input = index_by_input(
                    mod_contexts.get(other, (0, []))[1], all_actions, unknown, other)
                for inp, gacts in sorted(globals_by_input.items()):
                    if inp not in own:
                        continue
                    overlap = [a for a in own[inp] if a not in gacts]
                    if not overlap:
                        continue
                    warnings += 1
                    lines.append(f"  WARN  {inp:<30} {', '.join(overlap)}"
                                 f"  vs {other}: {', '.join(gacts)}")

        if lines:
            print(f"\n{context}  (priority {priority})")
            print("\n".join(lines))

    if unknown:
        print("\nUndefined action references (typo, or defined only in the base game):")
        for u in sorted(unknown):
            print(f"  {u}")

    if stale_baseline:
        print("\nBASELINE entries that no longer collide - delete them from this script:")
        for context, inp in sorted(stale_baseline):
            print(f"  {context} / {inp}")

    print()
    summary = (f"{errors} error(s), {warnings} warning(s), "
               f"{baseline_hits} pre-existing, {waived} acknowledged.")
    if not show_warnings:
        summary += "  (--warnings for always-active overlaps)"
    print(summary)

    if errors:
        print("\nFix an ERROR by rebinding one side, or by making the two buttons")
        print("mutually exclusive on screen and adding the mechanism to")
        print("ACKNOWLEDGED in this script. Do not add it to BASELINE.")
        return 1

    if baseline_hits:
        print("BASE entries are known pre-existing bugs, not a pass. Fix the one")
        print("on the screen you are working on and delete its BASELINE line.")

    return 0


if __name__ == "__main__":
    sys.exit(main())
