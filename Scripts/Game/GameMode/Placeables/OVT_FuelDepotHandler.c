//------------------------------------------------------------------------------------------------
//! Server-side placement gate for the Fuel Depot: it may only be built at a base the occupying
//! faction does not hold.
//!
//! WHY THIS CLASS EXISTS. `m_bBuildAtBase` and its four sibling location booleans are read by
//! NOTHING on the server. OVT_ResistanceFactionManager.BuildItem() validates funds, distance from
//! the builder and the per-location item limit, and then spawns whatever prefab index it was handed
//! - the "can this be built HERE" question is answered only by the client, in
//! OVT_BuildContext.CanBuild (`:294-303`), which is a UX gate and nothing more. A crafted
//! RpcAsk_BuildItem can therefore put any buildable anywhere (implementation.md R8). This handler
//! closes that hole FOR THIS BUILDABLE ONLY; the general gap belongs to resistance/building and this
//! feature deliberately does not widen it.
//!
//! WHY A REJECTION IS FREE. BuildItem() runs the handler BEFORE it charges
//! (OVT_ResistanceFactionManager.c:846-856): a false return deletes the spawned entity and returns
//! null, and TakePlayerMoney is never reached. A player who is refused here loses nothing.
//!
//! WHY playerId == -1 IS ALLOWED THROUGH. -1 is BuildItem()'s own "server-initiated, free" marker -
//! the same value that skips its funds, distance and item-limit checks. Server-side and test builds
//! use it, including the persistence case that builds a depot to assert its fuel level, so refusing
//! it here would break the only automated coverage this prefab has.
//!
//! THE RULE MIRRORS THE CLIENT'S, ON PURPOSE. Same nearest-base lookup, same `baseRange` radius,
//! same non-occupying-faction test as OVT_BuildContext.CanBuild, so the authority can never refuse
//! something the build menu offered (or the reverse). If that client check ever changes, change this
//! one in the same commit.
//!
//! NOT A USE-TIME GATE. Once built, the depot keeps dispensing free fuel to whoever reaches it, even
//! if the base changes hands (implementation.md D4). That fuel was hauled and paid for by the
//! resistance; losing it with the base is a consequence, not a bug.
//------------------------------------------------------------------------------------------------
class OVT_FuelDepotHandler : OVT_PlaceableHandler
{
	//------------------------------------------------------------------------------------------------
	//! Accepts or refuses a freshly spawned Fuel Depot. Refusing deletes it before anyone is charged.
	//! \param[in] entity The depot that BuildItem() just spawned.
	//! \param[in] playerId Runtime id of the builder, or -1 for a server-initiated build.
	//! \return True to keep the depot, false to delete it and abort the build.
	override bool OnPlace(IEntity entity, int playerId)
	{
		if (!entity) return false;

		// Server/test build - BuildItem() has already waived funds, distance and item limits for it.
		if (playerId == -1) return true;

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying) return false;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config || !config.m_Difficulty) return false;

		OVT_BaseData base = occupying.GetNearestBase(entity.GetOrigin());
		if (!base) return false;

		// The occupying faction still holds it - this is not a resistance base yet.
		if (base.IsOccupyingFaction()) return false;

		float dist = vector.Distance(base.location, entity.GetOrigin());
		if (dist >= config.m_Difficulty.baseRange) return false;

		return true;
	}
}
