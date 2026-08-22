[ComponentEditorProps(category: "Overthrow/Components/Controller", description: "Server-authoritative fuel transfers for one player")]
class OVT_FuelRequestComponentClass : OVT_ControllerRequestComponentClass {};

//------------------------------------------------------------------------------------------------
//! Server-authoritative fuel transfers, on the per-player OVT_OverthrowController entity.
//!
//! ONE REQUEST: the fast "Fill" action (amendment A1). Play-test found the vanilla trickle refuel
//! unusable at scale - a fuel truck's 5,000 L cargo tank at 700 L/min is over seven minutes of
//! holding one action - so a player may instead hold a five-second action and have the whole thing
//! move at once, paying up front for exactly what arrives. THE VANILLA REFUEL PATH IS UNTOUCHED: it
//! still exists on every vehicle, still trickles, still charges per litre through the modded
//! SCR_FuelSupportStationComponent, and nothing in this file runs during it.
//!
//! ================== WHY THIS IS ITS OWN CONTROLLER COMPONENT ==================
//!
//! The nearest precedent, OVT_ShopTransactionComponent.RpcAsk_RearmVehicle, is a money-for-goods
//! request with no shop entity that was deliberately NOT given a component of its own - and its own
//! comment says why: re-arm is a stop-gap the logistics epic intends to delete, so a whole component
//! for one RPC pair was not worth it. Neither half of that reasoning holds here. The fast fill is a
//! shipped, permanent part of economy/fuel; a fill from a fuel truck or the Fuel Depot moves NO MONEY
//! AT ALL, so calling it a shop transaction would be wrong in the common case; and the fuel domain
//! already has a named future consumer (resistance/high-command's auto-refuel tick) that will want a
//! server seam of exactly this shape. The architecture's own granularity agrees - OVT_TowerSabotageComponent
//! is a whole controller component for a single action.
//!
//! ================== WHAT THE CLIENT IS ALLOWED TO SAY ==================
//!
//! An RplId naming a networked entity, and an int naming which of that entity's tanks to fill.
//! Nothing else - not the source, not the litres, not the price, not who is asking. RplId and never
//! EntityID, because an EntityID from a client names a different entity (or nothing) on the server.
//! The caller's identity comes from the CONTROLLER ENTITY the request arrived on
//! (ResolveOwningPlayerId), so it cannot be spoofed by a payload that does not exist.
//!
//! WHY THE TANK IS AN INT AND NOT A SECOND RplId (amendment A2). A fuel truck's cargo tank is a real
//! entity slotted into the truck, but Vehicle_Fuel_Tank_Base.et carries no RplComponent - it is not
//! addressable across the network at all. So the request names the truck (which is addressable) plus
//! the tank's m_iFuelTankID, which is authored prefab data, identical on both machines, distinct
//! across a vehicle's hierarchy (Ural-4320: chassis 1 and 2, cargo 10) and exists for precisely this
//! purpose - vanilla's own SCR_RefuelAtSupportStationAction scopes itself with a list of the same ids.
//! OVT_FuelUtils.FindFuelManagerByTankId turns it back into a manager, on the server, from scratch.
//!
//! WHAT A LYING CLIENT CAN AND CANNOT DO WITH THAT INT. The search is bounded to the ROOT THE SERVER
//! RESOLVED, so an id can only ever pick a tank on the entity the caller is already standing at,
//! already within TARGET_MAX_DISTANCE of, and already has a valid fuel source covering. An
//! unrecognised id is a flat rejection, never a fall-back. The worst a spoofed id achieves is filling
//! the cargo tank while standing at the chassis fuel cap instead of walking three metres to the cargo
//! nozzle - i.e. a tank the SAME player could fill anyway, from another context on the SAME vehicle,
//! at the same range, out of the same source, paid at the same price from their own wallet. The
//! scoping is a gameplay decision (a fill should not silently drain a depot into 4,500 L of storage),
//! not a security boundary; the security boundaries - whose money, which entity, how far, which source
//! - are all re-derived below and are untouched by it.
//!
//! The server then re-derives, in order and from scratch: the requesting player, their character,
//! the root entity, the distance between them, WHICH fuel manager the tank id names, how much it can
//! take, the best source covering the root that is not part of it, how much that source can give, the
//! price at that source, the player's balance, and finally the plan. Every one of those is also
//! computed on the client to draw the action - and every one of them is thrown away here.
//!
//! ================== ORDERING, AND THE ONE RULE THAT IS NOT STYLE ==================
//!
//! The fuel moves BEFORE the money is taken, and the amount taken was already clamped to the balance
//! that was read a few lines earlier (OVT_FuelPricing.ComputeFillPlan). Both halves matter:
//!  - taking first and transferring second would charge for a transfer that could still fail;
//!  - taking an unclamped amount would overdraw SILENTLY, because TakePlayerMoneyPersistentId floors
//!    the account at zero and reports success either way. The balance is therefore read before the
//!    plan is built and re-checked before the take, exactly as the trickle charge path does.
//!
//! DEGRADATION: no economy manager, no player manager, no resolvable player, no target, no fuel
//! manager, no source in range, nothing to move - every one of them returns silently and does
//! nothing. The only refusal worth telling a player about is the one they can act on (no money at a
//! paid source), and that gets the existing CannotAfford toast.
//------------------------------------------------------------------------------------------------
class OVT_FuelRequestComponent : OVT_ControllerRequestComponent
{
	//! How far the requesting player may be from the entity they are filling. The same 15 m the
	//! vehicle requests and the trunk sell already use: the user action requires the player to be
	//! standing at the fuel cap, so this only has to cover a long vehicle - or the 9 m Fuel Depot -
	//! plus latency and movement slack.
	protected const float TARGET_MAX_DISTANCE = 15;

	//! Notification tag for the one refusal a player can do something about. Preset lives in
	//! Configs/overthrowBroadcastMessages.conf and is already used by every other purchase path.
	protected const string NOTIFICATION_CANNOT_AFFORD = "CannotAfford";

	//------------------------------------------------------------------------------------------------
	//! Ask the server to fill ONE tank from the best fuel source covering its vehicle.
	//!
	//! BRANCHES ON Replication.IsServer(), AND MUST: an RplRcver.Server RPC marshalled BY the authority
	//! is delivered to nobody, so an unconditional Rpc() here would make the action silently do nothing
	//! for a listen-server host or a single player (BUG-164 and the whole P2-P8 family).
	//! \param[in] root The networked entity the tank belongs to - a vehicle, or the Fuel Depot. For a
	//! truck's cargo tank this is still the TRUCK, because the cargo-tank part has no RplComponent.
	//! \param[in] fuelTankId Tank id naming which manager under that root to fill (amendment A2 - see
	//! the class header for why this cannot be an RplId and what it can and cannot be used for).
	void FillFuel(IEntity root, int fuelTankId)
	{
		if(!root || fuelTankId < 0) return;

		RplComponent rootRpl = GetEntityRpl(root);
		if(!rootRpl) return;

		if(Replication.IsServer())
		{
			RpcAsk_FillFuel(rootRpl.Id(), fuelTankId);
		}else{
			Rpc(RpcAsk_FillFuel, rootRpl.Id(), fuelTankId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: move as much fuel as ONE named tank can hold, the source can give and the player can
	//! pay for, then charge for exactly that.
	//!
	//! The client named a root entity and a tank id, and nothing else. Validation order: server ->
	//! requesting player -> character -> root entity -> player near it -> the tank id resolves to a
	//! manager UNDER THAT ROOT -> it has room -> a source covers the root that is not part of it and
	//! has fuel -> price -> balance -> plan. The manager resolution, the source rule and the capacity
	//! arithmetic are the same OVT_FuelUtils code the user action's gates run, so a request the action
	//! offered is one this handler accepts.
	//! \param[in] rootId RplId of the networked entity the tank belongs to.
	//! \param[in] fuelTankId Which manager under that root to fill.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_FillFuel(RplId rootId, int fuelTankId)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!character) return;

		IEntity target = ResolveEntity(rootId);
		if(!target) return;

		if(vector.Distance(character.GetOrigin(), target.GetOrigin()) > TARGET_MAX_DISTANCE) return;

		// ONE tank, named by id and resolved from scratch. The search is bounded to this root, so the
		// id can only ever pick a manager on the entity we just range-checked (see the class header's
		// abuse note). A null answer is a rejection, never a fall-back to "fill everything".
		SCR_FuelManagerComponent targetTank = OVT_FuelUtils.FindFuelManagerByTankId(target, fuelTankId);
		if(!targetTank) return;

		float needed = OVT_FuelUtils.GetRefuelableCapacity(targetTank);
		if(needed <= 0) return;

		SCR_FuelSupportStationComponent source = OVT_FuelUtils.FindBestFillSource(target);
		if(!source) return;

		// A source with no fuel manager is the vanilla backup-flow case: it pours without ever being
		// drained, so it is bounded only by what the target can take.
		SCR_FuelManagerComponent sourceFuel = OVT_FuelUtils.GetStationFuelManager(source);
		float available = needed;
		if(sourceFuel)
			available = OVT_FuelUtils.GetProvidableFuel(sourceFuel);

		if(available <= 0) return;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(!economy) return;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return;

		string persId = players.GetPersistentIDFromPlayerID(playerId);
		if(persId == "") return;

		float price = OVT_FuelUtils.GetFuelCostPerLitre(source);
		int balance = economy.GetPlayerMoney(persId);

		OVT_FuelFillPlan plan = OVT_FuelPricing.ComputeFillPlan(needed, available, balance, price);
		if(!plan.HasFuel())
		{
			// The only way a plan comes back empty at this point is an empty wallet at a paid source -
			// every other limit was checked above. That is the one refusal worth telling them about.
			if(price > 0)
				SendCannotAffordNotification(playerId);

			return;
		}

		// Belt and braces: ComputeFillPlan already guarantees cost <= balance, and the balance cannot
		// have moved since it was read a few lines ago. Asserting it anyway is the project's standing
		// rule, because the take that follows clamps at zero and would hide an overdraw as a sale.
		if(plan.m_iCost > 0 && !economy.PlayerHasMoney(persId, plan.m_iCost)) return;

		float delivered = AddFuelToManager(targetTank, plan.m_fLitres);
		if(delivered <= 0) return;

		if(sourceFuel)
			DrainFuelFromManager(sourceFuel, delivered);

		// Charged for what ARRIVED, never for what was planned. The two agree in every case the node
		// arithmetic can produce, but a target whose nodes moved between the capacity read and the
		// transfer must not be billed for the difference.
		int cost = CostForDelivered(plan, delivered);
		if(cost > 0)
			economy.TakePlayerMoneyPersistentId(persId, cost);
	}

	//------------------------------------------------------------------------------------------------
	//! Pour litres into every node of ONE fuel manager that can receive them. SERVER ONLY.
	//!
	//! Fills node by node, each to its own capacity, until the litres run out - so a Ural's chassis
	//! manager fills its 300 L main tank and its 60 L reserve together, because those are two nodes of
	//! the one tank the player is standing at. It never reaches beyond this manager: the cargo tank is
	//! a different manager on a different entity and is filled by its own action (amendment A2).
	//!
	//! No flow-capacity limit is applied on purpose: the flow caps are what make the vanilla trickle a
	//! trickle, and this action exists precisely to skip them. Every node is written with the same
	//! SCR_FuelNode.SetFuel the vanilla server-side transfer uses, so the level replicates to clients
	//! exactly as it always has.
	//! \param[in] manager The fuel manager being filled.
	//! \param[in] litres How much to add in total.
	//! \return Litres actually placed in this manager's nodes.
	protected float AddFuelToManager(SCR_FuelManagerComponent manager, float litres)
	{
		if(!manager || litres <= 0) return 0;

		array<SCR_FuelNode> nodes = {};
		manager.GetScriptedFuelNodesList(nodes, SCR_EFuelNodeTypeFlag.CAN_RECEIVE_FUEL);

		float remaining = litres;
		float delivered = 0;

		foreach(SCR_FuelNode node : nodes)
		{
			if(!node) continue;
			if(remaining <= 0) break;

			float room = node.GetMaxFuel() - node.GetFuel();
			if(room <= 0) continue;

			float add = remaining;
			if(add > room)
				add = room;

			node.SetFuel(node.GetFuel() + add);

			remaining = remaining - add;
			delivered = delivered + add;
		}

		return delivered;
	}

	//------------------------------------------------------------------------------------------------
	//! Take litres back out of a source's providing nodes. SERVER ONLY.
	//!
	//! The conservation half of the transfer, and it is not optional: vanilla drains the provider node
	//! by exactly what it delivered (SCR_FuelSupportStationComponent.OnFuelAddedToVehicleServer), so a
	//! fast fill that skipped this would let one fuel truck refuel a whole army. Drains node by node
	//! in the order the manager lists them, clamped so a node can never go below zero.
	//! \param[in] manager The source's fuel manager.
	//! \param[in] litres How much to remove in total.
	protected void DrainFuelFromManager(SCR_FuelManagerComponent manager, float litres)
	{
		if(!manager || litres <= 0) return;

		array<SCR_FuelNode> nodes = {};
		manager.GetScriptedFuelNodesList(nodes, SCR_EFuelNodeTypeFlag.CAN_PROVIDE_FUEL);

		float remaining = litres;

		foreach(SCR_FuelNode node : nodes)
		{
			if(!node) continue;
			if(remaining <= 0) break;

			float fuel = node.GetFuel();
			if(fuel <= 0) continue;

			float take = remaining;
			if(take > fuel)
				take = fuel;

			node.SetFuel(fuel - take);

			remaining = remaining - take;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! What to charge for the litres that actually landed, given the plan that was priced.
	//!
	//! Scales the planned cost by the shortfall, if there was one, and never rounds it UP past the
	//! plan - the player was shown a price and must never pay more than it. A full delivery pays the
	//! planned price exactly, so the common case is untouched by the arithmetic.
	//! \param[in] plan The plan that was priced against the balance.
	//! \param[in] delivered Litres the transfer actually placed.
	//! \return Whole dollars to take. Never more than the plan's cost, never negative.
	protected int CostForDelivered(OVT_FuelFillPlan plan, float delivered)
	{
		if(!plan || plan.m_iCost <= 0) return 0;
		if(delivered <= 0) return 0;

		if(delivered >= plan.m_fLitres) return plan.m_iCost;

		float share = delivered / plan.m_fLitres;
		int cost = Math.Floor(plan.m_iCost * share);

		if(cost < 0) return 0;
		if(cost > plan.m_iCost) return plan.m_iCost;

		return cost;
	}

	//------------------------------------------------------------------------------------------------
	//! Tells the player their wallet is empty at a paid source. The one refusal in this file that is
	//! actionable, and the same toast every other purchase path uses.
	//! \param[in] playerId The player to notify.
	protected void SendCannotAffordNotification(int playerId)
	{
		OVT_NotificationManagerComponent notify = OVT_Global.GetNotify();
		if(!notify) return;

		notify.SendTextNotification(NOTIFICATION_CANNOT_AFFORD, playerId);
	}
}
