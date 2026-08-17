# High Command — Requirements

**Epic:** resistance
**Created:** 2026-08-17 (formalized 2026-08-18 after design discussion)

## Overview

High Command is the resistance-side counterpart to the occupying faction's new deployment-driven
counter-attacks (`occupying/counter-attacks`): player-purchased, persistent, map-commanded AI groups.
It gives the resistance a way to assign forces to defense as well as offense — garrison a predicted
objective, patrol a supply route, hold a captured base — and to redirect those forces at will. It
builds on the `virtualization` epic (observer API) and generalizes the recruit system's proven
machinery (record-based ownership, JIP replication + status heartbeat, record-driven map markers,
body persistence).

## Architecture: always-live groups + observers

- HC groups are **always-live `SCR_AIGroup`s, never virtualization-registered**. Rationale
  (record it — it will be questioned): there is no virtual-vs-virtual combat resolution, so a
  dormant HC group could never actually fight the OF's virtual patrols. Always-live means every
  engagement is a real engagement, wherever it happens and whoever is online.
- Each HC group gets a **virtualization AI observer** (`AddEntityObserver`, the exact pattern
  `OVT_InactiveRecruitGroupComponent` ships for parked recruit squads, including the deferred
  install and cleanup discipline), so OF registered groups materialise around them and fights
  happen with no player nearby. Honor the same style of operator gate the recruit-observer knob
  uses (`m_bRecruitGroupsAreObservers` precedent).
- Consequence, stated as intended: **travel is real-time**. A foot group walks its orders live at
  AI pace; vehicle groups are the fast option. The map shows progress.

## What is retired (legacy garrisons)

High Command replaces the garrison systems — "garrisoning" a position is now just parking an HC
group there in Defense/Patrol stance, which keeps it redirectable at will.

- **Base garrisons**: the `OVT_BaseMenuContext` purchase UI, `AddGarrison` RPC chain and server
  path, spawn/waypoint helpers, and the serializer garrison fields on base records.
- **FOB garrisons**: `OVT_FOBMenuContext` purchase, `AddGarrisonFOB` chain, `SpawnGarrisonFOB`,
  serializer fields.
- **Camp garrisons**: the machinery (`AddGarrisonCamp` chain) — its UI path is already dead
  (nothing ever wires a camp into the FOB menu context), so this is pure code removal.
- The map info panels' `garrisonCount` rows (broken on clients anyway — documented as reading 0)
  are removed/replaced by HC group presence.
- **Existing saves: persisted garrisons are dropped on load** — changelog warning, no migration.
- The garrisons' town-supporter draw-down and the hardcoded `300/unit` equipment fee (with its
  `//To-do: factoring in warehouse`) are superseded by HC purchase pricing below, which is that
  TODO's fulfillment.

## Buildables

- **Barracks** — a new buildable; additionally, base-game barracks buildings that already exist at
  bases become functional barracks via Overthrow's thin prefab-override mechanism (inherit the
  vanilla building, add the component + interaction furniture). The friendly-base gate
  (`!IsOccupyingFaction()`, `OVT_ManageBaseAction` precedent) keeps them inert until the base is
  captured.
- **Warehouse** — a new buildable, placeable at captured bases and in towns (captured or not). It
  **joins the existing warehouse system** (registers the equivalent of `OVT_RealEstateManager`'s
  `OVT_WarehouseData`) so inventory, UI (`OVT_WarehouseContext`) and item logic stay unified — not
  a parallel storage system.

## Buying groups

- Inside a barracks, at a curated interaction point (desk prop), an action opens the purchase UI —
  **any player**, not just officers. Wiring copies the shop pattern: child prop with
  `ActionsManagerComponent`, action resolves the parent component (`OVT_ShopAction` /
  `OVT_BuyEquippedRecruitAction.ResolveTentRoot` precedents), gated on the base being friendly.
- The purchasable set is a list of group prefabs in the FIA faction config, **including vehicle
  groups**.
- **Pricing** (reusing `OVT_RecruitLoadoutPricing` + `OVT_RecruitPurchaseRules`): per member, the
  base recruit cost; per required item, either **consumed from in-range warehouse stock** (free) or
  charged buy price × the existing difficulty-scaled `recruitLoadoutFeeMultiplier` — same math as
  equipped recruits. Each group prefab's required-item list is built **once at game load** by
  inspecting its character prefabs (groups are static).
- **Supporters**: HC purchases draw down town supporters like garrisons did — a deliberate
  additional limiting factor.

## Converting recruit groups

- The recruit menu's inactive roster is displayed **split by group** (the merge grouping already
  exists; this surfaces it) and gains a **convert to High Command** action per group — enabling
  custom group compositions.
- Conversion is **one-way**. Converted recruits become **anonymous HC members**: they leave the
  recruit roster and cap, and their recruit progression (name/XP/skills tracking) ends. Their
  bodies — and therefore their equipment — carry over via the existing body-persistence machinery.

## Command & control (map)

- HC groups render as **green NATO symbols** (vanilla `MilitarySymbol` imageset precedent) on
  **every player's map** — own groups at full opacity, others dimmed (the 0.45 inactive-recruit
  precedent). Selecting any group — owned or not — shows its stance, destination (with travel
  indication), and status, so players can coordinate.
- A red **"!" tag** marks groups with enemy contact; an ammo badge marks groups completely out of
  ammo.
- **Orders**: select a group on the map, move the cursor, press the order button. One destination
  at a time; the group's **stance** defines what it does on arrival.
- **Stances** (initial set, mapped to proven waypoint kit): **Defend** (hunker at the destination —
  defend waypoint), **Patrol** (cycling perimeter around the destination), **Attack** (search &
  destroy at the destination). Extensible later.
- **Gamepad is mandatory from day one**: the map has no usable action context for ordering — the
  known recipe is a dedicated per-frame `ActionContext` with its own bindings (map-panels
  finding). "Ships controller-dead" is a known failure mode; spec against it.
- **Replication is record-based**, recruit-manager style: JIP `RplSave` of the HC table, delta
  RPCs, and a ~10 s status heartbeat carrying position + flag bits (contact, ammo). Clients never
  read live entities for this.
- New client→server seams (orders, purchase, conversion, transfer) follow the **per-player
  controller-component pattern** (`OVT_UprisingRequestComponent` / `OVT_FOBRequestComponent`
  precedents), with full server-side validation.

## Ownership, caps, lifetime

- Groups are owned per-player (persistent id, recruit precedent). **The cap counts members, not
  groups** — server-configurable, suggested default **48 members per player** (play-test tunable;
  this is the server's AI-budget knob, since HC groups are always spawned).
- Groups **persist indefinitely**: they are not despawned on owner disconnect (deliberate contrast
  with the recruits' 600 s offline despawn) — they continue to their destination and hold their
  stance.
- **Transfer**: any player can transfer a group they own to another player. **Officers can take
  ownership of an offline player's groups** (intended for abandoned groups; officers are trusted
  players) and can transfer a group back when that player returns.
- A **"Manage Groups" main-menu item** (recruit-roster precedent) shows status and allows
  dismissal.

## Logistics

- **Rearm**: groups in range of a warehouse holding the needed ammo automatically top up on a
  periodic tick — `OVT_VehicleRearmUtils` shape (client-answerable eligibility, server-only
  mutation, warehouse stock consumed). Group UI shows ammo status.
- **Refuel**: vehicle groups in range of a fuel source automatically refuel, **charging the owning
  player with the same math as the player-facing fuel purchase**. Depends on the prerequisite
  feature that wires base-game fuel stations to charge money (see Dependencies); any such fuel
  source counts.

## QRF zone control — required integration

Today `OVT_QRFControllerComponent.CheckUpdatePoints` counts **real players only** on the friendly
side — resistance AI can never hold a battle zone. That is a shipped defect (recruits should
count) being fixed **on the main branch** as a bug. This feature requires that **HC members count
toward zone control** the same way once that fix lands — otherwise stationing groups at the
counter-attacks objective is pointless the moment the QRF fires.

## Interplay with `occupying/counter-attacks`

No code coupling is needed for most of it — always-live groups make it emergent: HC patrols
intercept Phase 1 harassment before it reaches the town center, HC groups help clear enemy FOBs
(the area-clear + held action), and HC presence at an OF base's approaches is combat pressure.
Two explicit touch points:

- **OF FOB starvation**: counter-attacks defines starvation as the source base "cleared of
  garrison and/or strong resistance presence" — HC group presence is the intended measure of
  "resistance presence" (cross-referenced in that feature's requirements).
- **QRF zone control** (above).

## Difficulty / config knobs

Member cap per player (server config), group prices (via existing `baseRecruitCost` +
`recruitLoadoutFeeMultiplier`), supporter draw-down per member, rearm/refuel tick interval and
ranges, observer gate (operator off-switch, recruit-observer precedent).

## Dependencies

- `virtualization` epic (complete): the observer API (frozen `api.md` §3) and its gotchas
  (deferred install, explicit removal, expense).
- Recruit system machinery (`resistance/recruits` + `recruit-ux`): record/replication patterns,
  body persistence + reservation, inactive-group component (conversion seam), roster UI patterns.
- **Prerequisite feature: `economy/fuel`** (fuel stations charge money; fuel depot buildable) —
  must land before HC refuel; HC consumes it as-is (stations charge the owner, fueled depots are
  free).
- **Main-branch bug fix: resistance AI counts in QRF zone control** — HC extends it to HC members.
- `occupying/counter-attacks` (parallel): no build dependency, but the two features' requirements
  cross-reference each other and should be play-tested together.

## Out of Scope

- **Fuel depot buildable** — owned by `economy/fuel` (which lands first); HC only consumes it as
  another fuel source.
- **Virtual-vs-virtual combat resolution** (dormant HC groups fighting dormant OF groups).
- Converting HC groups back to recruits.
- Additional stances beyond the initial Defend/Patrol/Attack set.
- Resistance-side intel on enemy positions (Intel epic).
- Any change to how recruits themselves work beyond the roster split + convert action.

## Testing expectations

- Logic-tier coverage for pricing (warehouse sourcing vs charged items), cap enforcement, and
  transfer/ownership rules.
- Persistence-tier round trip for HC records (group, members' bodies, stance, destination,
  ownership).
- Map ordering UI, gamepad path, rearm/refuel, and long-distance travel (foot + vehicle, incl.
  owner-offline continuation) are play-test gated.
