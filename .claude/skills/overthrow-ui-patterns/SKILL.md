---
name: overthrow-ui-patterns
description: Overthrow UI development - .layout authoring, WLib_NavigationButton actions, SCR_InputButtonComponent wiring, OVT_UIContext lifecycle, and chimeraInputCommon.conf keybindings that work on gamepad
version: 1.0.0
---

# Overthrow UI Patterns

How menus are actually built in this mod: a `.layout` file, an `OVT_UIContext`
subclass registered on the player prefab, and a set of named actions in
`chimeraInputCommon.conf` that give every on-screen action a visible key/pad
hint.

---

## When to Use This Skill

- Creating or editing a `.layout` file
- Adding a button, tab, list row or card to a menu
- Writing or changing an `OVT_UIContext` subclass
- Registering a keybinding, or changing which actions a screen listens to
- Reviewing a menu for gamepad / console usability

---

## The Five Rules

**1. Every action is a `WLib_NavigationButton` with a named action.**
Not a bare `ButtonWidgetClass`, not a `WLib_ButtonText`. The navigation button
draws the keyboard key and the pad glyph on its own chrome, so the binding is
discoverable without a manual - and it is the only button type a console player
can use without first hunting for it with a stick. See `navigation-buttons.md`.

**2. Bind the action, then check for conflicts with a script, not by eye.**

```bash
python3 .claude/skills/overthrow-ui-patterns/scripts/check-input-conflicts.py
```

Exit 0 = you introduced no new same-context collision, no input owned by an
always-live base context, and no layout button bound to an action no conf
defines. It parses both confs — including the base game's 197 actions declared
**inline** inside `ActionContext` blocks, which appear in no `ActionRefs` list —
so it knows both which vanilla `Menu*` actions your context pulled in and that
`shoulder_left` belongs to VON. The `BASE` baseline is empty as of 2026-08-08;
every shipped collision was rebound. `--warnings` adds overlaps with the
always-active global context. See `keybindings.md`.

**3. `KC_W` `KC_A` `KC_S` `KC_D` are reserved — and so are `a`, `b`, the d-pad
and `shoulder_left`.** The base game binds WASD to `MenuUp` / `MenuLeft` /
`MenuDown` / `MenuRight` alongside the arrow keys, and the d-pad + `a` + `b` to
the same nav actions on a pad. Any context that lists those nav actions - i.e.
every menu that wants a gamepad to work - has already spent them.
`shoulder_left` is worse: `VONContext` holds it at **priority 110**, live every
frame the player is alive, so a menu at priority 50 never sees the press. Put
menu verbs on `x` / `y` / `shoulder_right` / the stick clicks. Full reserved
table in `keybindings.md`.

**4. Hiding a `WLib_NavigationButton` also disables its shortcut.**
`SCR_InputButtonComponent.OnInput()` bails when the widget is not
`IsVisibleInHierarchy()`. This is load-bearing, not incidental: it is what lets
two buttons share one pad input safely, provided they are never on screen
together. `SetVisible(false)` is the supported way to retire a shortcut.

**5. `OnClose` removes exactly what `OnShow` inserted.**
Widget handlers die with the layout, but invokers on managers and controllers
outlive it and will accumulate one subscription per menu open. See
`ui-contexts.md`.

---

## Anatomy of a Menu

Four files, in the order you should create them:

| # | File | Purpose |
|---|------|---------|
| 1 | `Configs/System/chimeraInputCommon.conf` | Named actions + the `ActionContext` listing them |
| 2 | `Language/localization_Overthrow.st` | `#OVT-` keys for every label |
| 3 | `UI/Layouts/Menu/<Name>.layout` (+ `.meta`) | The widget tree |
| 4 | `Scripts/Game/UI/Context/OVT_<Name>Context.c` | Lifecycle, wiring, refresh |

Then register the context on `Prefabs/Characters/Factions/INDFOR/FIA/Character_Player.et`
under `OVT_UIManagerComponent.m_aContexts` - a menu that is not in that array
never initialises. `ui-contexts.md` has the block to copy.

---

## Reference Files

| File | Read it when |
|------|--------------|
| `layouts.md` | Authoring `.layout` / `.meta`, GUID rules, inheriting WLib layouts |
| `navigation-buttons.md` | Adding a button, wiring `m_OnActivated`, enable/disable/visibility |
| `keybindings.md` | Adding an action, picking keys/pad inputs, contexts, conflict checking |
| `ui-contexts.md` | `OVT_UIContext` lifecycle, registration, refresh, teardown |
| `widget-components.md` | Dynamic lists - cards, tabs, rows instantiated at runtime |

---

## Verification

Compile is checkable; the runtime is not.

```bash
tools/compile-check.sh                      # script changes
python3 .claude/skills/overthrow-ui-patterns/scripts/check-input-conflicts.py
```

Layout and `.conf` edits are **not** covered by compile-check or the test
suites - UI is explicitly outside the automated test spine. Every layout change
needs a human in the Workbench. When you hand work back, name the screen, the
buttons to press, and **the pad inputs to try**, not just "test the menu".

Reference implementation to copy from: `OVT_ShopContext.c` +
`UI/Layouts/Menu/ShopMenu.layout` + `OverthrowShopContext` in the conf. It is
the newest and the only one built with gamepad cycling (`OverthrowShopPrevCategory` /
`NextCategory`) from the start.
