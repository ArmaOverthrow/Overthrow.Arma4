# Map Layers & Legend — Incidental Findings

**Raised by:** `map/map-layers` Phases 3, 4b and 6 (2026-08-11) + the Phase 7 gamepad pass (F7, 2026-08-13)
**Status:** ✅ **All filed as bugs 2026-08-13 — F1…F7 = BUG-149 … BUG-155.** (The write-ups predicted BUG-146 onward, but parallel sessions had taken BUG-146…148 by filing time — the re-check-the-highest-id rule in action.) None is fixed; the bug files are the tracking authority from here.

| Finding | Bug | linkedFeature |
|---|---|---|
| F1 vestigial tool-menu entry | BUG-149 | map/core |
| F2 five raw-English display names | BUG-150 | map/core |
| F3 SetOpacity(0) never restored + stale m_Widgets | BUG-151 | map/core |
| F4 unguarded config deref in OnMapOpen | BUG-152 | map/core |
| F5 four duplicate .st GUIDs | BUG-153 | dev-ops |
| F6 info-panel close button likely pad-dead | BUG-154 | map/core |
| F7 D-pad Left unticks + exits the panel | BUG-155 | map/map-layers |

> **None of these findings was introduced by `map/map-layers`.** Every one is either pre-existing at `HEAD`
> or a property of a file this feature merely passed through. They are recorded because they were found, not
> because they are this feature's to fix — and deliberately **not** fixed here, because repairing unrelated
> code inside a file a feature is already editing is how a small, reviewable change becomes an ambiguous one.
> F1, F3 and F4 all live in `OVT_MapPlayerLocation`, which Phase 3 edited under a standing *"leave the class
> otherwise exactly as-is"* rule; that rule is what preserved them intact and legible.
>
> ❌ **No `file:line` pointers below** (epic rule K-9: keep the rationale, name the symbol, drop the pointer).
> A `file:line` in a doc or comment is a reference with a human reader instead of a compiler — nothing ever
> re-resolves it, `tools/compile-check.sh` cannot see it, and a wrong one sends the next reader to the wrong
> place with full confidence. This epic has already caught three dangling ones, and **F2's own line numbers
> went stale inside this feature in under a day.** Symbols below are unique enough to `grep` for.

---

## F1 — `OVT_MapPlayerLocation` carries a vestigial tool-menu entry that is never assigned

**Symptom.** The class declares `protected SCR_MapToolEntry m_ToolMenuEntry` that **nothing ever assigns**;
its `override void Init()` is **empty**; and `protected void ZoomInOnPlayer()` — whose only other reference
is a **commented-out** `m_ToolMenuEntry.SetColor(...)` line inside its own body — has **zero callers**
(`grep -rn "ZoomInOnPlayer" Scripts/` returns exactly one hit, the declaration itself). The shape is
unmistakable: someone started to register a "centre the map on me" tool-menu entry and stopped.

**Where.** `Scripts/Game/UI/Map/Visualization/OVT_MapPlayerLocation.c` — symbols `m_ToolMenuEntry`,
`Init()`, `ZoomInOnPlayer()`.

**Why it matters beyond tidiness.** It reads as a *working precedent for tool-menu registration* and it is
not one. `map/map-layers` planning had to establish this by grep before designing its entry point (K3),
because a plan that trusted it would have copied a pattern that has never executed. The only working
precedents in the tree are vanilla's nine registrants.

**Why it was not fixed here.** Phase 3 edited this class to add `m_bMarkersVisible` and
`IsAvailableThisSession()` under an explicit instruction to leave everything else bit-identical — and
verified by `git diff` that the three vestigial symbols are unchanged. Deleting dead code in a file this
feature was already editing would have mixed a deletion into a behaviour change with no way to review them
apart.

**The decision the user has to make** (a *choice*, not a defect with one right answer): **delete it**, or
**wire it up** as a second tool-menu entry beside the layers entry this feature just added. The second is
now cheap — `OVT_MapLayersUI` contains a working, commented `RegisterToolMenuEntry` call to copy, including
the K7 trap (register in `Init()`, never in `OnMapOpen`, because `SCR_MapToolMenuUI.m_aMenuEntries` is only
ever inserted into and has no unregister API).

**Suggested severity: Low** — dead code, zero runtime effect, but with a real documented cost already paid
once in misleading a plan.

---

## F2 — 5 of 14 `m_sDisplayName` values are raw English literals and render untranslated today

**Symptom.** `Configs/Map/OverthrowMap.conf` sets `m_sDisplayName` on all 14 location types, and five are
**raw English strings rather than localization keys**: `"Town"`, `"Bus Stop"`, `"Vehicle"`,
`"Point of Interest"`, `"Waypoint"`. `m_sDisplayName` drives the **type line on the map info panel**, so
those five render in English on every non-English client **today**, independently of this feature.

**Where.** `Configs/Map/OverthrowMap.conf` — the `m_sDisplayName` entries in the Town, Bus Stop, Vehicle,
Point of Interest and Waypoint `OVT_MapLocationType` blocks. Consumed by
`OVT_MapLocationType.GetDisplayName()`. (⚠️ The Shop block also contains five **nested**
`OVT_ShopTypeInfo.m_sDisplayName` values — a different field on a different class. Anchor on each block's
`m_fVisibilityZoom` to find the right one.)

**Why it was not fixed here.** Pre-existing and orthogonal. `map/map-layers` needed a *plural*, localized
category label and could not reuse a singular one anyway, so it added a **third** field, `m_sCategoryName`,
leaving `m_sDisplayName` untouched and still doing its own job (user decision K2). Fixing F2 means authoring
five new `.st` ids *and* the user regenerating the six Workbench exports — a localization change with its
own gate, which does not belong inside a UI feature.

**Convenient consequence for whoever fixes it.** The English text is already authored and transcribed:
`map/map-layers` added `#OVT-Map_Category_*` ids covering all 14 types. Each of the five needs only a
**singular** sibling id. No translation *content* has to be invented, only the singular forms.

**Suggested severity: Low–Medium** — cosmetic, but a visible untranslated string on a shipped panel, in a
project that otherwise localizes rigorously.

---

## F3 — `OVT_MapPlayerLocation.Update()` sets an opacity it never restores, and never clears `m_Widgets`

**Symptom, two halves.**

1. `Update()` walks `m_Widgets` and, for a player whose controlled entity cannot be resolved, calls
   `SetOpacity(0)` on that player's marker widget — and **nothing ever sets it back**. There is no
   `SetOpacity(1)` anywhere in the class. A player briefly without a controlled entity (respawning) has an
   invisible marker for the **rest of the map session**, even once alive again. A map close/reopen — which
   rebuilds the widget set — is the only recovery.
2. `m_Widgets` is populated in `OnMapOpen` (and *is* cleared there, at the top) but is **not cleared on map
   close**. Between sessions the component holds `Widget` references to widgets the engine has destroyed.

**Where.** `Scripts/Game/UI/Map/Visualization/OVT_MapPlayerLocation.c` — symbols `Update()`, `m_Widgets`,
`OnMapOpen`, `OnMapClose`.

**Why it was not fixed here.** Same "leave the class otherwise as-is" rule as F1. `map/map-layers` did have
to *route around* half of it, and the way it did so constrains any fix: `SetMarkersVisible` uses
**`SetVisible`, never `SetOpacity`**, precisely so the new toggle and this defect cannot interact — and
`Update()` early-returns while markers are hidden, so the two cannot fight over the same widgets. **A fix
for F3 must not "unify" the two by converting the toggle to opacity.**

**Note on half 2.** The stale-reference half is latent rather than active: `OnMapOpen` clears the map before
refilling it, so stale refs are only reachable by something reading `m_Widgets` between a close and the next
open. `Update()` is the only reader and does not run then. That is *why nothing has broken*, not a reason it
is safe — it is one added caller away from being a crash.

**Suggested severity: Medium** for half 1 (an observable, session-long disappearing player marker on a
common code path — dying); **Low** for half 2.

---

## F4 — `OVT_MapPlayerLocation.OnMapOpen` dereferences the config with no null check

**Symptom.** `OnMapOpen` reads `OVT_Global.GetConfig().m_Difficulty.showPlayerOnMap` in a single expression
with **no null guard on `GetConfig()`** and none on `m_Difficulty`. Every other early return in that method
is guarded; this one is not. A null config at map-open time is a null-pointer error in a component that runs
on every fullscreen map open.

**Where.** `Scripts/Game/UI/Map/Visualization/OVT_MapPlayerLocation.c` — symbol `OnMapOpen`, the
`showPlayerOnMap` read.

**Why it was not fixed here.** Found during Phase 3 while adding `m_bAvailableThisSession` immediately below
this very line, and deliberately left alone under the same rule that preserves F1 and F3. It is also **not
obviously reachable**: the map can generally only be opened inside a running campaign, where the config
exists. That is an argument about severity, not about leaving it unguarded — the surrounding code guards
everything else, so the asymmetry is the finding.

**Suggested severity: Low** — a one-line guard, no known reproduction. Worth fixing next time the file is
open rather than scheduling.

---

## F5 — `localization_Overthrow.st` carries 4 duplicate `CustomStringTableItem` GUIDs

**Symptom.** Four GUIDs each appear on **two** `CustomStringTableItem` blocks in the master string table:

    {5D5C558A6E391A20}
    {5D86A310C893DBDE}
    {5D86A310C893DBDF}
    {A1B2C3D4E5F60003}

**🔴 Verified pre-existing at `HEAD`, and explicitly not introduced by this feature.** Run against the
committed file rather than the working tree:

    $ git show HEAD:Language/localization_Overthrow.st \
        | grep -oE 'CustomStringTableItem "\{[0-9A-F]+\}"' | sort | uniq -d
    CustomStringTableItem "{5D5C558A6E391A20}"
    CustomStringTableItem "{5D86A310C893DBDE}"
    CustomStringTableItem "{5D86A310C893DBDF}"
    CustomStringTableItem "{A1B2C3D4E5F60003}"

The working tree — with this feature's **18 new items** added, all from the freshly-allocated `{6A85D1E0…}`
series — produces the **identical four lines and no others**. The count did not move.

**Where.** `Language/localization_Overthrow.st`. ❌ **Do not touch `localization_Overthrow.<lang>.conf`** —
those are Workbench-generated exports, their `Ids{}`/`Texts{}` blocks are neither parallel nor same-length,
and hand-editing has corrupted all six before.

**Why nothing has broken yet.** There are **zero duplicate `Id`s** in the file (verified in the working
tree). Lookups resolve by `Id`, so the duplicated *resource GUIDs* are unreferenced at runtime. The risk is
Workbench-side: a resource-database collision on re-import or export, resolved arbitrarily, silently losing
one member of each pair.

**Why it was not fixed here.** Pre-existing, a Workbench-owned file class, and the fix (reissue four GUIDs)
wants to be made and then verified by a **clean Workbench load** — a user action, not an agent one. Doing it
inside a UI feature would put an unverifiable edit into an unrelated diff.

**Suggested severity: Low** — latent, no observed effect, but exactly the class of thing that becomes
confusing at the worst moment. Cheap to fix during any Workbench session.

---

## F6 — 🔴 `OverthrowCloseInfoPanel` is very likely gamepad-dead: a pre-existing cross-context collision

**Symptom.** `map/core` BUG-134 bound the info panel's close button to a new `OverthrowCloseInfoPanel`
action on `keyboard:KC_C` **and `gamepad0:b`**. Overthrow adds that action to `MapContext` as a same-file
delta (`ActionRefs +{ … }`), and vanilla declares `ActionContext MapContext` at **`Priority 50`**. Vanilla
also declares `ActionContext GadgetMapContext` at **`Priority 55`**, whose `MapEscape` action binds
**`gamepad0:b`** with an `InputFilterDown`. Higher priority wins, so on a controller pressing **B** over the
map very probably fires `MapEscape` (close the map) and **never reaches `OverthrowCloseInfoPanel`** —
leaving the info panel's close affordance **keyboard-only**, on a fix whose whole point was that the panel
could not be dismissed.

**Where.** `Configs/System/chimeraInputCommon.conf` (Overthrow) — the `Action OverthrowCloseInfoPanel`
block's `gamepad0:b` `InputSourceValue`, and the `ActionContext MapContext` `ActionRefs +{ }` delta.
Colliding declaration: `Configs/System/chimeraInputCommon.conf` (vanilla `ArmaReforger`) —
`ActionContext GadgetMapContext`, `Action MapEscape`.

**Why the repo's checker did not catch it.** `.claude/skills/overthrow-ui-patterns/scripts/check-input-conflicts.py`
exits **0** on this tree. The collision is **cross-context** — two contexts, two priorities, two files — and
the checker compares within a context. It also has a known blind spot for the base game's ~197 **inline**
`ActionContext` actions, which is exactly what `MapEscape` is. A clean checker exit is necessary and **not
sufficient**; priority ordering has to be reasoned about by hand.

**Why it was not fixed here.** It is `map/core`'s binding, shipped by BUG-134 and play-tested green **on
mouse and keyboard** on 2026-08-10 — which is why it survived. `map/map-layers` Phase 4b touched this file
only to add `ActionContext OverthrowMapLayersContext` (Priority 70, re-referencing existing `Menu*` actions,
**consuming no new key or pad button**), and widening that into a re-binding of another feature's action was
out of scope.

**How to confirm it, cheaply.** On a pad: open the map, hover a marker to pin the info panel, press **B**.
If the *map* closes rather than the *panel*, the finding is confirmed. Worth doing before designing a fix,
because the analysis above is a code reading — vanilla declares `MapEscape` **twice** with different filters
(`InputFilterDown` in `GadgetMapContext`; `InputFilterClick` with `Visible 0` in `CommandPostMapContext`),
so the resolution is not certain from the config alone.

**Suggested severity: Medium** — a shipped, play-test-approved affordance that is probably unusable on the
platform the epic explicitly cares about, and invisible to the automated checker that exists to catch
precisely this.

---

## F7 — D-pad Left on a focused row unticks it AND leaves the panel (found by the Phase 7 gamepad pass, 2026-08-13)

**Symptom.** With the panel open and a row focused, D-pad Left unticks the row **and** leaves the panel, so
every layer switched off costs a re-entry through the tool strip. Phase 4b predicted the untick half (the
`MenuLeft` shadow over `MapToolMenuFocus`); the panel-exit half was not predicted and is undiagnosed —
either both actions fire despite the priority-70 context, or the toolbox's `MenuLeft` handling moves focus
out of the row list. The same session closed the easy fix: **A on a focused row does nothing**, so
`MenuLeft` is load-bearing for unticking and cannot simply be dropped.

**Suggested fix shape.** A `MenuSelect` listener on `OVT_MapLayerRowComponent` so A toggles the focused row
(through the existing owner callback, never a second state path); then `MenuLeft`/`MenuRight` become
droppable, un-shadowing `MapToolMenuFocus` so D-pad Left returns to the strip cleanly. Diagnose the
panel-exit half first.

**The only F-series finding found by observation rather than code reading.** Filed as **BUG-155**.

---

## Not findings — checked and clean

Recorded so the next reader knows these were looked at rather than skipped.

- **The boundary greps are clean.** No `[RplProp]`, no `[RplRpc]`, no `Rpc(`, no `EPF_` in any new or
  changed file; nothing added to `OVT_PlayerCommsComponent` (untouched at `HEAD`); no `Replication.` or
  `OVT_Global.` call in any new file; and the only mutating calls on any added line across the four changed
  `.c` files are `SetVisible`, `SetPlayerVisible` and `SetMarkersVisible` — three client-side booleans on
  three client-side UI objects. No campaign record is written.
- **No `file:line` pointer was introduced into any shipped code comment** by this feature (epic K-9). The
  four `PlayerComms` hits inside `Language/localization_Overthrow.st` are pre-existing Field Manual
  **translator comments** citing fact-check sources; **zero** of them are on a line this feature added.
- **The canvas-layer contract needed no extension.** See the note under the `OVT_MapCanvasLayer` table in
  `docs/features/map/core/context.md`.
