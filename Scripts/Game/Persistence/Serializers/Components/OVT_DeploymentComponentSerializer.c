//------------------------------------------------------------------------------------------------
//! Persists one live deployment: which config it is running, for whom, and how far it has got.
//!
//! BINDING. Listed in the ComponentSerializers block of the DEPLOYMENT entity configuration in
//! Configs/Systems/Persistence/Overthrow.conf, which matches on
//! `ComponentClassPersistenceConfigRule { ComponentClass "OVT_DeploymentComponent" }`.
//!
//! WHY DEPLOYMENTS ARE ENTITY-LEVEL STATE AND THEIR UNITS ARE NOT.
//! A deployment is a marker entity (Prefabs/GameMode/OVT_Deployment.et) that OWNS a virtualized
//! force: OVT_InfantrySpawningDeploymentModule spawns its groups in OnActivate() when a player comes
//! within range and DELETES them again in OnDeactivate(). The AI is therefore transient by design and
//! must not be persisted - it is re-created from the module the next time a player approaches. What
//! cannot be re-derived is the deployment itself: its position, which config it is, whose it is, and
//! whether its force has already been wiped out. That is exactly the split EPF had
//! (OVT_DeploymentSaveData for the entity, OVT_DeploymentComponentSaveData for these fields), and it
//! is why Overthrow.conf turns vanilla's AI-group and AI-character self-spawn OFF.
//!
//! WHY THE MANAGER CANNOT REBUILD THEM INSTEAD. OVT_DeploymentManagerComponent.EvaluateDeployments()
//! does create deployments from the persisted faction resource pools - but it PAYS for each one out
//! of those pools. A campaign that comes back with its resources but none of its deployments would
//! buy the same army twice over its life, and every deployment would land somewhere new. Persisting
//! the deployment entity is what makes the restored pools mean what they meant when they were saved.
//!
//! THE CONFIG TRAVELS BY NAME. OVT_DeploymentConfig is authored data; the save stores
//! m_sDeploymentName and OVT_DeploymentRegistry.FindConfigByName() resolves it on load, so a config
//! that has been retuned since the save takes effect and one that has been deleted drops its
//! deployments instead of crashing. EPF stored the same key.
//!
//! NO SPAWNING, NO REGISTRATION HERE. Deserialize is a pure codec; every side effect - cloning the
//! config's modules, registering with the manager, starting the update loop - happens in
//! OVT_DeploymentComponent.ApplyPersistedDeployment(), which is idempotent.
//!
//! FORMAT. Binary contexts are POSITIONAL: write order must equal read order. Version first.
//------------------------------------------------------------------------------------------------
class OVT_DeploymentComponentSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	//! \return The component class this serializer is responsible for.
	override static typename GetTargetType()
	{
		return OVT_DeploymentComponent;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the deployment's config name, faction, threat, invested resources and wipe-out flag.
	//! \param[in] owner The deployment marker entity.
	//! \param[in] component The deployment component being saved.
	//! \param[in] context Save context to write into.
	//! \return OK, or ERROR when the component is not an Overthrow deployment.
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		OVT_DeploymentComponent deployment = OVT_DeploymentComponent.Cast(component);
		if (!deployment)
			return ESerializeResult.ERROR;

		context.WriteValue("version", 1);

		string configName;
		OVT_DeploymentConfig config = deployment.GetConfig();
		if (config)
			configName = config.m_sDeploymentName;
		context.Write(configName);

		const int controllingFaction = deployment.GetControllingFaction();
		context.Write(controllingFaction);

		const float threatLevel = deployment.GetThreatLevel();
		context.Write(threatLevel);

		const int resourcesInvested = deployment.GetResourcesInvested();
		context.Write(resourcesInvested);

		const bool spawnedUnitsEliminated = deployment.GetSpawnedUnitsEliminated();
		context.Write(spawnedUnitsEliminated);

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the deployment back and hands it to the component to apply.
	//! \param[in] owner The deployment marker entity.
	//! \param[in] component The deployment component being loaded.
	//! \param[in] context Load context to read from.
	//! \return True when the payload was consumed.
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		OVT_DeploymentComponent deployment = OVT_DeploymentComponent.Cast(component);
		if (!deployment)
			return false;

		// No version means no payload - see OVT_TownManagerSerializer.Deserialize(). A deployment with
		// no stored config name has nothing to initialize from, so leaving it alone is the only safe
		// answer; the manager's cleanup pass will collect it if it never becomes real.
		int version;
		context.ReadValue("version", version);
		if (version < 1)
			return true;

		string configName;
		context.Read(configName);

		int controllingFaction;
		context.Read(controllingFaction);

		float threatLevel;
		context.Read(threatLevel);

		int resourcesInvested;
		context.Read(resourcesInvested);

		bool spawnedUnitsEliminated;
		context.Read(spawnedUnitsEliminated);

		deployment.ApplyPersistedDeployment(configName, controllingFaction, threatLevel, resourcesInvested, spawnedUnitsEliminated);

		return true;
	}
}
