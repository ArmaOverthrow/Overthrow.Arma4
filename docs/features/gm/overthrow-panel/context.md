# Overthrow Panel — Context & Decisions

**Feature:** gm/overthrow-panel (epic `gm`, feature 2 of 5 — first renderer over the gm-state seam)
**Last Updated:** 2026-08-16
**Current Phase:** Complete
**Status:** ✅ Complete — user-verified; absorbed gm-state MP checklist discharged 2026-08-16 (batched epic test session)

---

## Quick Status

**What's Done:**
- ✅ Planning complete (`implementation.md`, citations verified at `462308f5`)
- ✅ Dev docs scaffolded
- ✅ Phase 0: baseline (compile 6091, `{6B08…}` free, BUG-173, seam citations hold at `4eff4685`) + gm-state doc-bug fix
- ✅ Phase 1 (ui-developer): injection (`Modded/SCR_EditModeEditorUIComponent.c`), `GMPanel.layout`+`.meta`
  (34 widgets, chrome matched to GM variant), `OVT_GMPanelUIComponent` skeleton (empty state), 11 `.st` keys —
  compile 6093, all greps clean. Workbench visual round-trip deferred to the Phase 4 user batch

- ✅ Phase 2 (ui-developer): data binding, countdowns, suppression, Logic tests (see Phase 2 findings below)
- ✅ Phase 3 (ui-developer): detail seam — `DetailSection` verified authored empty/`Visible 0`/zero-height,
  four documented methods on `OVT_GMPanelUIComponent`, contract written below. Compile 6095, Q-6 grep clean

- ✅ Phase 4 (user-driven, live): Workbench visual check with three feedback rounds applied (reposition to
  33,250 off the left stack, header band + user-supplied no-pad logo, spill fix); MP checks passed on the
  user's own server. See Verification Record below
- ✅ Phase 5: bookkeeping (this file, gm-state context, epic overview)

**What's Next:**
- 📋 `gm/hud-icons` (consumes the Detail Seam Contract below)
- ⏸️ gm-state's log-based MP checklist items (record counts/build ms, non-admin negative path, JIP) — see
  Verification Record

**Blockers:**
- None

---

## Key Files

### To be created
- `Scripts/Game/UI/Modded/SCR_EditModeEditorUIComponent.c` — the injection point (modded empty base class, ~25 lines)
- `UI/Layouts/GM/GMPanel.layout` + `.meta` — the panel layout, GUID series `{6B08…}`
- `Scripts/Game/UI/GM/OVT_GMPanelUIComponent.c` — widget component (render + lifecycle hygiene)
- `Scripts/Game/UI/GM/OVT_GMPanelFormat.c` — pure statics (countdown/threat formatting), world-free
- `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_GMPanelFormat.c` — Logic-tier cases

### Modified (outside this feature's dirs — exactly two allowed)
- `Language/localization_Overthrow.st` — `#OVT-GMPanel_*` items
- `docs/features/gm/gm-state/context.md` — doc-bug fix (Phase 0) + Phase 4 results

### Related
- `docs/features/gm/overthrow-panel/implementation.md` — the plan; §5 D1–D8 settled, do not re-litigate
- `docs/features/gm/gm-state/context.md` — the Sibling Consumption Contract this panel consumes
- `Scripts/Game/Components/Controller/OVT_GMRequestComponent.c` — the seam
- `Scripts/Game/GameMode/GM/OVT_GMCampaignState.c` — the store

---

## Important Decisions

(Planning decisions D1–D8 live in `implementation.md` §5 — settled: modded-class injection, append-don't-reorder, GM-variant chrome, zero new networking, empty detail seam, split empty state, loc keys from the start, fixed authored rows. Implementation-time decisions get recorded here.)

---

## Baseline (Phase 0) — 2026-08-15 13:50

- `tools/compile-check.sh` → exit 0, **6091 files**, 6s
- Git: HEAD `4eff4685` (plan citations verified at `462308f5`; seam citations re-checked below — all resolve)
- Highest allocated bug id: **BUG-173** (plan snapshot said BUG-167; the 2026-08-15 main merge renumbered — matches gm-state's note)
- GUID series `{6B08…}`: **0 hits** across `Prefabs`, `Configs`, `Scripts`, `UI` — free to mint
- Seam citations resolve at current HEAD: `HasData():90`, `IsDistributionSuppressed():98`,
  `IsPayoutSuppressed():106`, `GetDistributionSecondsRemaining():115`, `GetPayoutSecondsRemaining():124`
  in `OVT_GMCampaignState.c` — identical to the plan's cited lines
- gm-state doc bug (I-4) fixed: `gm-state/context.md` convenience line now names the shipped methods

---

## Gotchas & Learnings

### Phase 1 findings (2026-08-15)
- **GUIDs minted:** layout resource `{6B08D3A17C4B0000}`; widget instances `{6B08D3A17C4B0001}`–`{…0023}`
  (34 widgets, duplicate-GUID script: none); `.st` items `{6B08D3A17C4B1001}`–`{…100B}` (separate sub-block).
  `.meta` present with all five console configurations.
- **11 `.st` keys added** (`OVT-GMPanel_Threat/OFResources/OFDeployment/NextDistribution/NextPayout/Funds/QRF/NoData/QRFNone/SuppressedQRF/SuppressedNoPlayers`) — raw keys render on screen until the user regenerates the localization exports in Workbench (expected, §5 D7).
- **Logo authored 64×64 square** — the plan's `MainMenu.layout:113` cite (`Size 150 300`) is a 1:2 ratio but
  the source `logo_overthrow.edds` is 512×512 square (read from the DDS header); the MainMenu value would
  distort. Workbench round-trip judges whether 64 px reads.
- **The R6 rule sentence must avoid the literal grep tokens** — a Doxygen comment quoting the forbidden
  networking tokens verbatim trips the feature's own Q-6 grep. Worded in prose (same trap as the Logic-tier
  directory grep).
- Deviation: `PANEL_LAYOUT` is a plain instance `const`, not `static const` (initializer-support caution;
  trivially reversible). `ShowCampaignData()` authored in Phase 1 as the paired half of `ShowNoData()`,
  unused until Phase 2's `RenderAll()`.
- Gates: compile exit 0 (6093 files, re-run by orchestrator); networking grep over all three new files: none;
  `git status` clean of base-game/prefab/.conf changes. **Fast group: skipped — pure-UI phase, suites cover
  no UI (`.claude/test-policy.md` §2); Phase 2's run covers the tree.**

### Phase 2 findings (2026-08-15)
- **Bound rows:** funds + QRF render first and unconditionally in `RenderAll()`; the campaign block falls to
  `ShowNoData()` on null seam or `!HasData()`. Payout amount renders with `$` (money, `OVT_ResistanceMenuContext`
  precedent), distribution amount bare (resources).
- **NaN routing:** `FormatCountdown`'s clamp is spelled `if (!(seconds > 0)) return "0:00";` — a non-number
  fails both comparisons, so NaN routes to the clamp without naming NaN. Integer maths after `Math.Floor`, so
  the 3599/3600 hour boundary cannot move via float rounding.
- **`RenderStatus()` returns the previously-drawn suppression state** so `SetColor` fires only on transition;
  suppression uses `SetTextFormat("#OVT-GMPanel_Suppressed*")` in accent `0.761 0.392 0.08 1`, restoring the
  authored colour (cached once at attach) otherwise.
- **Two `.st` keys added beyond Phase 1's 11** — `OVT-GMPanel_QRFActive` (`%1 pts - %2`) and
  `OVT-GMPanel_QRFActiveTown` (`%1 pts - %2 - %3`), GUIDs `{6B08D3A17C4B100C}`/`{…100D}`: composing the QRF
  line in script would build English in code. Export regeneration already owed for Phase 1 covers these.
- **`ResolveQRFTownName()` bounds-checks before `GetTownName()`** — that manager method indexes `m_Towns`
  unguarded.
- **Prove-can-fail (static — orchestrator ran the real suite):** inverted `3599→"1:00:00"` and `3.0→"3"`,
  compiled, verified the named `SetFailure` path (label + input + actual + expected), reverted (grep-verified).
- **Gate verdict: Fast group 138/139 → green after diagnosis.** The one red,
  `OVT_TEST_Init_Tutorial_SettingsStoreRoundTrips` (pre-existing, added 2026-08-09, arrived via the
  2026-08-15 main merge), hit its 60 s wall-clock timeout because the client stalled 3 m 44 s on a Steamworks
  `OnGetTicketForWebApiResponse`; the case's own success print landed the same tick as the timeout verdict.
  Targeted re-run of the bare case: **OK (1 test, 25 s)**. Environmental, unrelated to this feature; noted
  as a latent flake risk in that case's design (wall-clock timeout spans engine stalls).

### Phase 4 Workbench findings — live user round-trip (2026-08-15)
- **Confirmed by screenshot:** panel rendered in EDIT mode, D2's append-lands-above held, strings localized
  after export regen, empty state + live Funds/QRF rows correct.
- **D1/D2 SUPERSEDED BY USER FEEDBACK — panel moved off the left stack entirely.** The base game's
  contextual help renders over the bottom-left stack and overlapped the panel. The panel root now carries
  its own `FrameWidgetSlot` (`Anchor 0 0 0 0`, `PositionX 24`, `PositionY 100`, `SizeToContent 1`) and is
  created directly under the Mode_Edit root (a `FrameWidgetClass`, `Mode_Edit.layout:1`). The modded class
  no longer references `Mode_Edit_Element_Left` — that base-game name coupling is gone, as is the D2
  child-ordering question (now moot).
- **Overlay measure ignores `AlignableSlot` vertical padding** — the panel background is an overlay child
  stretched to the overlay's measured size; the inner content's `AlignableSlot Padding 36 16 36 16` was
  applied at arrange but its vertical component was NOT measured, so the last row spilled ~32 px past the
  background (user-confirmed). Vanilla never hits this because `ModeMenu.layout` uses a fixed
  `HeightOverride 204`. **Fix pattern: put spacing on child `LayoutSlot` paddings (measured), keep only
  horizontal padding on the alignable slot.**
- **Slot align enums are 0=left/top, 1=center, 2=right/bottom, 3=stretch** — empirically: `HorizontalAlign 2`
  right-aligned the logo (user-observed). Do not guess 2=center.
- **Header band added (user request):** `GMPanel_Header` overlay (GUIDs `{6B08D3A17C4B0024}`–`{…0026}`),
  background `#1E2C2F` (`0.118 0.173 0.184 1`), new user-supplied asset
  `{6932D2D4F869C81D}UI/Textures/logo_overthrow_nopad.edds` (512×164 banner) centered at 140×45.

---

## Detail Seam Contract (for hud-icons)

The Overthrow panel offers `hud-icons` an **empty, named container** at the bottom of its widget tree
and four methods to drive it. That is the entire contract: the panel owns the slot's *existence* and
*lifecycle*, `hud-icons` owns everything about its *content*. **The panel makes no assumption whatsoever
about what the content widget contains** — no data shape, no layout, no widget type, no naming
convention — so nothing needs to be agreed with this feature before writing the fill code.

### The four calls

All four are on `OVT_GMPanelUIComponent`
(`Scripts/Game/UI/GM/OVT_GMPanelUIComponent.c`, the root component of `UI/Layouts/GM/GMPanel.layout`).

```enforce
static OVT_GMPanelUIComponent GetInstance();   // may be null
Widget GetDetailSlot();                        // may be null; never creates anything
void   ShowDetail(bool show);                  // visibility only
void   ClearDetail();                          // removes all children, then hides
```

| Call | Contract |
|---|---|
| `GetInstance()` | The attached panel component, **or null**. Null-check every time. |
| `GetDetailSlot()` | The `DetailSection` container widget to parent content into. **Never creates anything** — it is a plain accessor over a widget cached at attach. May return null while the panel is being torn down, or if the layout stops carrying a widget named `DetailSection`. |
| `ShowDetail(bool)` | Sets the section's visibility and nothing else. It neither adds nor removes content, so hide → show preserves whatever was parented in. A no-op when the slot is missing. |
| `ClearDetail()` | Removes **every** child of the slot, then hides the section — leaving it exactly as authored. It does not notify anyone; a caller holding references to the widgets it parented in must drop them itself. Safe on a missing or already-empty slot. |

### The null-may-happen rule

`GetInstance()` and `GetDetailSlot()` **may return null, and normally do.**

The panel exists **only while the Game Master editor is in EDIT mode**. `GetInstance()` is set in
`HandlerAttached` and nulled in `HandlerDeattached`, so outside EDIT mode there is no panel and no slot.
Since EDIT mode is exactly the state in which `hud-icons` will be asking, this is the ordinary path, not
an error path — a null result means "no panel right now", and the correct response is to do nothing and
try again next time the code runs.

**Never cache the instance or the slot widget across frames.** Switching EDIT → PHOTO → EDIT destroys
this component and its whole widget tree and constructs new ones, so a stored reference goes stale with
no signal. Re-fetch on every use; `GetInstance()` is one static read and `GetDetailSlot()` is one field
read.

```enforce
OVT_GMPanelUIComponent panel = OVT_GMPanelUIComponent.GetInstance();
if (!panel)
    return;                       // no editor panel right now - normal, not an error

Widget slot = panel.GetDetailSlot();
if (!slot)
    return;

panel.ClearDetail();              // replace whatever was there
GetGame().GetWorkspace().CreateWidgets(MY_DETAIL_LAYOUT, slot);
panel.ShowDetail(true);
```

### Lifecycle — the engine builds and destroys the panel, not you

The engine creates the panel's widget tree when EDIT mode activates and destroys it when EDIT mode
deactivates (`SCR_MenuLayoutEditorComponent.EOnEditorPostActivate` :87 /
`EOnEditorPostDeactivate` :109 — `RemoveFromHierarchy()` on the mode's hideable layout). Consequences
that matter to a caller:

- **Content parented into the slot dies with the panel.** It is destroyed along with the rest of the
  tree; no explicit cleanup is required, and none is possible after the fact.
- **A rebuilt panel comes back empty and collapsed.** `DetailSection` is authored `Visible 0` with no
  children, so anything shown before a mode switch is gone after it. `hud-icons` must re-fill on demand,
  never assume its content survived.
- **`hud-icons` does no lifecycle plumbing of its own.** There is nothing to subscribe to, nothing to
  register, and no "is the panel open?" query beyond the null check above.

### Authored state of the slot

`DetailSection` (`UI/Layouts/GM/GMPanel.layout`, widget GUID `{6B08D3A17C4B0023}`) is a
`VerticalLayoutWidgetClass`, the **last child of `GMPanel_Inner`**, with **no children**, `"Is Visible" 0`,
and no size override or slot padding — so hidden it occupies **zero vertical space** and the panel is
exactly its own height until a caller shows it. Content stacks vertically inside it; a caller wanting a
different arrangement parents its own container widget in.

---

## Verification Record (Phase 4, 2026-08-15)

- ✅ **Workbench visual round-trip (live, user-driven):** panel renders in EDIT mode, localized strings
  resolve after export regen, empty state + live Funds/QRF rows correct, Photo mode absent, 5× open/close
  clean, position confirmed at exactly 33,250 (user measured). Three rounds of visual feedback were applied
  during the check (reposition off the left stack, header band + new logo, background-spill fix — see
  Phase 4 Workbench findings above).
- ✅ **MP verification: user-verified on their own server (all panel checks pass).** The user ran the panel
  on their own server and confirmed the Step 2 checklist (rows populate, countdowns tick, funds live).
  **Auth-path detail was not captured** — which of admin login / `-ovtGmDev` / GM role gated the panel
  remains unrecorded; note for hud-icons planning.
- ⏸️ **Local dedicated-server experiment failed twice at backend auth** (`ServerImpl: authentication
  timeout` → `connection refused`, reason=3) — the client's ConnectionRequests were seen but backend auth
  never completed, consistent with the Steamworks stall observed in the same session's test run.
  `--mode local` (which skips real auth) was not retried because the user verified on their own server
  instead. Worth knowing for future local MP tests of GM features: **dedicated-mode local join can wedge on
  Steam backend auth.**
- ✅ **Absorbed gm-state Phase 5 log-based items — discharged 2026-08-16:** user confirmed the whole
  checklist (negative path, JIP, stale-discard, record counts) was exercised in the epic's batched MP/JIP
  test session alongside hud-icons; all tests green. Individual log measurements were not recorded.

## Triage: "the panel shows nothing"

1. Mode is EDIT? (Photo/Building/Screenshot never build the panel — structural.)
2. `OVT_ControllerComponent<OVT_GMRequestComponent>.Get()` null? (dedicated server, pre-ownership, or
   missing prefab block — `OVT_TEST_Init_GMRequestSeam` is the gate.)
3. `state.HasData()` false? (Unauthorized player or first poll in flight — both show "Waiting for campaign
   data..." by design, D6; Funds/QRF rows stay live either way. Since 2026-08-15 Workbench Play mode is
   always authorized — `#ifdef WORKBENCH` clause in `IsAuthorizedGM`, user decision: the panel is a debug
   tool. A permanent "Waiting" in Workbench after that date means the poll never started — check the
   editor-open hook, not the gate.)
4. Beyond that → gm-state's own triage (`gm-state/context.md`, "Triage: a GM sees nothing").

---

## Session Notes

### 2026-08-15 13:45
- Feature started via /autorun-feature (Discord). Docs scaffolded from the completed plan.
- Next: Phase 0 baseline, then Phase 1 via ui-developer.

### 2026-08-15 (post-completion amendments, same session)
- **User requirement: the panel must work in Workbench Play mode (it's a debug tool).** Added a
  `#ifdef WORKBENCH` authorize-all clause to `OVT_GMRequestComponent.IsAuthorizedGM` — `WORKBENCH` is
  defined only by addon.gproj's workbench script configuration, never in shipping builds, so no MP leak.
- **Real gm-state bug found and fixed: polling could never start.** `Event_OnEditorManagerInitOwner`
  fires once at player connect; when the editor manager initializes before the controller component's
  `OnPostInit` subscribes (observed: always, in practice), the open/close invokers were never wired and
  the store stayed empty forever — panel renders, data never flows, on every topology. Fix in
  `OnPostInit`: after subscribing, catch up via `SCR_EditorManagerEntity.GetInstance()` (wire the
  already-existing local manager; if `IsOpened()`, start polling immediately). **User-confirmed: campaign
  block now populates live in Workbench Play mode — the first end-to-end proof of gm-state's data path.**
- Final panel position per user: **22, 90** (was 33, 250). User also tuned the header colour in Workbench
  (`0.0681 0.0986 0.1046 1`).
- Gates after amendments: compile 6095 clean; **Fast group 139/139 green (53 s)**.
- The earlier "MP all pass on own server" claim is downgraded: it verified panel rendering only — the
  stuck-"Waiting" report that followed exposed the lifecycle bug. gm-state's dedicated-server/JIP/negative
  log-based checks remain owed; the WB host path is now genuinely proven.

---

*Update this file at the end of each work session.*
