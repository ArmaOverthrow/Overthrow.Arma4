//------------------------------------------------------------------------------------------------
//! "Fill Tank" - a five-second hold that fills every tank the target has, at once, for a price shown
//! before you commit.
//!
//! WHY IT EXISTS. Play-test found the vanilla trickle refuel unusable at scale: a fuel truck's
//! 5,000 L cargo tank at its 4,500 L/min inbound cap is over a minute of holding one action, and the
//! providing station's own outbound rate usually makes it several times that. This action is the
//! answer, and it is ADDITIVE - the vanilla Refuel action is still on every vehicle, still trickles,
//! still charges per litre. Nothing here replaces it.
//!
//! WHERE IT LIVES - three contexts, mirroring the three vanilla refuel actions it complements:
//!   - "fuel_cap" on the vehicle base prefab, beside vanilla's BaseRefuel;
//!   - "supportStation_fuel" on the fuel-tank part prefab, beside vanilla's Refuel_CargoTank - which
//!     is what lets fuel be moved depot -> truck storage -> another depot;
//!   - "default" on the Fuel Depot, which has no fuel_cap point of its own.
//!
//! ONE PRESS FILLS ONE TANK: THE ONE YOU ARE STANDING AT (amendment A2). GetOwner() is the chassis
//! for the fuel_cap instance, the cargo-tank PART for the supportStation_fuel instance and the depot
//! for the depot's, and this action only ever looks at that entity's OWN fuel manager - the same
//! scoping SCR_RefuelAtSupportStationAction.Init applies. The first build asked for every manager in
//! the vehicle's hierarchy instead, so one press at a truck's fuel cap pulled ~3,960 L (chassis +
//! cargo) and emptied a freshly filled depot to 21%.
//!
//! WHAT GOES ON THE WIRE IS NOT THE OWNER. A cargo-tank part is a real entity but has no
//! RplComponent, so the request names the OWNER'S ROOT (the truck, which is networked) plus the
//! tank's authored m_iFuelTankID. See OVT_FuelRequestComponent's header for the resolution and for
//! what a lying client can and cannot do with that id.
//!
//! NOTHING THIS CLASS DECIDES IS AUTHORITY. The server re-derives the target, the distance, the
//! source, the litres, the price and the balance in OVT_FuelRequestComponent.RpcAsk_FillFuel. The
//! gates below exist so the action does not offer a fill the server would refuse, and the label
//! exists so a player knows the real price - including a player who cannot afford a full tank and
//! will be sold as much fuel as their money buys.
//!
//! GATE POLICY (OVT_SabotageTowerAction's rule): HIDDEN only when the action is irrelevant - no fuel
//! manager on this entity, or no fuel source in range at all, which is most vehicles most of the
//! time and is why this must not clutter every car on the map. VISIBLE WITH A REASON for every state
//! a player might otherwise think is a bug: tanks already full, the source is dry, no money at a paid
//! source.
//!
//! Local-effect-only: the client wrapper resolves the LOCAL player's controller entity
//! (OVT_ControllerComponent<OVT_FuelRequestComponent>.Get), so PerformAction must run on the machine
//! that clicked. A server-executed PerformAction would find no local controller on a dedicated server.
//------------------------------------------------------------------------------------------------
class OVT_FillFuelAction : ScriptedUserAction
{
	//! How long the computed answers are reused, in milliseconds of world time. Both gates AND the
	//! label run every frame while a player looks at the vehicle, and the honest answer costs a walk
	//! of the whole registered fuel-station registry plus a node walk on two fuel managers. Same
	//! caching shape and the same one second as OVT_RearmVehicleAction and OVT_SellVehicleCargoAction.
	protected const float CHECK_TTL_MS = 1000;

	//! World time (ms) at which the cached answers go stale, and whether there are any.
	protected float m_fCacheExpiresAt;
	protected bool m_bHasCache;

	//! A fuel source covers this target, with or without fuel in it. Drives visibility ONLY - a dry
	//! source still shows the action, with a reason.
	protected bool m_bCachedSourceInRange;

	//! The best source found had fuel to give. False with m_bCachedSourceInRange true means "dry".
	protected bool m_bCachedSourceHasFuel;

	//! The target cannot receive any more fuel.
	protected bool m_bCachedTargetFull;

	//! The best source charges for its fuel (a static pump or tank, not a truck / jerrycan / depot).
	protected bool m_bCachedPaid;

	//! A source was positively resolved AND it dispenses for nothing. Deliberately not just
	//! "!m_bCachedPaid": with no source at all, nothing is known, and drawing "(Free)" over an action
	//! that has no fuel behind it would be a promise the world cannot keep.
	protected bool m_bCachedFree;

	//! Whole dollars this fill would actually cost the LOCAL player right now - the clamped figure
	//! from the plan, not the full price of a full tank, so a broke player sees what they will really
	//! pay rather than a number they cannot reach.
	protected int m_iCachedCost;

	//! There is a real, non-zero price to draw. False for a free source, a full tank, a dry pump and a
	//! player with nothing to spend - all of which get the bare name plus a reason instead of "($0)".
	protected bool m_bCachedShowPrice;

	//! The plan would move at least one drop. False at a paid source means the player has no money.
	protected bool m_bCachedPlanHasFuel;

	//! The networked entity the request will name - the action owner's root parent, because a cargo
	//! tank part is a real entity with no RplComponent of its own.
	protected IEntity m_CachedRoot;

	//! Tank id naming THIS action's own fuel manager under that root. -1 when it could not be named,
	//! which hides the action rather than risking a fill of the wrong tank.
	protected int m_iCachedTankId;

	//------------------------------------------------------------------------------------------------
	//! Ask the server to fill THIS action's own tank from the best fuel source covering its vehicle.
	//!
	//! Sends the cached root and tank id rather than pOwnerEntity, because the owner of a cargo-tank
	//! action is not addressable across the network - see the class header.
	//! \param[in] pOwnerEntity The entity this action lives on - the tank being filled.
	//! \param[in] pUserEntity The performing character.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		RefreshCache();

		if(!m_CachedRoot || m_iCachedTankId < 0) return;

		OVT_FuelRequestComponent fuel = OVT_ControllerComponent<OVT_FuelRequestComponent>.Get();
		if(!fuel) return;

		fuel.FillFuel(m_CachedRoot, m_iCachedTankId);

		// Every cached answer is now certainly stale (the tank is about to fill and the money to go).
		m_bHasCache = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Shown only where a fill could ever happen: this entity holds fuel, and something in range could
	//! give it some. Deliberately NOT hidden when the tanks are full, the source is dry or the player
	//! is broke - those are told why in CanBePerformedScript.
	//! \param[in] user The character looking at the action.
	//! \return True when the action should be listed.
	override bool CanBeShownScript(IEntity user)
	{
		// PHASE-0 GATE (core/damage D15): a ruin offers nothing. IsUsable() answers true for every
		// owner that is not a retrofitted structure, so this costs the other contexts nothing.
		if(!OVT_StructureDamage.IsUsable(GetOwner())) return false;

		RefreshCache();
		return m_bCachedSourceInRange;
	}

	//------------------------------------------------------------------------------------------------
	//! Blocked, with a reason, unless there is room to fill, fuel to fill it with, and the money to
	//! buy at least some of it. A courtesy, never a security boundary - the server re-derives every
	//! one of these.
	//! \param[in] user The character looking at the action.
	//! \return True when the request is worth sending.
	override bool CanBePerformedScript(IEntity user)
	{
		RefreshCache();

		if(m_bCachedTargetFull)
		{
			SetCannotPerformReason("#OVT-FillFuel_TankFull");
			return false;
		}

		if(!m_bCachedSourceHasFuel)
		{
			SetCannotPerformReason("#OVT-FillFuel_SourceEmpty");
			return false;
		}

		// Only a PAID source can be blocked by money, and only at a balance of zero - anything above
		// that buys a partial fill, which is the whole point of the clamped plan.
		if(m_bCachedPaid && !m_bCachedPlanHasFuel)
		{
			SetCannotPerformReason("#OVT-Refuel_CannotAfford");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The label carries the REAL price of this press, the way OVT_RearmVehicleAction carries the
	//! re-arm price: the clamped figure from the plan, so a player with $12 at a $1/L pump reads
	//! "($12)" rather than the $60 a full tank would have cost.
	//!
	//! THREE STATES, and the third is why this is not a one-liner. A free source gets its own key -
	//! "(Free)" is worth saying when the same action on the same vehicle costs money fifty metres
	//! away. A paid source with something to sell gets the price. EVERYTHING ELSE - a full tank, a dry
	//! pump, an empty wallet - gets the bare name, because "($0)" reads as free and would contradict
	//! the reason string sitting right underneath it.
	//! \param[out] outName The label to draw.
	//! \return Always true; this action always names itself.
	override bool GetActionNameScript(out string outName)
	{
		RefreshCache();

		if(m_bCachedShowPrice)
		{
			outName = "#OVT-FillFuel ($" + m_iCachedCost.ToString() + ")";
			return true;
		}

		if(m_bCachedFree)
		{
			outName = "#OVT-FillFuel_Free";
			return true;
		}

		outName = "#OVT-FillFuel";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Runs on the clicking machine only - see the class header.
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Recompute every cached answer when they have gone stale.
	//!
	//! Ordered so the expensive work is only paid for when the cheap work says it could matter: an
	//! entity with no fuel manager never touches the station registry, and a target that is already
	//! full never prices anything. The one case that walks the registry twice - a source in range but
	//! dry - is the rarest, and paying for it is what lets the action say "the pump is empty" instead
	//! of vanishing.
	//!
	//! Every lookup is null-guarded: this runs in the world editor and in scenarios with no Overthrow
	//! game mode, where none of the managers exist.
	protected void RefreshCache()
	{
		float now = GetWorldTimeMs();
		if(m_bHasCache && now < m_fCacheExpiresAt) return;

		m_fCacheExpiresAt = now + CHECK_TTL_MS;
		m_bHasCache = true;

		m_bCachedSourceInRange = false;
		m_bCachedSourceHasFuel = false;
		m_bCachedTargetFull = false;
		m_bCachedPaid = false;
		m_bCachedFree = false;
		m_bCachedShowPrice = false;
		m_bCachedPlanHasFuel = false;
		m_iCachedCost = 0;
		m_CachedRoot = null;
		m_iCachedTankId = -1;

		IEntity owner = GetOwner();
		if(!owner) return;

		// THIS action's own tank and no other (amendment A2). GetOwner() is the chassis for the
		// "fuel_cap" instance, the cargo-tank part for the "supportStation_fuel" instance, and the
		// depot for the depot's - which is exactly how vanilla scopes its own refuel action.
		SCR_FuelManagerComponent ownTank = OVT_FuelUtils.GetOwnFuelManager(owner);
		if(!ownTank) return;

		// The root is what goes on the wire; the tank id is what tells the server which manager under
		// it we meant. If the id does not resolve back to OUR manager - an authoring collision nothing
		// in the game currently has - stay hidden rather than risk filling the wrong tank.
		IEntity root = owner.GetRootParent();
		if(!root) return;

		int tankId = OVT_FuelUtils.GetPrimaryFuelTankId(ownTank);
		if(tankId < 0) return;

		if(OVT_FuelUtils.FindFuelManagerByTankId(root, tankId) != ownTank) return;

		m_CachedRoot = root;
		m_iCachedTankId = tankId;

		float needed = OVT_FuelUtils.GetRefuelableCapacity(ownTank);
		m_bCachedTargetFull = needed <= 0;

		// Measured from the ROOT, not from this tank: both contexts on one truck must agree about
		// which pump is being used, and the root is also what the server range-checks.
		SCR_FuelSupportStationComponent source = OVT_FuelUtils.FindBestFillSource(root);
		if(!source)
		{
			// Nothing with fuel in it. Ask the cheaper question - is there a source here AT ALL? - so a
			// dry pump shows the action with a reason instead of silently disappearing.
			m_bCachedSourceInRange = OVT_FuelUtils.HasFillSourceInRange(root);
			return;
		}

		m_bCachedSourceInRange = true;
		m_bCachedSourceHasFuel = true;

		// Read the price BEFORE the full-tank exit, so a full tank at the depot still says "(Free)"
		// and a full tank at a pump does not.
		float price = OVT_FuelUtils.GetFuelCostPerLitre(source);
		m_bCachedPaid = price > 0;
		m_bCachedFree = !m_bCachedPaid;

		if(m_bCachedTargetFull) return;

		// A source with no fuel manager pours without ever being drained (the vanilla backup-flow
		// case), so it is bounded only by what the target can take.
		float available = needed;
		SCR_FuelManagerComponent sourceFuel = OVT_FuelUtils.GetStationFuelManager(source);
		if(sourceFuel)
			available = OVT_FuelUtils.GetProvidableFuel(sourceFuel);

		// The LOCAL player's own money, which is the only balance a client is guaranteed to have
		// streamed - and the same number the server will read for this player.
		int balance = 0;
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(economy)
			balance = economy.GetLocalPlayerMoney();

		OVT_FuelFillPlan plan = OVT_FuelPricing.ComputeFillPlan(needed, available, balance, price);

		m_iCachedCost = plan.m_iCost;
		m_bCachedPlanHasFuel = plan.HasFuel();
		m_bCachedShowPrice = m_bCachedPaid && m_bCachedPlanHasFuel;
	}

	//------------------------------------------------------------------------------------------------
	//! World time in milliseconds, guarded so the action still answers in a world-less context.
	//! \return The current world time, or 0.
	protected float GetWorldTimeMs()
	{
		BaseWorld world = GetGame().GetWorld();
		if(!world) return 0;

		return world.GetWorldTime();
	}
}
