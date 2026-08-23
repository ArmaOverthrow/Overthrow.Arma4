
---

## Post-close change 2026-08-23 (d) — looting is a crime if you are seen

User call after close. See `context.md` "Post-close change 2026-08-23 (d)".

- [x] PF1 `OVT_PlayerWantedComponent.GetIllegalActionReason()` — so a caller can only close its OWN illegal-action window
- [x] PF2 `ArmLootIllegalWindow` / `ClearLootIllegalWindow` / `ResolvePlayerWanted` on `OVT_StorageRequestComponent`; armed at `StartLootJob`, **re-armed at every LOOT chunk**, closed at FINISH and ABORT
- [x] PF3 `#OVT-Msg-WantedLooting` `{6A8E2F1000000002}` (GUID verified repo-unique; braces 2439 → 2441)
- [x] PF4 Gate: `compile-check.sh` exit 0 (6341 files)
- [ ] PF5 🔴 `.st` re-export owed — the new key renders raw until then (joins the `OVT-Transfer_NoSpace` debt)
- [ ] PF6 Play-test: loot with a patrol watching → wanted 2 + "You were seen looting the dead!"; loot unobserved → nothing; walk away after a run → no lingering window
