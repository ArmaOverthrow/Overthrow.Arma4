#!/usr/bin/env python3
"""Report keybind collisions inside each Overthrow ActionContext.

Three severities, because they are not the same bug:

  ERROR  Two actions listed in the SAME ActionContext share a physical input,
         OR an input this context uses is already owned by an always-live base
         game context with a HIGHER priority. The first double-fires; the
         second silently loses the input to the base game.

  COMBO  An action fires only as a modifier combo (LT+pad_down), but one of the
         combo's own buttons is separately bound in the same context. Pressing
         the combo therefore also fires that other action. Not a double-fire on
         a plain press - vanilla ships this shape too (HintDismiss is pad_left+y
         over live pad_left/y) - but worth knowing before shipping.

  WARN   An action in this context shares an input with an always-active
         low-priority Overthrow context (OverthrowGeneralContext, activated
         every frame by OVT_UIManagerComponent.EOnFrame). Context Priority is
         supposed to let the menu win, but it is worth knowing when a menu
         shortcut sits on top of a global one - test it on a pad before
         shipping.

Both confs are parsed the same way, and that parse is inline-aware: the base
game declares 197 of its actions INSIDE ActionContext blocks
(`ActionContext X { Actions { Action Y { ... } } }`) rather than in the
top-level `Actions` block. Those actions appear in no ActionRefs list. An
earlier version of this script could not see them at all, which is why it
reported `gamepad0:shoulder_left` as free when VON owns it at priority 110
(BUG-092). Never treat a clean run as proof that an input is unbound without
also grepping both confs for the raw input string.

It also checks the other direction: every `m_sActionName` a `.layout` binds to a
`SCR_InputButtonComponent` must be an action some conf actually defines. A
button naming an undefined action still works by mouse and by focus+MenuSelect,
so it looks fine in the Workbench - it just silently has no keybind and draws no
pad glyph, which is invisible until a console player cannot reach it.

Usage:
    check-input-conflicts.py [--mod-conf PATH] [--base-conf PATH] [--layouts DIR]
                             [--warnings] [--all]

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
DEFAULT_LAYOUTS = "UI/Layouts"

# Activated every frame by OVT_UIManagerComponent.EOnFrame, so its actions are
# live even while a menu context is active.
ALWAYS_ACTIVE = ["OverthrowGeneralContext"]

# Base game contexts that script activates unconditionally every frame, so their
# inputs are spent no matter what Overthrow has open. Priority is read from the
# base conf, not hardcoded here - only membership is a judgement call.
#
# VONContext: SCR_VONController.Update calls ActivateContext(VON_CONTEXT) every
# frame the player is alive and conscious. At priority 110 it outranks every
# Overthrow menu context (50), so gamepad0:shoulder_left and keyboard:KC_T are
# NOT available to a menu.
#
# Everything else in the base conf (Inventory 120, RadialMenu 100, Dialog 51,
# GadgetMap 55, TaskList*) is activated only while that specific UI is open, so
# it cannot be live at the same time as an Overthrow menu.
ALWAYS_LIVE_BASE = ["VONContext"]

# Collisions that already shipped. These ARE bugs - they are recorded so that a
# NEW conflict fails the check instead of drowning in known noise. They are
# reported as BASELINE and do not affect the exit code.
#
# When you touch one of these screens, fix its entry and delete the line. Never
# add to this set: a collision you are introducing is an ERROR.
#
# Emptied 2026-08-08 (BUG-082 / BUG-092 / BUG-093): all twelve were rebound off
# the navigation inputs. Keep the mechanism, not the entries.
BASELINE = set()

# Same-context collisions that are deliberate and safe. Each entry must carry
# the reason the double-fire cannot happen at runtime; a waiver without a
# mechanism is just a hidden bug. Key: (context, input) -> reason.
ACKNOWLEDGED = {
    ("OverthrowShopContext", "gamepad0:x"):
        "BuyButton and SellAllButton are never visible at the same time "
        "(OVT_ShopContext.RefreshActionButtons) and a hidden "
        "SCR_InputButtonComponent refuses its keybind.",
    # DOCUMENTARY ONLY - this entry can never be matched, and that is the point.
    # A waiver is consulted only when two actions in the SAME mod context share an
    # input; here the collision is CROSS-context, against an action declared inline
    # inside vanilla's MapContext, which this parser cannot see (the ~197-inline-
    # action blind spot). Kept so the reasoning is written down where the next
    # person to touch gamepad0:x will look, not because the checker enforces it.
    # Verified by hand 2026-08-11 (map/respawn Phase 7).
    ("OverthrowRespawnContext", "gamepad0:x"):
        "The respawn screen also activates vanilla MapContext, whose "
        "MapContextualMenu is on gamepad0:x. Only SCR_MapRadialUI and "
        "SCR_MapDrawingUI listen for MapContextualMenu, and neither module is "
        "carried by Configs/Map/MapRespawn.conf, so it has no handler on this "
        "screen. Re-check if either module is ever added to that config.",
}


def _balanced(text, open_index):
    """Index just past the '}' matching the '{' at open_index."""
    depth = 0
    for i in range(open_index, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return i + 1
    return len(text)


def _blocks(text, keyword):
    """Yield (name, body) for every `<keyword> <Name> ... { ... }` in text.

    Handles the `Name : "{GUID}path.conf" {` external-reference form by simply
    returning whatever body follows - those bodies are empty in practice.
    """
    for m in re.finditer(r"\n\s*%s\s+(\w+)" % keyword, text):
        brace = text.find("{", m.end())
        if brace == -1:
            continue
        end = _balanced(text, brace)
        yield m.group(1), text[brace:end]


def extract_inputs(block):
    """Input tokens for one Action block.

    A plain `InputSourceValue` yields its raw input string. An
    `InputSourceCombo` yields ONE composite token, "a+b", because the action
    fires only when every part is held - it does not collide with a plain press
    of either part. The parts stay visible in the token so the COMBO check can
    still see them.
    """
    tokens = []
    consumed = [False] * len(block)

    for m in re.finditer(r'InputSourceCombo\s+"[^"]*"\s*\{', block):
        brace = block.rfind("{", m.start(), m.end())
        end = _balanced(block, brace)
        if any(consumed[m.start():end]):
            continue  # nested inside a combo already taken
        parts = re.findall(r'Input "([^"]+)"', block[brace:end])
        if parts:
            tokens.append("+".join(parts))
        for i in range(m.start(), end):
            consumed[i] = True

    for m in re.finditer(r'Input "([^"]+)"', block):
        if not consumed[m.start()]:
            tokens.append(m.group(1))

    return tokens


def parse_conf(path):
    """Return (actions, contexts) from a chimeraInputCommon.conf.

    actions:  name -> [input tokens]        (top-level AND inline declarations)
    contexts: name -> (priority, [action names])
    """
    text = Path(path).read_text(encoding="utf-8", errors="replace")

    if "\n Contexts {" not in text:
        raise ValueError(f"{path}: no Contexts block found")

    actions_part, contexts_part = text.split("\n Contexts {", 1)

    actions = {}
    for name, body in _blocks(actions_part, "Action"):
        actions[name] = extract_inputs(body)

    contexts = {}
    for name, body in _blocks(contexts_part, "ActionContext"):
        priority = re.search(r"Priority (\d+)", body)
        refs = []
        if "ActionRefs {" in body:
            brace = body.index("{", body.index("ActionRefs {") + len("ActionRefs"))
            refs = re.findall(r'"(\w+)"', body[brace:_balanced(body, brace)])
        # Actions declared inline in this context. They are live here and appear
        # in no ActionRefs list anywhere - this is the parse the old script
        # lacked, and the reason it under-reported.
        inline = re.search(r"\n\s*Actions\s*\{", body)
        if inline:
            brace = body.index("{", inline.end() - 1)
            for aname, abody in _blocks(body[brace:_balanced(body, brace)], "Action"):
                actions.setdefault(aname, extract_inputs(abody))
                refs.append(aname)
        contexts[name] = (int(priority.group(1)) if priority else 0, refs)

    return actions, contexts


def layout_actions(layouts_dir):
    """(file, action name) for every m_sActionName in every .layout under dir."""
    root = Path(layouts_dir)
    if not root.is_dir():
        return []
    found = []
    for path in sorted(root.rglob("*.layout")):
        text = path.read_text(encoding="utf-8", errors="replace")
        for m in re.finditer(r'm_sActionName\s+"([^"]+)"', text):
            found.append((path, m.group(1)))
    return found


def index_by_input(refs, all_actions, unknown, context):
    """input token -> [action names], preserving order and de-duplicating."""
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
    ap.add_argument("--layouts", default=DEFAULT_LAYOUTS,
                    help="directory scanned for .layout m_sActionName references")
    ap.add_argument("--warnings", action="store_true",
                    help="also report overlaps with always-active contexts")
    ap.add_argument("--all", action="store_true",
                    help="imply --warnings and print acknowledged collisions too")
    args = ap.parse_args()

    show_warnings = args.warnings or args.all

    try:
        base_actions, base_contexts = parse_conf(args.base_conf)
        mod_actions, mod_contexts = parse_conf(args.mod_conf)
    except (OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    # Mod definitions win, so a re-declared vanilla action reports its mod binding.
    all_actions = dict(base_actions)
    all_actions.update(mod_actions)

    # input token -> [(priority, base context, [actions])] for contexts that are
    # live every frame regardless of what Overthrow has open.
    live_base = {}
    for cname in ALWAYS_LIVE_BASE:
        if cname not in base_contexts:
            print(f"error: ALWAYS_LIVE_BASE names {cname}, which the base conf "
                  f"does not define", file=sys.stderr)
            return 2
        priority, refs = base_contexts[cname]
        for inp, acts in index_by_input(refs, base_actions, set(), cname).items():
            live_base.setdefault(inp, []).append((priority, cname, acts))

    unknown = set()
    errors = 0
    warnings = 0
    combos = 0
    waived = 0
    baseline_hits = 0
    stale_baseline = set(BASELINE)

    for context in sorted(mod_contexts):
        priority, refs = mod_contexts[context]
        lines = []

        own = index_by_input(refs, all_actions, unknown, context)

        for inp, acts in sorted(own.items()):
            # Owned by an always-live base context that outranks this one.
            for base_pri, base_ctx, base_acts in live_base.get(inp, []):
                if base_pri <= priority:
                    continue
                errors += 1
                lines.append(f"  ERROR {inp:<30} {', '.join(acts)}"
                             f"  vs {base_ctx} (priority {base_pri}, always live): "
                             f"{', '.join(base_acts)}")

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

        # A combo does not fire on a plain press of one of its buttons, but
        # pressing the combo still fires whatever those buttons are bound to.
        for inp, acts in sorted(own.items()):
            if "+" not in inp:
                continue
            for part in inp.split("+"):
                others = [a for a in own.get(part, []) if a not in acts]
                if others:
                    combos += 1
                    lines.append(f"  COMBO {inp:<30} {', '.join(acts)}"
                                 f"  also fires {', '.join(others)} ({part})")

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

    # A layout button naming an action no conf defines has no keybind and draws
    # no pad glyph. It still works by mouse and by focus+MenuSelect, so nothing
    # looks broken until a console player tries to reach it.
    dangling = sorted({(str(p), a) for p, a in layout_actions(args.layouts)
                       if a not in all_actions})
    if dangling:
        print("\nLayout buttons bound to an action no conf defines "
              "(no keybind, no pad glyph):")
        for path, action in dangling:
            errors += 1
            print(f"  ERROR {action:<36} {path}")

    if stale_baseline:
        print("\nBASELINE entries that no longer collide - delete them from this script:")
        for context, inp in sorted(stale_baseline):
            print(f"  {context} / {inp}")

    print()
    summary = (f"{errors} error(s), {warnings} warning(s), {combos} combo note(s), "
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
