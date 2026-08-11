# Reforger (upstream) bug reports

Bugs in Arma Reforger itself that Overthrow has hit and (mostly) had to code around, written up for
submission to Bohemia's feedback tracker. One file per report, `RFG-NNN.md`, each structured to be
close to copy-pasteable: Summary, Steps to reproduce, Observed, Expected, script/config citations
against **1.7.0.54** (line numbers are from the extracted script reference tree at
`/mnt/n/Projects/Arma 4/ArmaReforger`), plus the workaround Overthrow ships.

`status:` in each file's frontmatter tracks the report's lifecycle: `draft` → `submitted` (with the
tracker URL in `relatedExternal:`) → whatever BI's resolution is; `cancelled` means we decided not
to file it (the file records why).

**2026-08-12: all eight live reports submitted** to BI's feedback tracker as
`ARMD-10`…`ARMD-17` (see the table). RFG-005 was cancelled before submission: the arsenal box not
saving physical contents is intended base-game behaviour for Conflict-style modes — the real issue
is RFG-006, which blocks modders from overriding it.

## Index

| id | title | severity | status / tracker |
|---|---|---|---|
| [RFG-001](RFG-001.md) | `MovePlayerToGroup` strips the old group before the full-check — failed move leaves player groupless | major | [ARMD-10](https://report.bistudio.com/projects/arma-reforger/reforger-modding/ARMD-10) |
| [RFG-002](RFG-002.md) | `RequestJoinGroup` is a silent no-op server-side — private-group approvals do nothing on dedicated (but notify "accepted") | major | [ARMD-11](https://report.bistudio.com/projects/arma-reforger/reforger-modding/ARMD-11) |
| [RFG-003](RFG-003.md) | `OnItemAdded` NULL VME when inventory manager's owner is not a character | major | [ARMD-12](https://report.bistudio.com/projects/arma-reforger/reforger-modding/ARMD-12) |
| [RFG-004](RFG-004.md) | Scripted `PersistenceConfigRule.IsMatch` is never called — silent never-match | minor | [ARMD-13](https://report.bistudio.com/projects/arma-reforger/reforger-modding/ARMD-13) |
| [RFG-005](RFG-005.md) | Arsenal boxes silently destroy all deposited items on load (no storage serializer, no error) | major | cancelled — intended vanilla behaviour; real issue is RFG-006 |
| [RFG-006](RFG-006.md) | Mod persistence config can never win a prefab match vs vanilla; Priority not honoured | minor | [ARMD-14](https://report.bistudio.com/projects/arma-reforger/reforger-modding/ARMD-14) |
| [RFG-007](RFG-007.md) | `RequestSpawn` on a live record retries forever; batch requests can wedge all saves for the session | major | [ARMD-15](https://report.bistudio.com/projects/arma-reforger/reforger-modding/ARMD-15) |
| [RFG-008](RFG-008.md) | GetIn/RemoveCasualty missing the same-faction guard the door/handbrake actions have | minor | [ARMD-16](https://report.bistudio.com/projects/arma-reforger/reforger-modding/ARMD-16) |
| [RFG-009](RFG-009.md) | `Rpc()` arity/type mismatch compiles clean and fails silently at runtime | minor (QoL) | [ARMD-17](https://report.bistudio.com/projects/arma-reforger/reforger-modding/ARMD-17) |

## Considered and NOT written up (with reasons)

- **The engine never loops an RPC back to its sender** (listen-server host never receives its own
  owner-targeted RPCs — internal BUG-090). By design; vanilla's own code adds local direct-call
  branches. A docs improvement at most.
- **`SCR_PlayerReconnectData.RemoveUnusedCharacters`** deletes unclaimed player bodies ~60 s after
  load. Correct for Conflict's respawn model, wrong for persistent-body modes, but a design
  mismatch rather than a defect. A feature request ("make the sweep opt-out per game mode") could be
  filed if BI is receptive; Overthrow neutralises it in `Scripts/Game/Modded/SCR_PlayerReconnectData.c`.
- **`RandomGenerator.RandInt` max-exclusive + `min == max` engine error** — API design and caller
  error (ours), not a defect.
- **`SCR_ItemAttributeCollection.m_bVisible` has no public setter**, **`SCR_FieldManualUI` has no
  entry-lookup/open-by-id API** — API gaps worked around with modded classes; feature requests, not bugs.
- **`SCR_GroupTileButton` stale Join button** (visibility only re-evaluated on `RefreshPlayers`) —
  minor UI staleness, folded into RFG-002's notes rather than filed alone.
