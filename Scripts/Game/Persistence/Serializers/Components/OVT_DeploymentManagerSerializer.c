//------------------------------------------------------------------------------------------------
//! Persists the deployment manager's per-faction resource pools.
//!
//! BINDING. Listed in the ComponentSerializers block of the game-mode configuration in
//! Configs/Systems/Persistence/Overthrow.conf.
//!
//! SCOPE - MANAGER-LEVEL STATE ONLY, AND THAT MATCHES WHAT EPF PERSISTED.
//! OVT_DeploymentManagerComponentSaveData stored exactly one thing: the faction resource map. The
//! manager's other state is entity-shaped and cannot be restored from here:
//!
//!   m_aActiveDeployments / m_mFactionDeployments hold the ENTITY IDS of deployment entities that
//!   were spawned into the world. Entity ids do not survive a session, and the entities themselves
//!   are separate tracked instances - restoring them is Phase 3 (world entities), not this phase.
//!   A continued campaign therefore comes back with its resource pools intact and no live
//!   deployments; EvaluateDeployments() re-populates them from those pools on its normal timer.
//!
//!   m_aAvailableSlots is a cache rebuilt from the world by CacheAvailableSlots() during Init and
//!   is not state at all.
//!
//! RESOURCES TRAVEL AS TWO PARALLEL ARRAYS rather than as a map keyed by faction index. Faction
//! INDICES are positional - they are whatever order the faction manager registered factions in - so
//! the pair is written explicitly and re-associated on load, which also keeps the payload to the
//! primitive array shapes the binary context is known to round-trip.
//!
//! POST-LOAD. No RPC: the deployment manager is server-only (its collections are not even allocated
//! on clients) and nothing about resource pools is replicated.
//!
//! IDEMPOTENT ON A LIVE SESSION. Deserialize also runs when saved data is re-applied to a running
//! campaign (OVT_PersistenceManagerComponent.ReapplyLatestSaveData). The apply is a clear-and-refill
//! of one map, so a second pass produces the same map.
//!
//! FORMAT. Binary contexts are POSITIONAL: write order must equal read order. Version first.
//------------------------------------------------------------------------------------------------
class OVT_DeploymentManagerSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	//! \return The component class this serializer is responsible for.
	override static typename GetTargetType()
	{
		return OVT_DeploymentManagerComponent;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the per-faction resource pools.
	//! \param[in] owner The game mode entity owning the deployment manager.
	//! \param[in] component The deployment manager being saved.
	//! \param[in] context Save context to write into.
	//! \return OK, or ERROR when the component is not an Overthrow deployment manager.
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		OVT_DeploymentManagerComponent deployments = OVT_DeploymentManagerComponent.Cast(component);
		if (!deployments)
			return ESerializeResult.ERROR;

		context.WriteValue("version", 1);

		array<int> factionIndices = new array<int>();
		array<int> factionResources = new array<int>();

		map<int, int> resources = deployments.GetAllFactionResources();
		if (resources)
		{
			for (int i = 0; i < resources.Count(); i++)
			{
				factionIndices.Insert(resources.GetKey(i));
				factionResources.Insert(resources.GetElement(i));
			}
		}

		context.Write(factionIndices);
		context.Write(factionResources);

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the resource pools back and hands them to the manager to apply.
	//! \param[in] owner The game mode entity owning the deployment manager.
	//! \param[in] component The deployment manager being loaded.
	//! \param[in] context Load context to read from.
	//! \return True when the payload was consumed.
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		OVT_DeploymentManagerComponent deployments = OVT_DeploymentManagerComponent.Cast(component);
		if (!deployments)
			return false;

		// See OVT_TownManagerSerializer.Deserialize(). Without this guard an absent payload would clear
		// every faction's deployment budget.
		int version;
		context.ReadValue("version", version);
		if (version < 1)
			return true;

		array<int> factionIndices = new array<int>();
		context.Read(factionIndices);

		array<int> factionResources = new array<int>();
		context.Read(factionResources);

		deployments.ApplyPersistedFactionResources(factionIndices, factionResources);

		return true;
	}
}
