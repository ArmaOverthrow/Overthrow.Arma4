//------------------------------------------------------------------------------------------------
//! TIER B - the gate that stops a composition deployment being bought at a base that has nowhere to
//! put it, and the one thing about it that can silently rot.
//!
//! WHAT THIS EXISTS TO CATCH. OVT_CompositionSlotConditionDeploymentModule cannot read its own
//! config's composition modules - it is asked at CREATION time, against the config TEMPLATE, with no
//! deployment and no route to its siblings - so the slot types it gates on are AUTHORED separately
//! from the m_eSlotType each composition module authors. That duplication is deliberate and explained
//! at the attribute, but it is exactly the kind of pair that drifts: change a composition to
//! ROAD_MEDIUM, forget the condition, and the gate quietly starts testing a pool that has nothing to
//! do with what would be built. Nothing fails, nothing logs, and the deployment is either bought where
//! it cannot build or refused where it could.
//!
//! So this case asserts the two SETS match exactly, on every shipped config that carries both.
//!
//! ⚠ SET EQUALITY, NOT ORDER. The condition's list is a set of acceptable types and several
//! composition modules legitimately share one type (Base Fortifications authors SMALL three times), so
//! the assertion is "every type a composition module wants is accepted, and every type accepted is
//! wanted by some composition module" - not a positional comparison.
//!
//! NOT ASSERTED HERE: whether any base actually HAS such a slot. That is world authoring, it changes
//! every time a slot is placed in slots.layer, and it is what the base controller's own inventory log
//! reports at runtime. A test that pinned it would fail on every edit to the world.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Every config with composition modules also carries a slot gate, and the gate accepts exactly the
//! slot types its compositions need.
//!
//! ⚠ NOT YET PROVEN ABLE TO FAIL - the fault injection is OWED. The intended injection: change one
//! m_eSlotType in Deployment_BaseCheckpoints.conf (ROAD_LARGE -> LARGE) without touching
//! m_aAcceptedSlotTypes, and require this case to name the unaccepted type.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_CompositionSlotGate_AcceptedTypesMatchTheCompositions : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		if (!deployments)
		{
			SetFailure("OVT_Global.GetDeploymentManager() is null - the deployment framework did not resolve");
			return true;
		}

		if (!deployments.m_DeploymentRegistry || !deployments.m_DeploymentRegistry.m_aDeploymentConfigs)
		{
			SetFailure("the deployment registry did not resolve, so no config could be checked");
			return true;
		}

		int checkedConfigs = 0;

		foreach (OVT_DeploymentConfig config : deployments.m_DeploymentRegistry.m_aDeploymentConfigs)
		{
			if (!config || !config.m_aModules)
				continue;

			array<int> wanted = {};
			OVT_CompositionSlotConditionDeploymentModule gate;

			foreach (OVT_BaseDeploymentModule module : config.m_aModules)
			{
				OVT_CompositionSpawningDeploymentModule composition = OVT_CompositionSpawningDeploymentModule.Cast(module);
				if (composition)
				{
					if (!wanted.Contains(composition.m_eSlotType))
						wanted.Insert(composition.m_eSlotType);

					continue;
				}

				if (!gate)
					gate = OVT_CompositionSlotConditionDeploymentModule.Cast(module);
			}

			if (wanted.IsEmpty())
				continue;

			checkedConfigs++;

			if (!gate)
			{
				SetFailure(string.Format("config '%1' builds compositions but authors no OVT_CompositionSlotConditionDeploymentModule - it can be bought at a base with nowhere to put them, and the cost is paid before the slot is ever looked up", config.m_sDeploymentName));
				return true;
			}

			if (!gate.m_aAcceptedSlotTypes || gate.m_aAcceptedSlotTypes.IsEmpty())
			{
				SetFailure(string.Format("config '%1' authors a slot gate with an EMPTY m_aAcceptedSlotTypes, which the gate treats as 'allow everything' - so it gates nothing at all", config.m_sDeploymentName));
				return true;
			}

			foreach (int want : wanted)
			{
				if (!gate.m_aAcceptedSlotTypes.Contains(want))
				{
					SetFailure(string.Format("config '%1' builds a %2 composition but its slot gate does not accept that type - the gate is testing the wrong pool of slots", config.m_sDeploymentName, typename.EnumToString(OVT_EDeploymentSlotType, want)));
					return true;
				}
			}

			foreach (int accepted : gate.m_aAcceptedSlotTypes)
			{
				if (!wanted.Contains(accepted))
				{
					SetFailure(string.Format("config '%1' accepts %2 slots but builds no composition of that type - a free slot of it would let the deployment be bought when nothing it carries can use one", config.m_sDeploymentName, typename.EnumToString(OVT_EDeploymentSlotType, accepted)));
					return true;
				}
			}
		}

		// A pass that checked nothing is a pass that proves nothing: the shipped set carries two such
		// configs (Base Checkpoints and Base Fortifications), so zero means the walk found no
		// composition modules at all and the assertions above never ran.
		if (checkedConfigs == 0)
		{
			SetFailure("no registered config carries a composition module, so this case asserted nothing - the registry or the module type has changed");
			return true;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The free-slot scan itself: a slot absent from the claim list is free, and the two null cases answer
//! opposite ways on purpose.
//!
//! ⚠ WHY IT IS A SCAN AND NOT THE EXISTING ROLL. FindFreeSlot() picks by random roll, which is right
//! for CHOOSING a slot and wrong for ASKING whether one exists: a roll landing on a taken slot is
//! indistinguishable from there being none free, so a creation gate built on it would refuse buildable
//! deployments at random. This pins the distinction.
//!
//! ⚠ NOT YET PROVEN ABLE TO FAIL - the fault injection is OWED. The intended injection: make
//! HasFreeSlot return true for a null `filled`, and require the "no claim list is not no claims" row
//! to go red.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_CompositionSlotGate_FreeSlotScanIsExhaustive : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		array<ref EntityID> empty = {};
		array<ref EntityID> claims = {};

		if (OVT_CompositionSpawningDeploymentModule.HasFreeSlot(null, claims))
		{
			SetFailure("a null slot list must answer 'no free slot'");
			return true;
		}

		if (OVT_CompositionSpawningDeploymentModule.HasFreeSlot(empty, claims))
		{
			SetFailure("an empty slot list must answer 'no free slot' - a base with no slots of a kind has none free");
			return true;
		}

		// ⚠ THE ASYMMETRY: no slot list means no slots, but no CLAIM list means the bookkeeping is
		// missing, and answering "everything is free" off a missing ledger would hand out slots that may
		// already be built on.
		// Any real EntityID will do - the scan only ever compares ids for membership, never dereferences
		// them, so the game mode's own id is a valid stand-in for a slot entity and needs no fixture.
		IEntity idSource = GetGame().GetGameMode();
		if (!idSource)
		{
			SetFailure("no game mode entity to borrow an EntityID from");
			return true;
		}

		array<ref EntityID> oneSlot = {};
		oneSlot.Insert(idSource.GetID());

		if (OVT_CompositionSpawningDeploymentModule.HasFreeSlot(oneSlot, null))
		{
			SetFailure("a null claim list must answer 'no free slot' - 'no claim list' is not the same as 'nothing is claimed'");
			return true;
		}

		if (!OVT_CompositionSpawningDeploymentModule.HasFreeSlot(oneSlot, claims))
		{
			SetFailure("a slot absent from an empty claim list must read as free");
			return true;
		}

		claims.Insert(oneSlot[0]);

		if (OVT_CompositionSpawningDeploymentModule.HasFreeSlot(oneSlot, claims))
		{
			SetFailure("the only slot being claimed must read as no free slot");
			return true;
		}

		return true;
	}
}
