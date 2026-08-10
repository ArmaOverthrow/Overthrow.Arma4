# Core / Options — Implementation

**Scope:** server-operator options and admin conveniences that are not gameplay systems of their
own. First slice: admin chat commands.

## Admin chat commands

### Why this exists

BUG-116 established that Game-Master-spawned arsenal boxes never save deposited items (vanilla
`Arsenal.conf` has no storage serializer), so admins must stop seeding supplies through GM boxes.
The legitimate replacement flow — buy stock from gun dealers into real storage containers — needs
money. `/givemoney` lets an admin mint it without touching saves or the GM editor.

### Architecture

- **`OVT_AdminCommandsComponent`** (`Scripts/Game/Components/Controller/`) on the per-player
  **`OVT_OverthrowController`** entity (`Prefabs/GameMode/OVT_OverthrowController.et`), following
  the controller-component pattern (`OVT_ShopTransactionComponent` is the model; project rule: no
  new client→server RPCs on the legacy `OVT_PlayerCommsComponent`).
- **Chat registration**: `OVT_OverthrowController.RpcDo_NotifyOwnerAssignment` — the one hook that
  runs exactly once on the owning client — calls `RegisterChatCommands()`, which subscribes to
  `SCR_ChatPanelManager.GetCommandInvoker` for `give-money` (documented form) and `givemoney` (typo alias) (the chat manager is a global game core,
  always present client-side; vanilla command syntax is `/<command> <args>`).
- **Authority**: registration is not a permission. The single gate is server-side in
  `RpcAsk_GiveMoney`: `SCR_Global.IsAdmin(playerId)` against the engine's own role flags
  (`ADMINISTRATOR | SESSION_ADMINISTRATOR`) for the calling connection, with the caller resolved
  from which controller instance ran the RPC (`ResolveOwningPlayerId`, same as the shop component).
  A modified client can send the RPC; it cannot make itself an admin.
- **Effect**: amount clamped to 1..1,000,000 per invocation, then
  `OVT_EconomyManagerComponent.AddPlayerMoney(playerId, amount, true)`. Every grant writes a server
  console audit line.
- **Feedback**: `SendTextNotification` presets `AdminFundsAdded` / `AdminCommandRefused`
  (`Configs/overthrowBroadcastMessages.conf`), strings `OVT-Msg-AdminFundsAdded` /
  `OVT-Msg-AdminCommandRefused` in `Language/localization_Overthrow.st` (runtime exports must be
  regenerated in Workbench before the text renders localized).

### Usage

Admin (server admin login or session admin) types in chat:

```
/give-money 50000
```

Non-admins get "This command requires server admin rights" and the attempt is logged server-side.

### Adding further commands

Register another invoker in `RegisterChatCommands()`, parse in a client-side callback, send one
RPC, gate it server-side with `SCR_Global.IsAdmin`. Keep every command's real check on the server.
