//------------------------------------------------------------------------------------------------
//! Finds the vehicles around a point.
//!
//! ONE INSTANCE PER CALL, like every other query in this feature: the accumulator is a member that
//! Run() re-creates, never a static. The filter is the shipped ClassName() == "Vehicle" test the
//! Load/Unload actions have always used.
//------------------------------------------------------------------------------------------------
class OVT_StorageVehicleQuery : Managed
{
	//! Per-instance, re-created by every Run(). The query callback is the only writer.
	protected ref array<IEntity> m_aResults;

	//------------------------------------------------------------------------------------------------
	//! The nearest vehicle to a point.
	//! \param[in] pos Centre of the search.
	//! \param[in] radius Sphere query radius in metres.
	//! \param[in] maxDistance How far the chosen vehicle may be.
	//! \return The nearest vehicle inside \a maxDistance, or null.
	IEntity FindNearest(vector pos, float radius, float maxDistance)
	{
		m_aResults = new array<IEntity>();

		BaseWorld world = GetGame().GetWorld();
		if (world)
			world.QueryEntitiesBySphere(pos, radius, null, FilterVehicles, EQueryEntitiesFlags.ALL);

		IEntity nearest;
		float nearestDistance = maxDistance;

		foreach (IEntity candidate : m_aResults)
		{
			float distance = vector.Distance(candidate.GetOrigin(), pos);
			if (distance < nearestDistance)
			{
				nearest = candidate;
				nearestDistance = distance;
			}
		}

		m_aResults = null;

		return nearest;
	}

	//------------------------------------------------------------------------------------------------
	//! Query filter.
	//! \param[in] e The entity the query offered.
	//! \return Always false - the query runs to completion.
	protected bool FilterVehicles(IEntity e)
	{
		if (!e || !m_aResults)
			return false;

		if (e.ClassName() == "Vehicle")
			m_aResults.Insert(e);

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! Shared half of Load Storage and Unload Storage: pick the nearest vehicle, refuse a driven one,
//! and hand both ends to the storage request component as one whole-ledger move.
//!
//! Both actions are STORAGE ONLY as of logistics/storage - neither touches the player's inventory or
//! moves entities between two vanilla storages any more, which is why the base is
//! OVT_StorageActionBase (the MayUseHolder client mirror) rather than SCR_InventoryAction.
//------------------------------------------------------------------------------------------------
class OVT_StorageVehicleActionBase : OVT_StorageActionBase
{
	//! Authored, defaulting to the shipped ammobox numbers. A warehouse needs bigger ones: the search
	//! is measured from the OWNER's origin, and a building's origin is its centre, so a truck at the
	//! loading door is already outside the crate-sized 15 m.
	[Attribute(defvalue: "10", desc: "Sphere query radius (m) for candidate vehicles, measured from this action's owner")]
	protected float m_fVehicleSearchRadius;

	[Attribute(defvalue: "15", desc: "How far (m) the chosen vehicle may be from this action's owner")]
	protected float m_fVehicleMaxDistance;

	//------------------------------------------------------------------------------------------------
	//! The vehicle this action moves storage to or from, with every shipped refusal reported.
	//! \param[in] pOwnerEntity The box the action sits on.
	//! \return A usable vehicle holder, or null when one of the gates refused.
	protected IEntity ResolveVehicle(IEntity pOwnerEntity)
	{
		OVT_StorageVehicleQuery query = new OVT_StorageVehicleQuery();
		IEntity nearest = query.FindNearest(pOwnerEntity.GetOrigin(), m_fVehicleSearchRadius, m_fVehicleMaxDistance);

		if (!nearest)
		{
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-NoVehiclesNearby");
			return null;
		}

		SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(nearest.FindComponent(SCR_BaseCompartmentManagerComponent));
		if (compartments)
		{
			array<IEntity> pilots = {};
			compartments.GetOccupantsOfType(pilots, ECompartmentType.PILOT);

			if (!pilots.IsEmpty())
			{
				SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-DriverMustExit");
				return null;
			}
		}

		OVT_StorageComponent storage = OVT_StorageUtils.GetStorage(nearest);
		if (!storage || storage.GetCapacity() == OVT_StorageComponent.NO_CAPACITY)
		{
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-Storage_NoCapacity");
			return null;
		}

		return nearest;
	}

	//------------------------------------------------------------------------------------------------
	//! Sends the whole-ledger move.
	//! \param[in] source The holder emptied.
	//! \param[in] dest The holder filled.
	//! \param[in] sweepFirst Whether the source's vanilla inventory is converted into its ledger first.
	protected void SendMove(IEntity source, IEntity dest, bool sweepFirst)
	{
		RplId sourceId = OVT_StorageUtils.GetHolderId(source);
		RplId destId = OVT_StorageUtils.GetHolderId(dest);

		if (!sourceId.IsValid() || !destId.IsValid())
			return;

		OVT_StorageRequestComponent requests = GetRequests();
		if (!requests)
			return;

		requests.RequestMoveAllToHolder(sourceId, destId, sweepFirst);
	}
}
