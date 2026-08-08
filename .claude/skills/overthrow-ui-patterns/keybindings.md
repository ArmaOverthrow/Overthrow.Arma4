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

**The always-live trap.** The table above only bites when your context *lists*
the action. Two base contexts are activated every frame by script no matter what
Overthrow has open, so their inputs are gone unconditionally:

| Context | Priority | Inputs it owns | Activated by |
|---|---|---|---|
| `VONContext` | **110** | `KC_T`, `KC_CAPITAL`, **`gamepad0:shoulder_left`** (+ LB combos with `a`/`b`) | `SCR_VONController.Update` — every frame the player is alive and conscious |
| `GlobalContext` | 10000 | mouse axes/buttons, `gamepad0:touch` | engine |

110 outranks every Overthrow menu context (50), so an Overthrow verb on
`shoulder_left` is not a double-fire — it is a **press that never arrives**,
while the radio keys instead. That was BUG-092 (`OverthrowShopPrevCategory`,
`OverthrowWarehouseTakeAll`, `OverthrowRotateLeft`). `LB` is not a spare button.

**Safe-ish keyboard letters** for menu verbs, given the above:
`B F G H I J K L N O P R U V X Y` — minus whatever the screen's own actions
already took, minus `Q`/`E` if the screen lists `MenuTabLeft`/`Right`, minus
`Z`/`C` if it lists `MenuNavLeft`/`Right`, and **never `T`** (VON push-to-talk).

**Gamepad buttons.** `a` and `b` belong to `MenuSelect` and `MenuBack`; the
d-pad and left stick belong to `Menu*` navigation; `shoulder_left` belongs to
VON. What is actually left for a menu context at priority 50:

| Input | Notes |
|---|---|
| `x`, `y` | the two free face buttons — put the primary verbs here |
| `shoulder_right` | third verb / next-page; only lost to `MenuTabRight` if the context lists it |
| `thumb_left`, `thumb_right` | stick clicks; free unless the context lists nothing else on them |
| `left_trigger`, `right_trigger` | free **unless** the context lists `MenuNavLeft`/`Right` (vanilla pagination widgets use those — see `PagingButtons.layout`) |
| `view` | free at priority 50; every base binding on it lives at 10–40 or in a menu that cannot be open at the same time. Odd glyph, so use it last |
| `menu` | do **not** use — engine pause menu |

**Out of buttons?** Use a modifier combo (`InputSourceCombo`), the way
`OverthrowMainMenu` is `left_trigger + pad_down`. A combo does not fire on a
plain press of either part, so it dodges the collision — but pressing it *does*
still fire whatever those parts are bound to. Vanilla ships that shape too
(`HintDismiss` is `pad_left + y` over live `pad_left`/`y`). The checker reports
it as a `COMBO` note rather than an error.

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

- **ERROR** — either a *new* same-context collision (both listeners fire; one pad
  press does two things), or an input owned by an always-live base context of
  higher priority (the press never reaches your menu). Exit code 1.
- **COMBO** — an action fires only as a modifier combo, but one of the combo's
  own buttons is separately bound in the same context, so pressing the combo
  also fires that action. Informational; exit code unaffected.
- **BASE** — a same-context collision that already shipped. The set is **empty**
  as of 2026-08-08; the mechanism is kept so a future sweep can use it.
- **WARN** (`--warnings`) — overlap with the always-active
  `OverthrowGeneralContext`. Priority should suppress it; verify on a pad.
- **OK** (`--all`) — an acknowledged-safe collision.

The script also reports `BASELINE` entries that no longer collide, so a fix
cannot silently rot into a stale waiver.

**It checks layouts too.** Every `m_sActionName` under `UI/Layouts` must name an
action some conf defines, or it is an ERROR. A button bound to an undefined
action still works by mouse and by focus+`MenuSelect`, so it looks fine in the
Workbench — it just has no keybind and draws no pad glyph, which is invisible
until a console player tries to reach it. `LoadoutsMenu.layout` shipped two of
these (`OverthrowLoadoutsApplyToRecruit`, `OverthrowLoadoutsApplyToAll`) until
2026-08-08.

**What the parse does and does not see.** It reads both confs' top-level
`Actions` blocks, their `ActionRefs` lists, **and** the 197 actions the base game
declares inline inside `ActionContext` blocks
(`ActionContext X { Actions { Action Y { … } } }`). That inline half was invisible
to the script until 2026-08-08, which is why it once reported `shoulder_left` and
`KC_T` as free (BUG-092). It still does not model cross-context priority for
*conditionally* activated base contexts — only the unconditional ones listed in
`ALWAYS_LIVE_BASE`. When you pick a new input, grep both confs for the raw string
as well.

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

### Current gamepad map

Rebound 2026-08-08 (BUG-082 / BUG-092 / BUG-093) — `BASELINE` is empty and every
verb is off the navigation inputs:

| Screen | `x` | `y` | Other |
|---|---|---|---|
| Global | — | — | `LT`+`pad_down` main menu, `LT`+`pad_up` vehicle menu |
| Shop | Buy / Sell All¹ | Sell | `L3` prev category, `R3` next category, `RB` buy mode, `view` sell mode, `LT`/`RT` page (`MenuNav*`) |
| Warehouse | take 1 | take 10 | `RB` take 100, `RT` take all |
| Port | buy 10 | buy 100 | — |
| Jobs | Accept | Decline | `RB` show on map |
| Recruits | Rename | Dismiss | `RB` show on map |
| Loadouts | Apply | Delete | `RB` apply to selected recruit, `RT` apply to all recruits |
| Resistance | Make officer | Donate | `LT` send resistance funds, `RT` send own money |
| Start | Continue save | Start game | — |
| Camp | Toggle privacy | Delete | — |
| Manage vehicle | Upgrade | Repair | — |
| Place / Build | Rotate left | Rotate right | `pad_left`/`pad_right` prev/next item |

¹ mutually exclusive on screen — the only entry in `ACKNOWLEDGED`.

Patterns worth keeping: **binding a menu verb to `a` is almost always wrong** —
`MenuSelect` already activates the focused button, so a gamepad player pressing
`a` on the focused Accept button fires Accept twice. **Never `shoulder_left`** —
VON outranks you and eats the press. **Never the d-pad** — it is `Menu*`
navigation in every screen that a pad can drive at all.

---

## Console UX Checklist

Before calling a screen done:

- [ ] Every on-screen action is a `WLib_NavigationButton` with `m_sActionName` set
- [ ] Every action has a `gamepad0:` source
- [ ] Context lists `MenuUp/Down/Left/Right`, `MenuSelect`, `MenuBack`
- [ ] Conflict checker exits 0
- [ ] No verb bound to `a`, `b`, the d-pad, or `shoulder_left`
- [ ] Paging/tabs on `shoulder_right` or the stick clicks, not the d-pad
- [ ] Buttons that are contextually unavailable are `SetVisible(false)` or
      `SetEnabled(false)` — never left live to fail silently
- [ ] Labels are `#OVT-` localization keys, so the glyph row reads correctly in
      every language
