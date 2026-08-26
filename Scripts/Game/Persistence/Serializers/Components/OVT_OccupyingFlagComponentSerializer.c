//------------------------------------------------------------------------------------------------
//! Brings the occupying faction's forward operating base back across a load.
//!
//! BINDING. Listed in the ComponentSerializers block of the OCCUPYING FLAG entity configuration in
//! Configs/Systems/Persistence/Overthrow.conf, which matches on
//! `ComponentClassPersistenceConfigRule { ComponentClass "OVT_OccupyingFlagComponent" }`.
//!
//! ⚠ THE CONFIG IS THE FIX, THIS FILE IS ITS TAIL. OVT_FOBRaiseSpawningDeploymentModule tracks the
//! structure at its spawn site (OVT_PersistenceTracking.Track), and its header used to claim that
//! tracking alone was enough for "vanilla persistence saves it and puts it back". It is not: an
//! entity only comes back when the configuration it MATCHES has SelfSpawn set, and before this
//! configuration existed the FOB matched none of Overthrow's four rules. MEASURED 2026-08-23 by
//! decoding a save blob: zero records for the structure, while the deployment that garrisons it
//! (OVT_DeploymentComponent, SelfSpawn 1) came back every time - a garrison standing around a base
//! that was not there, and an objective the player could no longer end because the dismantle action
//! rides on the flagpole.
//!
//! NO COMPONENT STATE. OVT_OccupyingFlagComponent holds one field, the faction index of the material
//! currently on the pole, and its own 10-second re-check re-derives it from the campaign config on
//! every machine. Storing it would be a second answer that a campaign started against the other
//! occupier could contradict. The version int is written anyway so a later field has somewhere to go.
//!
//! FORMAT. Binary contexts are POSITIONAL: write order must equal read order. Version first.
//------------------------------------------------------------------------------------------------
class OVT_OccupyingFlagComponentSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	//! \return The component class this serializer is responsible for.
	override static typename GetTargetType()
	{
		return OVT_OccupyingFlagComponent;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] owner The forward base structure.
	//! \param[in] component The flag component being saved.
	//! \param[in] context Save context to write into.
	//! \return OK, or ERROR when the component is not an occupying flag.
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		OVT_OccupyingFlagComponent flag = OVT_OccupyingFlagComponent.Cast(component);
		if (!flag)
			return ESerializeResult.ERROR;

		context.WriteValue("version", 1);

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] owner The respawned forward base structure.
	//! \param[in] component The flag component being loaded.
	//! \param[in] context Load context to read from.
	//! \return True when the payload was consumed.
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		OVT_OccupyingFlagComponent flag = OVT_OccupyingFlagComponent.Cast(component);
		if (!flag)
			return false;

		// THE NAVMESH DOES NOT COME BACK WITH THE STRUCTURE - the placeable serializer's note applies
		// verbatim. The raise path calls RebuildNow() at the moment the base goes up, which is the only
		// reason the AI pathing around it looks right in the session it was raised in. Queued, and
		// before the version guard: the structure is blocking pathfinding whether or not it brought a
		// payload with it.
		OVT_NavmeshRebuild.Queue(owner);

		int version;
		context.ReadValue("version", version);
		if (version < 1)
			return true;

		return true;
	}
}
