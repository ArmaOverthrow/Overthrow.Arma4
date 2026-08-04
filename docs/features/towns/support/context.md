# Towns Support - Context & Decisions

**Last Updated:** 2026-08-03
**Current Phase:** Retrospective Documentation
**Status:** ✅ Documented (Existing Feature)

---

## Quick Status

**What's Done:**
- ✅ Feature fully implemented (existing code)
- ✅ Retrospective documentation created (`/discover-feature`, 2026-08-02)
- ✅ Top findings filed as BUG-063, BUG-064, BUG-065, BUG-066 (BUG-058's shared hour-gate also hits GrowingDiscontent)

**What's Next:**
- 📋 Review for potential improvements (see implementation.md Future Enhancements)

**2026-08-04 additions (non-violent support levers, from play-through design discussion):**
- **Pirate radio placeable** — `PirateRadio` support modifier (+30, 3600 s, non-stackable, appended LAST in `supportModifiers.conf` — index identity preserved) via a new illegal placeable (placeholder prefab `RadioStation_ANPRC77_01`, swap pending user pick). Re-placing refreshes the timer.
- **Radio tower sabotage** — hold action (20 s) on the three `TransmitterTower_01_*_base.et` prefabs → `OVT_TowerSabotageComponent` (on OVT_OverthrowController) → `OVT_RadioTowerData.disabledRemaining` (difficulty `radioTowerSabotageTime`, 900–3600 s). Disabled towers count as no tower: the town tick now clears BOTH permanent tower modifiers when no tower is on the air in range. Disabled state replicates via RPC + JIP stream but is NOT persisted (a loaded save re-enables towers).
- **Illegal actions while seen** (posters/sandbags/hedgehogs/pirate radio placement, sabotage) = wanted 2 via `OVT_PlayerWantedComponent.OnIllegalActionSeen`.
- Patrol removal of deployments is NOT built — filed as BUG-084.

**Blockers:**
- None

---

## Key Files

- `Scripts/Game/GameMode/Systems/Modifiers/OVT_TownSupportModifierSystem.c` — the ±1 random-walk `Recalculate` override
- `Scripts/Game/GameMode/Systems/Modifiers/Support/*.c` — the 5 handlers
- `Scripts/Game/UserActions/OVT_ConvertSupporterAction.c` + `OVT_PlayerCommsComponent.c:83-92` — conversion path
- `Scripts/Game/GameMode/Placeables/OVT_PlaceableSupportModHandler.c` + `Configs/Resistance/placeables.conf` — posters
- `Configs/Modifiers/supportModifiers.conf` — 13 entries; index = id
- `OVT_TownManagerComponent.c` — direct mutators, 70 s cadence, transport (towns/core)

## Important Decisions

- **Support is a headcount and a random walk** — modifier sums are *rates* (±1 per 70 s max), with equilibrium damping (gain chance ×0.25 once support% overtakes the sum, no damping on decay). Do not convert it to a derived value; persistence restoring it **raw** (vs stability's recompute) depends on this and is documented at `OVT_TownManagerSerializer.c:50-71`.
- **`RpcAsk_AddSupportModifier` deliberately does not recalculate** and the tick discards the support `OnTick` return — support only moves on the 70 s cadence. Asymmetries vs stability are mostly intentional; see the seam table in implementation.md before "fixing" any of them.
- **Diplomacy assigns (`player.diplomacy = chance`)** — idempotent under JIP/load skill replay without a reset call.
- **Poster refusal contract**: `OnPlace` returning false → entity deleted before charging (commit `79f5290`).

## Gotchas & Learnings

- The div-by-zero guard (`max == 0` → return + ERROR) fires every 70 s for any population-0 town — expected console noise, recorded in the Logic suite.
- `MarkAsPerformed` on convert is client-local — it does not survive despawn/respawn and does not sync between players.
- Village flips live inside `RecalculateSupport` (support code owns a control-flow rule); QRF town selection excludes villages — the peaceful/battle partition is deliberate.
- A lost QRF calls `ResetSupport` outright — the anti-oscillation mechanism; don't remove it when touching QRF outcomes.
- `stackLimit`/`flags` omissions in the conf take defaults — RevolutionaryMomentum's ×5 stacking (BUG-065) is an omission, posters' ×5 is intended.
- The RNG (`s_AIRandomGenerator`) has no injection seam — the equilibrium branch is untestable by design; the Logic suite honestly documents this.
- No victory condition exists anywhere in the game mode — grep confirmed; `ResistanceVictory` is just a modifier name.

---

*This context file was created retrospectively by analyzing existing code.*
