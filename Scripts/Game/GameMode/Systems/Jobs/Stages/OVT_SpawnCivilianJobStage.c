//------------------------------------------------------------------------------------------------
//! Spawns the job's target character at job.location and records it as the job's entity, so a later
//! OVT_WaitTillDeadJobStage can wait for it to die.
//!
//! AFFILIATION IS CAMPAIGN-RELATIVE, NOT A LITERAL KEY. m_bCivilian (the default, and what every
//! shipped job uses) affiliates the spawn with CIV: an unarmed target who will not fight back.
//! Clearing it affiliates the spawn with one of Overthrow's configured factions instead, resolved
//! through OVT_OverthrowConfigComponent at spawn time - so a target meant to be "the enemy" is the
//! enemy of whoever occupies THIS campaign. A literal faction key in the .conf would be silently
//! wrong on any campaign whose occupier differs.
//!
//! Side-effecting stage: everything happens in OnStart, which returns false to advance immediately.
//! That is what keeps the no-replay persistence restore correct - see core/persistence.
//------------------------------------------------------------------------------------------------
class OVT_SpawnCivilianJobStage : OVT_JobStage
{
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Unit Prefab", params: "et")]
	ResourceName m_pPrefab;

	[Attribute("1", UIWidgets.CheckBox, "Spawn as a civilian (CIV). Uncheck to affiliate with one of Overthrow's factions instead")]
	bool m_bCivilian;

	[Attribute("0", UIWidgets.ComboBox, "Faction to affiliate with when this is not a civilian", "", ParamEnumArray.FromEnum(OVT_FactionType))]
	OVT_FactionType m_Faction;

	override bool OnStart(OVT_Job job)
	{
		vector spawnPosition = job.location;

		//! Job civilians spawn in bulk, so skip the spawn-point sphere query and just find safe ground
		spawnPosition = OVT_WorldUtils.FindSafeSpawnPosition(spawnPosition, "-0.5 0 -0.5", "0.5 2 0.5", true);

		IEntity entity = OVT_Global.SpawnEntityPrefab(m_pPrefab, spawnPosition);
		if(!entity)
		{
			Print("[Overthrow] OVT_SpawnCivilianJobStage: could not spawn " + m_pPrefab, LogLevel.WARNING);
			return false;
		}

		//! BUG-118: same as OVT_SpawnFactionCharacterJobStage - the job replays this stage on
		//! every restore and nothing recalls the old target by persistence id, so a lone spawned
		//! character (which bypasses the SCR_AIGroup chokepoint) must be released from tracking or
		//! its record tree is a permanent orphan per restart.
		OVT_PersistenceManagerComponent.UntrackTransient(entity);

		//! Without an RplComponent there is no id to hand to OVT_WaitTillDeadJobStage, and the target
		//! would not exist on any client
		RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		if(rpl)
		{
			job.entity = rpl.Id();
		}
		else
		{
			Print("[Overthrow] OVT_SpawnCivilianJobStage: " + m_pPrefab + " has no RplComponent - the job cannot track it", LogLevel.WARNING);
		}

		FactionAffiliationComponent fac = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
		if(fac)
		{
			fac.SetAffiliatedFactionByKey(ResolveFactionKey());
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The faction key to affiliate the spawn with.
	//! \return "CIV" for a civilian target, otherwise the configured faction's own key.
	protected string ResolveFactionKey()
	{
		if(m_bCivilian) return "CIV";

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if(!config) return "CIV";

		OVT_Faction faction = config.GetFactionByType(m_Faction);
		if(!faction) return "CIV";

		return faction.GetFactionKey();
	}
}
