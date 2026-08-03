# Keybindings — chimeraInputCommon.conf

`Configs/System/chimeraInputCommon.conf` has two halves: an `Actions` block
naming every input Overthrow can respond to, and a `Contexts` block saying which
actions are live on which screen.

An action does nothing until (a) a context lists it and (b) that context is
activated — `OVT_UIContext.EOnFrame` calls
`m_InputManager.ActivateContext(m_sContextName)` every frame the menu is open.

---

## Adding an Action

```
  Action OverthrowShopNextCategory {
   Type Digital
   InputSource InputSourceSum "{6A7C4E1D00000007}" {
    Sources {
     InputSourceValue "{6A7C4E1D00000008}" {
      Input "keyboard:KC_E"
      Filter InputFilterClick "{6A7C4E1D00000009}" {
      }
     }
     InputSourceValue "{6A7C4E1D0000000A}" {
      Input "gamepad0:shoulder_right"
      Filter InputFilterClick "{6A7C4E1D0000000B}" {
      }
     }
    }
   }
  }
```

Rules:

- **Name it `Overthrow<Screen><Verb>`** — `OverthrowShopSellAll`,
  `OverthrowRecruitsDismiss`. The screen prefix is what keeps the flat global
  namespace navigable.
- **Always bind both a keyboard and a `gamepad0:` source.** A keyboard-only
  action is invisible and unusable on console — the navigation button renders no
  glyph for it in pad mode.
- **`InputSourceSum` = alternatives (OR).** Each `InputSourceValue` is one way to
  trigger the action. For a *combo* (modifier + key), see `OverthrowVehicleMenu`
  in the conf, which uses `keyboard:KC_LSHIFT` + `keyboard:KC_U`.
- **`InputFilterClick`** = fires once on press. This is what you want for a menu
  button. Hold/continuous behaviour is a `SCR_InputButtonComponent` attribute
  (`m_bIsHoldAction`), not a filter change.
- **GUIDs must be unique in the file.** Pick an unused 16-hex-digit base and
  increment. Existing Overthrow actions reuse GUIDs across actions
  (`OverthrowWarehouseTakeOne` and `OverthrowPortBuyTen` share
  `{5D48A6A7AE022164}`) — that is pre-existing and works, but do not copy it;
  duplicate GUIDs make a conf impossible to diff or merge cleanly.

---

## Reserved Inputs

The base game (`/mnt/n/Projects/Arma 4/ArmaReforger/Configs/System/chimeraInputCommon.conf`)
binds these. Any context that lists the action has spent the input:

| Action | Keyboard | Gamepad |
|---|---|---|
| `MenuUp` | `KC_UP`, **`KC_W`** | `pad_up`, `left_thumb_vertical+` |
| `MenuDown` | `KC_DOWN`, **`KC_S`** | `pad_down`, `left_thumb_vertical-` |
| `MenuLeft` | `KC_LEFT`, **`KC_A`** | `pad_left`, `left_thumb_horizontal-` |
| `MenuRight` | `KC_RIGHT`, **`KC_D`** | `pad_right`, `left_thumb_horizontal+` |
| `MenuSelect` | `KC_RETURN`, `KC_NUMPADENTER` | `a` |
| `MenuBack` | `KC_ESCAPE` | `b` |
| `MenuTabLeft` | `KC_Q` | `shoulder_left` |
| `MenuTabRight` | `KC_E` | `shoulder_right` |
| `MenuNavLeft` | `KC_Z`, `mouse:wheel+` | `left_trigger` |
| `MenuNavRight` | `KC_C`, `mouse:wheel-` | `right_trigger` |

**The WASD trap.** `W A S D` are menu navigation, not free letters. A "Sell"
button on `KC_S` collides with `MenuDown` in any context that supports gamepad
navigation — which is every menu. Overthrow shipped that exact bug in
`OverthrowShopContext` until `OverthrowShopSell` was moved to `KC_F`.

**Safe-ish keyboard letters** for menu verbs, given the above:
`B E F G H I J K L N O P R T U V X Y` — minus whatever the screen's own actions
already took, and minus `Q`/`E` if the screen lists `MenuTabLeft`/`Right`.

**Gamepad face buttons.** `a` and `b` belong to `MenuSelect` and `MenuBack`.
That leaves `x` and `y` as the only free face buttons — which is why Overthrow's
menus put their two primary verbs there (`OverthrowShopBuy` = `x`,
`OverthrowShopSell` = `y`). For a third and fourth verb, use `shoulder_left` /
`shoulder_right` (paged/tabbed navigation, matching the `MenuTab*` convention
players already know) before reaching for the d-pad, which usually collides with
`Menu*` navigation.

---

## Adding a Context

```
  ActionContext OverthrowShopContext {
   Priority 50
   Flags 4
   ActionRefs {
    "OverthrowShopBuy"
    "OverthrowShopSell"
    "MenuNavLeft"
    "MenuNavRight"
    "MenuBack"
    "MenuSelect"
    "MenuLeft"
    "MenuRight"
    "MenuUp"
    "MenuDown"
   }
  }
```

- **`Priority 50` / `Flags 4` for a menu context**; `Priority 10` / `Flags 2`
  for a world/gameplay context (`OverthrowPlaceContext`, `OverthrowBuildContext`).
  The base game uses `Flags 0` everywhere and the field is engine-side with no
  scripted enum — follow the existing Overthrow convention rather than inventing
  a value.
- **List the `Menu*` nav actions you need.** A context that omits `MenuUp`/`Down`/
  `Left`/`Right`/`MenuSelect` cannot be driven by a stick or d-pad at all: the
  player can see the menu and not move in it. This is the single most common
  console-blocking omission.
- **Always list `MenuBack`.** It is the context's close action
  (`m_sCloseAction "MenuBack"` on the prefab) and it is `b` / Escape, which is
  what every player tries first.
- The context name must match `m_sContextName` on the registered `OVT_UIContext`.

`OverthrowGeneralContext` (`Priority 10`) is activated **every frame** by
`OVT_UIManagerComponent.EOnFrame`, so its actions (`OverthrowMainMenu`,
`OverthrowVehicleMenu`) are live even inside a menu. Priority should let the
menu win, but treat an overlap with it as worth pad-testing.

---

## Conflict Checking

```bash
python3 .claude/skills/overthrow-ui-patterns/scripts/check-input-conflicts.py
python3 .claude/skills/overthrow-ui-patterns/scripts/check-input-conflicts.py --warnings
python3 .claude/skills/overthrow-ui-patterns/scripts/check-input-conflicts.py --all
```

- **ERROR** — a *new* same-context collision. Both listeners fire; one pad press
  does two things. Exit code 1.
- **BASE** — a same-context collision that already shipped (see below). Reported
  every run so it stays visible, but does not fail the check.
- **WARN** (`--warnings`) — overlap with the always-active
  `OverthrowGeneralContext`. Priority should suppress it; verify on a pad.
- **OK** (`--all`) — an acknowledged-safe collision.

The script also reports `BASELINE` entries that no longer collide, so a fix
cannot silently rot into a stale waiver.

When a same-context collision is genuinely safe, add it to `ACKNOWLEDGED` in the
script **with the mechanism that prevents the double-fire**. The only mechanism
that actually works is mutual exclusivity on screen:

```python
("OverthrowShopContext", "gamepad0:x"):
    "BuyButton and SellAllButton are never visible at the same time "
    "(OVT_ShopContext.RefreshActionButtons) and a hidden "
    "SCR_InputButtonComponent refuses its keybind.",
```

A waiver without a mechanism is a hidden bug. If you cannot name the code that
guarantees separation, rebind instead.

### Known pre-existing collisions

12 same-context collisions already ship. They are listed in the script's
`BASELINE` set so a *new* conflict fails the check instead of drowning in known
noise — but they are bugs, not a pass. **Never add to `BASELINE`.** When you work
on one of these screens, fix its collision and delete the line:

| Context | Input | Actions |
|---|---|---|
| `OverthrowJobsMenuContext` | `a` / `b` / `pad_up` | Accept/Decline/ShowOnMap vs `MenuSelect`/`MenuBack`/`MenuUp` |
| `OverthrowRecruitsMenuContext` | `pad_up`, `KC_D` | ShowOnMap vs `MenuUp`; Dismiss vs `MenuRight` |
| `OverthrowResistanceMenuContext` | `pad_left`, `pad_right` | SendFunds/SendMoney vs `MenuLeft`/`MenuRight` |
| `OverthrowStartContext` | `a`, `KC_RETURN` | `OverthrowStartGame` vs `MenuSelect` |
| `OverthrowLoadoutsContext` | `a` | `OverthrowLoadoutsApply` vs `MenuSelect` |
| `OverthrowMainMenuContext`, `OverthrowVehicleMenuContext` | `pad_down` | open action vs `MenuDown` |

Pattern worth noticing: **binding a menu verb to `a` is almost always wrong.**
`MenuSelect` already activates the focused button, so a gamepad player pressing
`a` on the focused Accept button fires Accept twice. Put the verb on `x`/`y` and
let `MenuSelect` handle "press the thing I'm pointing at".

---

## Console UX Checklist

Before calling a screen done:

- [ ] Every on-screen action is a `WLib_NavigationButton` with `m_sActionName` set
- [ ] Every action has a `gamepad0:` source
- [ ] Context lists `MenuUp/Down/Left/Right`, `MenuSelect`, `MenuBack`
- [ ] Conflict checker exits 0
- [ ] No verb bound to `a` or `b`
- [ ] Paging/tabs on `shoulder_left`/`shoulder_right`, not the d-pad
- [ ] Buttons that are contextually unavailable are `SetVisible(false)` or
      `SetEnabled(false)` — never left live to fail silently
- [ ] Labels are `#OVT-` localization keys, so the glyph row reads correctly in
      every language
