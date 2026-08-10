# Core / Options — Context & Decisions

**Last Updated:** 2026-08-10
**Status:** 🚧 First slice implemented (admin chat commands), awaiting play-test

---

## Quick Status

**What's Done:**
- ✅ `/give-money <amount>` admin chat command (alias `/givemoney`) — `OVT_AdminCommandsComponent` on
  `OVT_OverthrowController`, server-side admin gate, notifications + localization keys, compile
  clean (2026-08-10)

**What's Next:**
- 📋 Play-test on a server with an admin login (chat commands cannot be exercised headlessly —
  the harness drives no chat UI)
- 📋 Candidates for later slices: BUG-116 follow-ups (warn on arsenal-box deposits; an
  Overthrow-owned persistable arsenal-box prefab for the GM catalog)

**Blockers:**
- Localization runtime exports need regenerating in Workbench before the two new strings render
  (`OVT-Msg-AdminFundsAdded`, `OVT-Msg-AdminCommandRefused`); until then the raw keys show.

---

## Key Files

- `Scripts/Game/Components/Controller/OVT_AdminCommandsComponent.c` — the command component
- `Scripts/Game/GameMode/OVT_OverthrowController.c` — `RpcDo_NotifyOwnerAssignment` calls
  `RegisterChatCommands()` (once, on the owning client)
- `Prefabs/GameMode/OVT_OverthrowController.et` — component wired
- `Configs/overthrowBroadcastMessages.conf` — `AdminFundsAdded`, `AdminCommandRefused`
- `Language/localization_Overthrow.st` — the two message strings

## Important Decisions

- **Chat command, not a menu**: admins are the audience and chat is the vanilla-native admin
  surface (`SCR_ChatPanelManager` command invokers, `/name args`); no layout, keybinding, or
  gamepad work needed.
- **Server-side `SCR_Global.IsAdmin` is the only gate** — registration happens on every client on
  purpose, so the deny path is honest (a notification) instead of the command silently not
  existing.
- **Born from BUG-116**: the money faucet exists so admins seed supplies via gun dealers into real
  containers instead of GM arsenal boxes, whose contents are never saved.
