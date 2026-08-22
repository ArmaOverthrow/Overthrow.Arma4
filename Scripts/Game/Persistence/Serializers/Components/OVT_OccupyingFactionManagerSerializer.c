//------------------------------------------------------------------------------------------------
//! One AI group belonging to a base upgrade, as prefab plus where it stood.
//! Mirrors OVT_BaseUpgradeGroupData field for field - see OVT_PersistedBase's header for why the
//! save carries its own record classes rather than the live ones.
//------------------------------------------------------------------------------------------------
class OVT_PersistedBaseUpgradeGroup
{
	string prefab;
	vector position;
}

//------------------------------------------------------------------------------------------------
//! One base upgrade's persisted state, from a PRE-MIGRATION save. Mirrors OVT_BaseUpgradeData field
//! for field.
//!
//! ⚠ NOTHING WRITES ONE OF THESE ANY MORE AND NOTHING REPLAYS ONE. The base-upgrade system this
//! payload described is gone; the class stays declared because it is the element type of
//! OVT_PersistedBase.upgrades, which is field 4 of 5 in a POSITIONAL binary context, and because an
//! old save still has to be readable. `type` used to be the key that found the live upgrade object to
//! rebuild from; it is now read only to tell a composition/checkpoint record (worth nothing - the
//! structure comes back from vanilla persistence on its own) apart from a patrol record (worth its
//! banked resources plus its group count). See OVT_OccupyingFactionManager.ApplyPersistedBaseUpgrades()
//! and OVT_BaseDefenseConversion.
//------------------------------------------------------------------------------------------------
class OVT_PersistedBaseUpgrade
{
	string type;
	int resources;
	string tag;
	vector pos;

	ref array<ref OVT_PersistedBaseUpgradeGroup> groups = {};
}

//------------------------------------------------------------------------------------------------
//! One base's persisted state.
//!
//! MATCHED BY LOCATION. Bases, like towns, are world-derived: OVT_OccupyingFactionManager.
//! InitializeBases() finds them by querying the world during Init. A save cannot create one, so the
//! location is a match key and never a spawn instruction.
//!
//! AN OCCUPYING-FACTION BASE stores its filled slots (and, in a PRE-MIGRATION save only, the upgrade
//! records that half is named for). A base the resistance has taken stores nothing beyond its faction:
//! InitBaseControllers() consumes the slot claims and nothing else.
//!
//! ⚠ AN OCCUPYING BASE'S DEFENCE IS NO LONGER IN THIS PAYLOAD AT ALL. It is a set of deployments,
//! persisted by the deployment framework's own serializer and re-created group by group by the
//! virtualization core, so a base comes back with the men it had rather than with a recipe for
//! rebuilding them. What survives here is the SLOT CLAIM, which is what stops a deployment putting a
//! second structure where one is already standing.
//------------------------------------------------------------------------------------------------
class OVT_PersistedBase
{
	vector location;
	int faction;

	ref array<vector> slotsFilled = {};
	ref array<ref OVT_PersistedBaseUpgrade> upgrades = {};

	//! VERSION 1-2 ONLY, AND THE FIELD MUST NOT BE REMOVED. Written EMPTY from version 3 on. Reflection
	//! reads and writes these members in declaration order and the save context is binary, so dropping
	//! this field would change the record's length and desynchronise every record after the first in
	//! every existing save. An older payload's contents are read into it and discarded.
	ref array<ResourceName> garrison = {};
}

//------------------------------------------------------------------------------------------------
//! One radio tower's persisted state: who controls it, and whether it is currently off the air.
//!
//! FIELD ORDER IS THE FORMAT. Reflection writes and reads these members in declaration order and the
//! save context is binary, so a new field may only ever be APPENDED - never inserted, never
//! reordered, never removed. disabledRemaining is the version 2 addition and therefore sits last.
//------------------------------------------------------------------------------------------------
class OVT_PersistedRadioTower
{
	vector location;
	int faction;

	//! Version 2. Seconds of sabotage downtime left, so a tower sabotaged before the save comes back
	//! still down with the right time on the clock instead of silently back on the air. 0 when up.
	float disabledRemaining;
}

//------------------------------------------------------------------------------------------------
//! Persists the occupying faction's war state: resources, threat, base and radio tower control, and
//! how long any sabotaged tower still has left off the air.
//!
//! BINDING. Listed in the ComponentSerializers block of the game-mode configuration in
//! Configs/Systems/Persistence/Overthrow.conf.
//!
//! THE OCCUPYING FACTION KEY IS PART OF THE SAVE. Which faction is occupying is a campaign
//! DECISION taken once at DoStartNewGame() (from Overthrow_Config.json or the configured default),
//! not a static config value, and every faction index in the rest of this payload is relative to it.
//! It is applied first on load for that reason. EPF stored it in the same place.
//!
//! WHY THE OPENING RESOURCE SEED MUST BE SUPPRESSED ON A RESTORE.
//! OVT_OccupyingFactionManager.NewGameStart() credits the occupying faction's deployment pool with an
//! opening defense budget - one tick of base income plus every base's authored starting allocation -
//! and gates it on m_bDistributeInitial. A saved campaign has already had that budget and has spent
//! some of it, so ApplyPersistedOccupyingFaction() clears the flag, which is precisely what EPF's
//! OVT_OccupyingFactionSaveData.ApplyTo() did. (The generation path itself cannot run on a continue -
//! RestoreStartedCampaign() runs DoStartGame() and never DoStartNewGame() - so the flag is the second
//! line of defence rather than the only one.)
//!
//! WHAT REBUILDS THE WORLD. Nothing here spawns anything. The restored slot lists sit on the base
//! records until InitBaseControllers() runs during PostGameStart, and IT consumes them - the same path
//! a campaign takes when it starts. Deserialization must not spawn, both because the world is still
//! loading and because the same payload can be re-applied to a live session.
//!
//! A PRE-MIGRATION PAYLOAD IS CONVERTED, NOT REPLAYED. Saves written before the base-defense migration
//! carry per-base upgrade records. Those upgrades no longer exist, so their VALUE is summed and
//! refunded to the occupying faction's deployment resource pool once, and the deployment evaluator
//! re-establishes defense from it - value-parity, not entity-identity. See
//! OVT_OccupyingFactionManager.ApplyPersistedBaseUpgrades() and WriteBase() below for why the payload
//! field survives the change and why the conversion cannot double-count.
//!
//! POST-LOAD. No RPC, deliberately. Clients receive base and tower factions through this
//! component's JIP path and the base controllers' own replication.
//!
//! IDEMPOTENT ON A LIVE SESSION. Deserialize also runs when saved data is re-applied to a running
//! campaign (OVT_PersistenceManagerComponent.ReapplyLatestSaveData). Everything applied is either an
//! assignment or a clear-and-rebuild of a data list; no spawning, no double-allocation.
//!
//! FORMAT. Binary contexts are POSITIONAL: write order must equal read order. Version first.
//!
//! VERSION 3 RETIRED THE GARRISONS. A resistance-held base's garrison prefab list is no longer written
//! and no longer applied; the payload field stays declared and is still read, because the format is
//! positional. A campaign continued from a version 1 or 2 save comes back with its bases and WITHOUT
//! their garrisons - High Command groups replace them.
//------------------------------------------------------------------------------------------------
class OVT_OccupyingFactionManagerSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	//! \return The component class this serializer is responsible for.
	override static typename GetTargetType()
	{
		return OVT_OccupyingFactionManager;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the occupying faction key, its resources and threat, then the bases and radio towers.
	//! \param[in] owner The game mode entity owning the occupying faction manager.
	//! \param[in] component The occupying faction manager being saved.
	//! \param[in] context Save context to write into.
	//! \return OK, or ERROR when the component is not an Overthrow occupying faction manager.
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		OVT_OccupyingFactionManager occupying = OVT_OccupyingFactionManager.Cast(component);
		if (!occupying)
			return ESerializeResult.ERROR;

		context.WriteValue("version", 3);

		string occupyingFactionKey;
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (config)
			occupyingFactionKey = config.m_sOccupyingFaction;
		context.Write(occupyingFactionKey);

		const int resources = occupying.m_iResources;
		context.Write(resources);

		const float threat = occupying.m_iThreat;
		context.Write(threat);

		array<ref OVT_PersistedBase> bases = new array<ref OVT_PersistedBase>();
		if (occupying.m_Bases)
		{
			foreach (OVT_BaseData base : occupying.m_Bases)
			{
				if (!base)
					continue;

				bases.Insert(WriteBase(base));
			}
		}
		context.Write(bases);

		array<ref OVT_PersistedRadioTower> towers = new array<ref OVT_PersistedRadioTower>();
		if (occupying.m_RadioTowers)
		{
			foreach (OVT_RadioTowerData tower : occupying.m_RadioTowers)
			{
				if (!tower)
					continue;

				OVT_PersistedRadioTower record = new OVT_PersistedRadioTower();
				record.location = tower.location;
				record.faction = tower.faction;
				record.disabledRemaining = tower.disabledRemaining;
				towers.Insert(record);
			}
		}
		context.Write(towers);

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the war state back and hands it to the manager to apply.
	//! \param[in] owner The game mode entity owning the occupying faction manager.
	//! \param[in] component The occupying faction manager being loaded.
	//! \param[in] context Load context to read from.
	//! \return True when the payload was consumed.
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		OVT_OccupyingFactionManager occupying = OVT_OccupyingFactionManager.Cast(component);
		if (!occupying)
			return false;

		// See OVT_TownManagerSerializer.Deserialize(). It matters most here: without this guard an
		// absent payload would zero the occupying faction's resources and threat and suppress its
		// opening resource distribution, gutting a campaign that had nothing wrong with it.
		int version;
		context.ReadValue("version", version);
		if (version < 1)
			return true;

		// A version 1 or 2 payload still carries a garrison prefab list on every base record. It is read
		// (the format is positional, so it has to be) and then discarded - nothing replays it.
		string occupyingFactionKey;
		if (!context.Read(occupyingFactionKey))
			return AbortUnreadablePayload("occupying faction key");

		int resources;
		if (!context.Read(resources))
			return AbortUnreadablePayload("resources");

		float threat;
		if (!context.Read(threat))
			return AbortUnreadablePayload("threat");

		array<ref OVT_PersistedBase> bases = new array<ref OVT_PersistedBase>();
		if (!context.Read(bases))
			return AbortUnreadablePayload("bases");

		array<ref OVT_PersistedRadioTower> towers = new array<ref OVT_PersistedRadioTower>();
		if (!context.Read(towers))
			return AbortUnreadablePayload("radio towers");

		// A version 1 payload was written before towers could be sabotaged. The field is the LAST one
		// declared, so an older payload simply has no data for it - but whatever the reader left there
		// is not ours, so it is cleared rather than trusted. Every tower from such a save is on the
		// air, which is exactly what version 1 promised.
		if (version < 2)
			ClearDisabledRemaining(towers);

		occupying.ApplyPersistedOccupyingFaction(occupyingFactionKey, resources, threat, bases, towers);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Reports an unreadable payload and consumes it without touching the live war state.
	//!
	//! NOTHING IS APPLIED after a failed read: a binary context is positional, so once one field has
	//! not come back every field after it is another field's bytes, and a half-read payload would zero
	//! the occupying faction's resources and threat on a campaign that had nothing wrong with it.
	//! \param[in] field The field that could not be read.
	//! \return True - the payload is consumed either way; nothing was applied.
	protected bool AbortUnreadablePayload(string field)
	{
		Print(string.Format("[Overthrow] Could not read '%1' from the occupying faction payload - the live war state is left exactly as it is", field), LogLevel.ERROR);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Blanks the version 2 sabotage timer on every record of an older payload.
	//! \param[in] towers Records just read, may be null.
	protected void ClearDisabledRemaining(array<ref OVT_PersistedRadioTower> towers)
	{
		if (!towers)
			return;

		foreach (OVT_PersistedRadioTower tower : towers)
		{
			if (tower)
				tower.disabledRemaining = 0;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Snapshots one base into a save record.
	//!
	//! Reads the live base CONTROLLER for an occupying-faction base, because that is where the filled
	//! slots actually live; the OVT_BaseData record only carries what has already been flattened for
	//! storage. Unlike EPF's version this leaves the live data untouched - saving is not allowed to
	//! mutate the campaign.
	//!
	//! ⚠ THE `upgrades` AND `garrison` ARRAYS ARE WRITTEN EMPTY, ALWAYS, AND NEITHER FIELD IS REMOVED.
	//! Base upgrades no longer exist - base defense is nine Deployment_Base*.conf configs, persisted by
	//! the deployment framework's own serializer - and garrisons were retired with High Command, so
	//! there is nothing left to walk in either. Both FIELDS stay declared and stay written because
	//! OVT_PersistedBase is read POSITIONALLY: dropping either would change the record's length and
	//! every existing save would read one base's fields out of another's. The read side still consumes
	//! `upgrades`, once, to convert a pre-migration campaign's investment into a deployment-pool refund
	//! (OVT_OccupyingFactionManager.ApplyPersistedBaseUpgrades); `garrison` is read and discarded.
	//! Writing them empty here is exactly what makes that conversion idempotent without a flag: the
	//! first save after a legacy load rewrites the payload, and every load after that converts zero.
	//! \param[in] base The live base data.
	//! \return A populated record. Never null.
	protected OVT_PersistedBase WriteBase(notnull OVT_BaseData base)
	{
		OVT_PersistedBase record = new OVT_PersistedBase();
		record.location = base.location;
		record.faction = base.faction;

		// record.upgrades and record.garrison stay the empty arrays they were constructed with.
		if (!base.IsOccupyingFaction())
			return record;

		OVT_BaseControllerComponent controller = FindBaseController(base);
		if (!controller || !controller.m_aSlotsFilled)
			return record;

		foreach (EntityID slotId : controller.m_aSlotsFilled)
		{
			IEntity slot = GetGame().GetWorld().FindEntityByID(slotId);
			if (slot)
				record.slotsFilled.Insert(slot.GetOrigin());
		}

		return record;
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves a base's controller component without going through
	//! OVT_OccupyingFactionManager.GetBase(), which dereferences a null entity when the marker has
	//! gone away.
	//! \param[in] base The live base data.
	//! \return The controller, or null.
	protected OVT_BaseControllerComponent FindBaseController(notnull OVT_BaseData base)
	{
		IEntity marker = GetGame().GetWorld().FindEntityByID(base.entId);
		if (!marker)
			return null;

		return OVT_BaseControllerComponent.Cast(marker.FindComponent(OVT_BaseControllerComponent));
	}

}
