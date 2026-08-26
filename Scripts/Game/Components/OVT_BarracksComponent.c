//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "Overthrow/Components", description: "Marks a structure as a High Command barracks")]
class OVT_BarracksComponentClass : OVT_ComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Identity marker for a High Command barracks. It holds no state, replicates nothing and does no
//! work - the OVT_ResourcePileComponent role.
//!
//! WHY A MARKER AND NOT AN RplId ON THE WIRE (D7). The vanilla barracks overrides are deltas of
//! building prefabs that may or may not carry an RplComponent, and a request that marshals a handle
//! the client chose is client trust. The purchase ask is therefore RpcAsk_PurchaseGroup(int) and the
//! server finds the barracks itself, with FindNearest() below - the same query that answers "is there
//! a barracks here" for the client-side action gate, so the two can never disagree.
//------------------------------------------------------------------------------------------------
class OVT_BarracksComponent : OVT_Component
{
	//! How close a player must stand to a barracks to use it, in metres.
	static const float BARRACKS_USE_RADIUS = 5.0;

	//------------------------------------------------------------------------------------------------
	//! The nearest barracks to a point, or null when there is none in range.
	//!
	//! ONE QUERY OBJECT PER CALL - see OVT_BarracksQuery. Never a static accumulator: two players at
	//! two barracks in the same frame would otherwise read each other's result.
	//! \param[in] pos Centre of the search.
	//! \param[in] radius Search radius in metres.
	//! \return The nearest barracks component, or null.
	static OVT_BarracksComponent FindNearest(vector pos, float radius)
	{
		array<OVT_BarracksComponent> found = new array<OVT_BarracksComponent>();

		OVT_BarracksQuery query = new OVT_BarracksQuery();
		query.Run(pos, radius, found);

		OVT_BarracksComponent nearest;
		float nearestDistanceSq = -1;

		foreach (OVT_BarracksComponent barracks : found)
		{
			IEntity owner = barracks.GetOwner();
			if (!owner)
				continue;

			float distanceSq = vector.DistanceSq(owner.GetOrigin(), pos);
			if (nearestDistanceSq < 0 || distanceSq < nearestDistanceSq)
			{
				nearestDistanceSq = distanceSq;
				nearest = barracks;
			}
		}

		return nearest;
	}
}

//------------------------------------------------------------------------------------------------
//! Finds every barracks in a radius.
//!
//! ONE INSTANCE PER CALL. The accumulator is a member of this object and is re-created by every
//! Run() - the OVT_StorageHolderQuery shape, for the reason its header gives.
//------------------------------------------------------------------------------------------------
class OVT_BarracksQuery : Managed
{
	//! Per-instance, re-created by every Run(). The query callback is the only writer.
	protected ref array<OVT_BarracksComponent> m_aResults;

	//------------------------------------------------------------------------------------------------
	//! Collects the barracks around a point.
	//! \param[in] pos Centre of the search.
	//! \param[in] radius Search radius in metres.
	//! \param[out] results Receives the barracks; cleared first.
	//! \return How many were found.
	int Run(vector pos, float radius, out array<OVT_BarracksComponent> results)
	{
		if (!results)
			results = new array<OVT_BarracksComponent>();

		results.Clear();

		m_aResults = new array<OVT_BarracksComponent>();

		BaseWorld world = GetGame().GetWorld();
		if (world)
			world.QueryEntitiesBySphere(pos, radius, null, FilterBarracks, EQueryEntitiesFlags.STATIC | EQueryEntitiesFlags.DYNAMIC);

		foreach (OVT_BarracksComponent barracks : m_aResults)
		{
			results.Insert(barracks);
		}

		// Nothing outside Run() may reach into the accumulator, so it does not outlive the call.
		m_aResults = null;

		return results.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Query filter. A barracks is any entity - or child of one - carrying the marker component.
	//! \param[in] e The entity the query offered.
	//! \return Always false - the query runs to completion.
	protected bool FilterBarracks(IEntity e)
	{
		if (!e || !m_aResults)
			return false;

		OVT_BarracksComponent barracks = OVT_ComponentFinder<OVT_BarracksComponent>.Find(e);
		if (!barracks)
		{
			IEntity root = e.GetRootParent();
			if (root && root != e)
				barracks = OVT_ComponentFinder<OVT_BarracksComponent>.Find(root);
		}

		if (barracks && m_aResults.Find(barracks) == -1)
			m_aResults.Insert(barracks);

		return false;
	}
}
