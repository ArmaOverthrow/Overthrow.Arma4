//------------------------------------------------------------------------------------------------
//! TIER B - the medical tent's dressing and its heal beds are actually in the prefab.
//!
//! WHY THIS FILE EXISTS. Everything this case asserts is authored in
//! Prefabs/Structures/Military/FOB/OVT_MedicalTent.et by hand: eleven vanilla prefab GUIDs for the
//! furniture, and three ActionsManagerComponent blocks that put OVT_HealAction on the cots. A wrong
//! GUID does not fail a compile and does not stop the tent spawning - the child is simply MISSING,
//! silently, and the tent comes back looking like the empty shell this change replaced. A dropped
//! action block is worse: the tent still looks right and cannot heal anybody.
//!
//! TWO CLAIMS:
//!   1. exactly three children carry an OVT_HealAction - the cots, and only the cots;
//!   2. the ROOT carries none. The heal moved off the tent body deliberately: the old one sat at head
//!      height in the middle of the tent with no bed under it.
//! The child COUNT is asserted too, but only as a "a child block was deleted" guard.
//!
//! ⚠ A WRONG FURNITURE PREFAB REFERENCE IS NOT DETECTABLE HERE, and three attempts to make it so all
//! failed on the same wall: a child whose prefab does not resolve is still CREATED (empty), so the
//! count is unchanged; and Resource.Load() + FindEntitySource() answer with a usable source for a
//! corrupted GUID AND for a path that names no file at all. Both were fail-probed and both stayed
//! green. Only Workbench, or an eye on the tent, will catch that.
//!
//! It says nothing about whether the heal WORKS - that is server-side in
//! OVT_ResistanceRequestComponent.RpcAsk_HealPlayer and needs a wounded player.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_MedicalTent_BedsCarryTheHealAction : SCR_AutotestCaseBase
{
	static const ResourceName MEDICAL_TENT = "{F634370733F16BC0}Prefabs/Structures/Military/FOB/OVT_MedicalTent.et";

	//! Every child a spawned tent must have: THIRTEEN authored here plus TWO from the vanilla medical
	//! base (its support-station area mesh and its floor). This catches a DELETED child block, not a
	//! bad GUID - see claim A.
	static const int EXPECTED_CHILDREN = 15;


	//! Cots, and therefore heal actions.
	static const int EXPECTED_BEDS = 3;

	protected IEntity m_Tent;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		vector position;
		if (!OVT_TEST_StorageSeamSubject.ResolveSpawnPosition("780 0 660", position))
		{
			SetFailure("No town is registered, so there is nowhere sensible to put a test tent");
			return true;
		}

		m_Tent = OVT_Global.SpawnEntityPrefab(MEDICAL_TENT, position);
		if (!m_Tent)
		{
			SetFailure("SpawnEntityPrefab() produced no entity from %1 - the medical tent's own GUID is wrong, which would also break every FOB that builds one.", MEDICAL_TENT);
			return true;
		}

		int children = 0;
		int beds = 0;

		IEntity child = m_Tent.GetChildren();
		while (child)
		{
			children += 1;
			if (HasHealAction(child))
				beds += 1;

			child = child.GetSibling();
		}

		if (children < EXPECTED_CHILDREN)
		{
			SetFailure("The medical tent spawned with %1 children, expected at least %2. A child block has been dropped from OVT_MedicalTent.et.",
				children.ToString(), EXPECTED_CHILDREN.ToString());
			return FinishAndCleanUp();
		}


		if (beds != EXPECTED_BEDS)
		{
			SetFailure("%1 of the tent's children carry an OVT_HealAction, expected %2. The cots' ActionsManagerComponent blocks are not landing, so the tent is dressed but cannot treat anybody.",
				beds.ToString(), EXPECTED_BEDS.ToString());
			return FinishAndCleanUp();
		}

		if (HasHealAction(m_Tent))
		{
			SetFailure("The tent ROOT still carries an OVT_HealAction. It was moved onto the cots on purpose - the root one sat at head height in the middle of the tent with nothing under it.");
			return FinishAndCleanUp();
		}

		Print("Medical tent: " + children.ToString() + " children, " + beds.ToString() + " heal beds");
		return FinishAndCleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] entity The entity to inspect.
	//! \return True when it hosts at least one OVT_HealAction.
	protected bool HasHealAction(IEntity entity)
	{
		if (!entity)
			return false;

		BaseActionsManagerComponent actions = BaseActionsManagerComponent.Cast(entity.FindComponent(BaseActionsManagerComponent));
		if (!actions)
			return false;

		array<BaseUserAction> list = new array<BaseUserAction>();
		actions.GetActionsList(list);

		foreach (BaseUserAction action : list)
		{
			if (OVT_HealAction.Cast(action))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Always true - the case is over either way.
	protected bool FinishAndCleanUp()
	{
		if (m_Tent)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(m_Tent);
			m_Tent = null;
		}

		return true;
	}
}
