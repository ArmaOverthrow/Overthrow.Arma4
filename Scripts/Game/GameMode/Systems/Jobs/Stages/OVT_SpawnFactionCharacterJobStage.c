//------------------------------------------------------------------------------------------------
//! Spawns ONE named character belonging to one of Overthrow's configured factions at job.location,
//! and records it as the job's entity so a later OVT_WaitTillDeadJobStage can wait for it to die.
//!
//! WHY NOT JUST NAME A PREFAB. Which faction occupies is campaign configuration, so a job config
//! that names a literal prefab ("Character_USSR_Officer") is silently wrong the moment a campaign
//! makes the US the occupier: you get a Soviet model wearing the US faction's colours. This stage
//! names the character by ROLE ("Officer") and lets each faction's *_OverthrowData.conf say which
//! prefab that is - the same indirection the group and vehicle registries already use for squads
//! and trucks.
//!
//! Use OVT_SpawnCivilianJobStage instead when the job wants one SPECIFIC authored character
//! regardless of faction (the traitor in assassinateTraitor is a named FIA prefab affiliated CIV).
//! Use this one whenever the character is "whoever plays this role for that faction".
//!
//! Side-effecting stage: everything happens in OnStart, which returns false to advance immediately.
//! That is what keeps the no-replay persistence restore correct - see core/persistence. A job that
//! comes back resting on the FOLLOWING stage keeps the target it already has (job.entity is a live
//! RplId, which is exactly why FindRestorableJobConfig() drops jobs parked on OVT_WaitTillDeadJobStage).
//------------------------------------------------------------------------------------------------
class OVT_SpawnFactionCharacterJobStage : OVT_JobStage
{
	[Attribute("0", UIWidgets.ComboBox, "Which faction this character belongs to", "", ParamEnumArray.FromEnum(OVT_FactionType))]
	OVT_FactionType m_Faction;

	[Attribute("Officer", UIWidgets.EditBox, "Character name in that faction's character registry (see *_OverthrowData.conf)")]
	string m_sCharacterName;

	override bool OnStart(OVT_Job job)
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if(!config) return false;

		OVT_Faction faction = config.GetFactionByType(m_Faction);
		if(!faction)
		{
			Print("[Overthrow] OVT_SpawnFactionCharacterJobStage: no faction configured for this type", LogLevel.WARNING);
			return false;
		}

		ResourceName prefab = faction.GetCharacterPrefab(m_sCharacterName);
		if(prefab.IsEmpty())
		{
			Print("[Overthrow] OVT_SpawnFactionCharacterJobStage: faction " + faction.GetFactionKey() + " has no character named '" + m_sCharacterName + "' - add one to its character registry", LogLevel.WARNING);
			return false;
		}

		vector spawnPosition = OVT_Global.FindSafeSpawnPosition(job.location);

		IEntity entity = OVT_Global.SpawnEntityPrefab(prefab, spawnPosition);
		if(!entity)
		{
			Print("[Overthrow] OVT_SpawnFactionCharacterJobStage: could not spawn " + prefab, LogLevel.WARNING);
			return false;
		}

		//! BUG-118: the job system replays this stage on every restore, so the target is rebuilt
		//! each boot and nothing ever recalls it by persistence id - a record for it (plus its whole
		//! nested kit, ~32 records) would be a permanent orphan. This was the last leak: one job
		//! target per restart. A lone character bypasses the SCR_AIGroup chokepoint, so it is
		//! released here.
		OVT_PersistenceManagerComponent.UntrackTransient(entity);

		//! Without an RplComponent there is no id for OVT_WaitTillDeadJobStage to wait on, and the
		//! character would not exist on any client
		RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		if(rpl)
		{
			job.entity = rpl.Id();
		}
		else
		{
			Print("[Overthrow] OVT_SpawnFactionCharacterJobStage: " + prefab + " has no RplComponent - the job cannot track it", LogLevel.WARNING);
		}

		//! The prefab usually carries the right affiliation already, but it is only right by
		//! coincidence: the registry entry is whatever the faction author picked. Setting it from the
		//! faction's own key makes the spawn hostile to the player and friendly to its own garrison
		//! no matter which prefab was registered
		FactionAffiliationComponent fac = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
		if(fac)
		{
			fac.SetAffiliatedFactionByKey(faction.GetFactionKey());
		}

		return false;
	}
}
