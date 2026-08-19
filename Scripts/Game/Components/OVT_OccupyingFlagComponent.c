//! Flies the OCCUPYING FACTION'S flag on whatever flagpole this component is attached to.
//!
//! WHY THIS EXISTS: every vanilla flagpole descends from FlagPole_02_base, whose SCR_FlagComponent
//! default material and whose Flag slot are BOTH the United States - so a hand-authored prefab that
//! inherits a pole and says nothing about flags flies a US flag no matter who is occupying the map.
//! That is what shipped on Prefabs/Bases/OVT_OccupyingFOB.et and what this component fixes.
//!
//! ⚠ THE OCCUPIER IS A CAMPAIGN SETTING, NOT A CONSTANT. Overthrow campaigns can be started against
//! either the USSR or the US (Overthrow_Config.json on a dedicated server, the start-game menu
//! otherwise), so the correct flag CANNOT be baked into a prefab. Baking USSR in would be exactly as
//! wrong as the US flag it replaced, just wrong for the other half of the campaigns. The material is
//! resolved from OVT_OverthrowConfigComponent at runtime instead.
//!
//! RUNS ON EVERY MACHINE, REPLICATES NOTHING. A flag material is a local visual: each client derives
//! it from the occupying-faction key it already holds. This is the same arrangement
//! OVT_TownControllerComponent.CheckUpdateFlag() uses, and the reason its 10-second re-check is
//! copied here rather than a one-shot latch: a joining client receives the campaign's occupying
//! faction through OVT_OccupyingFactionManager's JIP bitstream, and until that arrives the config
//! still holds its born-with default ("USSR"). A latch would freeze whatever it read first - which on
//! a US-occupied campaign is the wrong flag, permanently. Re-checking makes the wrong read
//! self-correcting, and the check is a single integer compare once it has settled.
//!
//! ⚠ THE SLOTTED FLAG ENTITY IS DELIBERATELY LEFT ALONE. OVT_BaseController.et and
//! OVT_TownController.et each override SlotManagerComponent {55DAE04E55ECE7FA} as well, because they
//! inherit the FIA pole whose slot holds a 2:3 flag - the wrong SHAPE, which no material swap can
//! fix. A plain FlagPole_02_V1 already slots a 1:2 flag, the same shape as both faction flags, so
//! ChangeMaterial() alone is sufficient and nothing faction-specific is baked into the prefab.
[ComponentEditorProps(category: "Overthrow", description: "Flies the occupying faction's flag on this flagpole")]
class OVT_OccupyingFlagComponentClass : OVT_ComponentClass
{
}

class OVT_OccupyingFlagComponent : OVT_Component
{
	//! Faction index whose flag is currently on the pole. -1 means "nothing applied yet", which is
	//! also what makes the first successful check do work.
	protected int m_iFlagFactionIndex = -1;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (SCR_Global.IsEditMode())
			return;

		GetGame().GetCallqueue().CallLater(CheckUpdateFlag, 10000, true);
		GetGame().GetCallqueue().CallLater(CheckUpdateFlag, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves the occupying faction's flag material and puts it on the pole, if it is not there
	//! already. Every failure here is a "cannot tell yet" - the repeating timer asks again.
	protected void CheckUpdateFlag()
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return;

		FactionManager factionManager = GetGame().GetFactionManager();
		if (!factionManager)
			return;

		Faction faction = config.GetOccupyingFactionData();
		if (!faction)
			return;

		int factionIndex = factionManager.GetFactionIndex(faction);
		if (factionIndex == m_iFlagFactionIndex)
			return;

		SCR_Faction scrFaction = SCR_Faction.Cast(faction);
		if (!scrFaction)
			return;

		SCR_FlagComponent flag = OVT_ComponentFinder<SCR_FlagComponent>.Find(GetOwner());
		if (!flag)
			return;

		ResourceName material = scrFaction.GetFactionFlagMaterial();
		if (material.IsEmpty())
			return;

		flag.ChangeMaterial(material);
		m_iFlagFactionIndex = factionIndex;
	}

	//------------------------------------------------------------------------------------------------
	//! A forward base is torn down mid-session, so the timer MUST come off the queue with the entity -
	//! the call queue holds the method on this instance and would keep firing it after deletion.
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(CheckUpdateFlag);

		super.OnDelete(owner);
	}
}
