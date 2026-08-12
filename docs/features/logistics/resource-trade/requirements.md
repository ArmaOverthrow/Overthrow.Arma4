# Resource Trade — Requirements

**Epic:** logistics
**Created:** 2026-08-11

## Overview

`resource-trade` connects the resource ledger to the money economy at the single point where the two convert: the port. It adds an "Import Resources" flow — separate from the existing item Import screen — that fills the truck the player is sitting in, plus an export flow that sells resources back for cash. It is the MVP's only source of resources, which is why it lands before construction.

## Requirements

- A **new port flow, separate from the existing Import screen** (`OVT_PortContext`). The existing item-import UI keeps working unchanged; resources get their own entry point and screen. Follow the established `OVT_UIContext` lifecycle and `.layout` conventions, and keep it gamepad-navigable.
- **Requires a truck.** The flow is only available to a player seated in a vehicle with a resource cargo store, and buying deposits directly into that store. If the player is not in a suitable vehicle, the reason is shown rather than the option silently missing.
- **Capacity-aware buying.** A purchase that would exceed the truck's remaining m³ is rejected with a clear message, or clamped to what fits — decide during planning and be consistent with the transport feature's load rejection.
- **Import gating on the `importable` flag.** Only resources marked importable appear in the buy list. Resources that exist but cannot be imported must not be listed at all (following the `OVT_PortContext` precedent that a row whose only outcome is a no-op click is a bug, per BUG-102).
- **Export sells anything.** Any resource in the truck's store can be sold at the port, **including non-importable ones**. Sell price derives from the resource's **live (drifted) price** via a configurable ratio, not from the config base.
- **Illegal resources are control-gated.** A resource flagged `illegal` may only be imported or exported when the resistance controls the town the port belongs to. The gate is enforced **server-side**, not just hidden in the UI.
  - Note: the agreed MVP resource set contains no illegal member, so this path ships correct but unexercised until a config flag is flipped. Do not remove it as dead code, and prove it works by temporarily flagging a resource during testing.
- **Pricing is config-driven** from the resource definitions; buy and sell prices must be visible in the UI before the player commits — and prices **move**, so the UI must read the live price, never the config base.
- **Resource prices drift slowly over time.** The config price is a base; the live price wanders around it. Requirements:
  - **Tick on the existing economy clock.** `OVT_EconomyManagerComponent` already runs `CheckUpdate` with hour-gated events (income at 0/6/12/18, stock resupply at 07:00, rent at 00:00), each guarded by an `m_iHourPaid*` field so it cannot double-fire within an hour. A drift step must use that same guard idiom — the tick fires repeatedly within an hour and an unguarded drift would compound many times per game hour. Pick the cadence during planning (once per in-game day, or per 6-hour block alongside income) and state it.
  - **Server-only.** The server drifts and the new prices replicate; a client must never compute its own drift or the port would quote a different price than the server charges.
  - **Bounded.** The walk is clamped to a band around the base price (e.g. 0.5×–2.0×) so a long campaign cannot drift a resource to free or to unaffordable. Band edges are config, not magic numbers.
  - **Biased by the war, not just noise.** The step is a random walk **plus a pressure term read from game state** — resistance control of the port's town and/or occupying-faction threat push prices up or down, so a blockaded or contested port is felt in the wallet and the price becomes a readout of how the campaign is going. Choose the exact inputs during planning and document the mapping; keep it to state that is already cheap to read on the server.
  - **Sell price follows.** The sell price is derived from the live buy price via the sell ratio, so it drifts with it — there is no second independent walk.
- **Difficulty drives price two ways** — both `OVT_DifficultySettings` fields in the `Economy` category, applied through `OVT_OverthrowConfigComponent` accessors, exactly as `buildableCostMultiplier` / `vehiclePriceMultiplier` already are:
  1. **A level multiplier** applied to the drifted price (e.g. `resourcePriceMultiplier`) — harder difficulties make resources dearer across the board. It multiplies the *drifted* value, so it scales the whole market rather than the base only. State the composition order explicitly and assert it.
  2. **A volatility multiplier** (e.g. `resourcePriceVolatility`) scaling the size of each drift step — a low value gives an almost-flat market, a high value gives a market that swings. Volatility scales the step, **not** the clamp band; the band stays fixed so volatility changes how fast prices move, not how far they can go.
  - Difficulty `.conf` files override only what differs from the `[Attribute(defvalue:)]` — `Difficulty_Normal.conf` and `Difficulty_TestWorld.conf` list no multipliers at all today. Add lines only to tiers that want non-default values.
- **Speculation is allowed by design.** Buying low and selling high across a drift cycle is an intended mid-game money loop — it costs the player cash up front, truck volume, and the round trip. Do **not** constrain the sell ratio to make it impossible. Do make sure the price shown is the price charged, so the loop rewards timing rather than exploiting a UI/server mismatch.
- **The player must be able to see the market moving.** A drift nobody can perceive is indistinguishable from random pricing. The port UI should show each resource's price relative to its base — at minimum a direction/degree indicator (up, down, near base) — so a player can judge whether now is a good time to buy or sell. Presentation is a planning decision; the requirement is that drift is legible.
- Purchases and sales go through `OVT_EconomyManagerComponent` for the money side so player balances, and any existing money-changed events, behave exactly as they do for item purchases.
- Server-authoritative: the client requests a buy/sell through a component on `OVT_OverthrowController` (**never** `OVT_PlayerCommsComponent` — it is legacy/deprecated); the server re-validates capacity, flags, town control, money and stock before mutating anything.
- New strings go in `Language/localization_Overthrow.st`; layouts referencing not-yet-exported keys use literal text until the user regenerates.
- Automated coverage: Logic-tier assertions for the drift step (clamping at both band edges, volatility scaling the step size, the difficulty multiplier composing in the stated order, and the war-pressure term moving the price in the expected direction), for the guard preventing more than one drift per cadence window, and for price computation and the importable/illegal predicates; Campaign-tier assertion for a port purchase moving money and resources in the started-campaign world, if the test world supports it.

## Dependencies

- **`logistics/resource-core`** — resource definitions, flags, prices and the ledger.
- **`logistics/resource-transport`** — a truck cargo store to buy into and sell out of.
- `economy` epic — `OVT_EconomyManagerComponent` for money, for `CheckUpdate`'s hour-gated tick that the drift step hangs off, and its `m_iHourPaid*` guard idiom; `OVT_PortContext` and the port location for the entry point.
- `towns` epic — town control state for the illegal gate **and** for the war-pressure term in the price drift; the port must resolve to its owning town. Occupying-faction threat state if that is chosen as a pressure input.
- `core/controller-migration` — the `OVT_OverthrowController` component pattern for the client→server request path.
- Can be **implemented in parallel** with `logistics/resource-construction` (disjoint code), but should land first so construction is play-testable.

## Out of Scope

- Producing resources anywhere other than by import — no factories, salvage, or passive town production.
- Buying resources directly into a warehouse or a crate pile. The truck is the only destination.
- Changing the existing item Import screen's behaviour or its importables list.
- **Player supply-and-demand pricing** — a player buying or selling does not itself move the price. Drift comes from the clock and from war state only.
- **Per-port price variation.** All ports quote the same live price for a given resource; there is no arbitrage between locations, only across time.
- **Price drift for non-resource items.** The existing item and vehicle prices stay static; this drift engine applies to resources only.
- Smuggling gameplay beyond the binary town-control gate (no patrols, searches, or contraband detection).
- Trading resources between players, or any resistance-funds path for resources.
