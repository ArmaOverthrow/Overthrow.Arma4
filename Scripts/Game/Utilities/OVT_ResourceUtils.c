//------------------------------------------------------------------------------------------------
//! Holder addressing for resources: every resource holder is an RplId and nothing else.
//!
//! One vocabulary, deliberately - truck, pile and warehouse are all an OVT_ResourceStoreComponent
//! behind one RplId. There is no warehouse index, no per-manager id and no EntityID on the wire: an
//! EntityID names a different entity (or nothing) on the other machine, and an RplId is the only
//! reference that means the same thing on both.
//------------------------------------------------------------------------------------------------
class OVT_ResourceUtils
{
	//------------------------------------------------------------------------------------------------
	//! A networked holder reference back to this machine's copy of the entity.
	//! \param[in] id The holder id carried by the route.
	//! \return The entity, or null when the id resolves to nothing here (always a rejection, never a retry).
	static IEntity ResolveHolder(RplId id)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
		if (!rpl)
			return null;

		return rpl.GetEntity();
	}

	//------------------------------------------------------------------------------------------------
	//! An entity's networked name.
	//! \param[in] e The holder to name.
	//! \return Its RplId, or RplId.Invalid() when it is not replicated and therefore cannot be named.
	static RplId GetHolderId(IEntity e)
	{
		if (!e)
			return RplId.Invalid();

		RplComponent rpl = RplComponent.Cast(e.FindComponent(RplComponent));
		if (!rpl)
			return RplId.Invalid();

		return rpl.Id();
	}

	//------------------------------------------------------------------------------------------------
	//! The resource store on an entity, if it has one.
	//! \param[in] e The candidate holder.
	//! \return Its OVT_ResourceStoreComponent, or null.
	static OVT_ResourceStoreComponent GetStore(IEntity e)
	{
		return OVT_ComponentFinder<OVT_ResourceStoreComponent>.Find(e);
	}

	//------------------------------------------------------------------------------------------------
	//! The store behind a networked holder reference, in one step.
	//! \param[in] id The holder id carried by the route.
	//! \return The store, or null when the id resolves to nothing or to a non-holder.
	static OVT_ResourceStoreComponent ResolveStore(RplId id)
	{
		return GetStore(ResolveHolder(id));
	}

	//------------------------------------------------------------------------------------------------
	//! The pile marker on an entity, if it has one.
	//! \param[in] e The candidate pile.
	//! \return Its OVT_ResourcePileComponent, or null.
	static OVT_ResourcePileComponent GetPile(IEntity e)
	{
		return OVT_ComponentFinder<OVT_ResourcePileComponent>.Find(e);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] e The candidate.
	//! \return True when the entity is a dropped resource crate pile.
	static bool IsPile(IEntity e)
	{
		return GetPile(e) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! A resource's translated display name. DISPLAY LAYER ONLY - it resolves a localization key, so
	//! nothing the Logic tier or the server's own decisions touch may call it.
	//! \param[in] id A stable resource id.
	//! \return The translated catalogue title, falling back to the id so a label is never blank.
	static string ResolveResourceTitle(string id)
	{
		OVT_ResourceManagerComponent resources = OVT_Global.GetResources();
		if (!resources)
			return id;

		OVT_ResourcesConfig config = resources.GetResourcesConfig();
		if (!config || !config.m_aResources)
			return id;

		foreach (OVT_Resource res : config.m_aResources)
		{
			if (!res || res.m_sId != id)
				continue;

			if (res.m_sTitle == "")
				return id;

			string translated = WidgetManager.Translate(res.m_sTitle);
			if (translated == "")
				return id;

			return translated;
		}

		return id;
	}

	//------------------------------------------------------------------------------------------------
	//! Translated display names for a requirement list, index-aligned with it.
	//! \param[in] amounts The requirement list.
	//! \param[out] titles Receives one name per entry. Cleared first.
	static void ResolveResourceTitles(notnull array<ref OVT_ResourceAmount> amounts, notnull array<string> titles)
	{
		titles.Clear();

		foreach (OVT_ResourceAmount amount : amounts)
		{
			if (!amount)
			{
				titles.Insert("");
				continue;
			}

			titles.Insert(ResolveResourceTitle(amount.m_sId));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Integer litres as the m3 figure every resource readout shows. DISPLAY ONLY - no capacity
	//! decision is ever taken on the float this produces (D3).
	//! \param[in] litres The volume in litres.
	//! \return Cubic metres to one decimal.
	static string FormatCubicMetres(int litres)
	{
		float cubicMetres = litres / 1000.0;

		return cubicMetres.ToString(-1, 1);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a store may hold anything at all. A store authored at 0 m3 is not a holder and must
	//! never appear as a transfer destination.
	//! \param[in] store The store to test.
	//! \return True when the store has an unlimited or a positive litre capacity.
	static bool IsUsableHolder(OVT_ResourceStoreComponent store)
	{
		if (!store)
			return false;

		return store.GetCapacityLitres() != 0;
	}
}

//------------------------------------------------------------------------------------------------
//! Finds every usable resource holder in a radius - the destination picker and the server's own
//! re-derivation of it.
//!
//! ONE INSTANCE PER CALL. The accumulator is a member of this object and is re-created by every
//! Run(), never shared and never static. OVT_InventoryManagerComponent.m_aContainerSearchResults
//! (:497) is a singleton that every concurrent search points at the caller's array in turn, so two
//! players searching at once fill each other's results - the defect this project keeps re-learning.
//! `new` one of these per call, on the server for validation and on the client for the picker.
//------------------------------------------------------------------------------------------------
class OVT_ResourceHolderQuery : Managed
{
	//! Per-instance, re-created by every Run(). The query callback is the only writer.
	protected ref array<IEntity> m_aResults;

	//------------------------------------------------------------------------------------------------
	//! Collects the holders around a point.
	//! \param[in] pos Centre of the search.
	//! \param[in] radius Search radius in metres.
	//! \param[out] results Receives the holders; cleared first, so a reused buffer never accumulates.
	//! \return How many holders were found.
	int Run(vector pos, float radius, out array<IEntity> results)
	{
		if (!results)
			results = new array<IEntity>();

		results.Clear();

		m_aResults = new array<IEntity>();

		BaseWorld world = GetGame().GetWorld();
		if (world)
			world.QueryEntitiesBySphere(pos, radius, null, FilterHolders, EQueryEntitiesFlags.STATIC | EQueryEntitiesFlags.DYNAMIC);

		foreach (IEntity holder : m_aResults)
		{
			results.Insert(holder);
		}

		// Nothing outside Run() may reach into the accumulator, so it does not outlive the call.
		m_aResults = null;

		return results.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Query filter: an entity carrying a store that can hold something.
	//! \param[in] e The entity the query offered.
	//! \return Always false - the query runs to completion.
	protected bool FilterHolders(IEntity e)
	{
		if (!e || !m_aResults)
			return false;

		if (OVT_ResourceUtils.IsUsableHolder(OVT_ResourceUtils.GetStore(e)))
			m_aResults.Insert(e);

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! Finds the crate piles in a radius - the drop-merge decision and a construction site's supply
//! sweep.
//!
//! ONE INSTANCE PER CALL, for the same reason OVT_ResourceHolderQuery is. It filters on the pile
//! MARKER rather than on the store, so a truck parked beside a site is never drained by it.
//------------------------------------------------------------------------------------------------
class OVT_ResourcePileQuery : Managed
{
	//! Per-instance, re-created by every Run(). The query callback is the only writer.
	protected ref array<IEntity> m_aResults;

	//------------------------------------------------------------------------------------------------
	//! Collects the piles around a point.
	//! \param[in] pos Centre of the search.
	//! \param[in] radius Search radius in metres.
	//! \param[out] results Receives the piles; cleared first, so a reused buffer never accumulates.
	//! \return How many piles were found.
	int Run(vector pos, float radius, out array<IEntity> results)
	{
		if (!results)
			results = new array<IEntity>();

		results.Clear();

		m_aResults = new array<IEntity>();

		BaseWorld world = GetGame().GetWorld();
		if (world)
			world.QueryEntitiesBySphere(pos, radius, null, FilterPiles, EQueryEntitiesFlags.STATIC | EQueryEntitiesFlags.DYNAMIC);

		foreach (IEntity pile : m_aResults)
		{
			results.Insert(pile);
		}

		// Nothing outside Run() may reach into the accumulator, so it does not outlive the call.
		m_aResults = null;

		return results.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Query filter: an entity carrying the pile marker.
	//! \param[in] e The entity the query offered.
	//! \return Always false - the query runs to completion.
	protected bool FilterPiles(IEntity e)
	{
		if (!e || !m_aResults)
			return false;

		if (OVT_ResourceUtils.IsPile(e))
			m_aResults.Insert(e);

		return false;
	}
}
