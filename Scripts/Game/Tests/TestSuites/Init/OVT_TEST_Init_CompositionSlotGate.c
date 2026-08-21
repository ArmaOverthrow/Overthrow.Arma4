//------------------------------------------------------------------------------------------------
//! TIER B - THE RULE THAT A DEPLOYMENT IS NEVER CHARGED FOR A COMPOSITION IT CANNOT PLACE.
//!
//! ==========================================================================================
//! 🔴 WHAT THIS CASE USED TO ASSERT, AND WHY THAT RULE IS GONE (2026-08-22).
//! ==========================================================================================
//! It used to require that every config building compositions ALSO authored an
//! OVT_CompositionSlotConditionDeploymentModule, on the grounds that "the cost is paid before the slot
//! is ever looked up". That was true, and the author refused the config-level answer to it:
//!
//!     "They shouldn't have to pay for unplaceable positions, some bases simply don't have the slots
//!      or anywhere to put them."
//!
//! So the FRAMEWORK was fixed instead of the configs. A deployment is now priced for the position it is
//! about to be built at (OVT_DeploymentConfig.GetTotalResourceCost(mult, position) ->
//! OVT_BaseDeploymentModule.GetResourceCostAt), and a config whose compositions are ALL unplaceable
//! there is not offered at that place at all (CanPlaceCompositionsAt). Authoring a slot-condition
//! module is now one way to express a preference, not a requirement - so a case demanding one was
//! pinning a config decision, which is the same mistake as pinning m_iPriority.
//!
//! ==========================================================================================
//! WHAT IS ASSERTED INSTEAD - THREE INVARIANTS, NONE OF THEM A TUNING VALUE.
//! ==========================================================================================
//!   NEVER DEARER        pricing a module FOR A PLACE may only ever reduce its cost. If a position
//!                       could make something cost MORE, the manager would debit a figure it never
//!                       checked against the pool, and nothing would say so.
//!   ALL OR NOTHING      the difference between a module's template price and its priced-for-here price
//!                       is either zero or EXACTLY the composition's own m_iCost. That is the author's
//!                       rule stated as arithmetic: you are charged for the structure or you are not,
//!                       never a fraction of it, and never for something else.
//!   ONLY COMPOSITIONS   a module that builds no composition prices identically everywhere. Infantry
//!                       and vehicles are delivered wherever they are sent; if their price ever became
//!                       position-dependent, the "never dearer" guarantee would be silently load-bearing
//!                       somewhere nobody had looked.
//!
//! ⚠ EVERY ONE OF THESE IS SILENT WHEN BROKEN, which is what earns them a test. A wrong charge does not
//! error, does not log, and looks exactly like a correct one - that is the entire reason the leak
//! survived to a play-test.
//!
//! ⚠ IT PRICES AGAINST THE REAL BASES, not a fixture, because the whole question is what the world's
//! slots say - so it also exercises HasFreeSlotAt() against real controllers.
//!
//! ⚠ NOT YET PROVEN ABLE TO FAIL - the fault injection is OWED. The intended injection: drop the
//! HasFreeSlotAt() test from OVT_CompositionSpawningDeploymentModule.GetResourceCostAt so it always
//! adds the composition price, and require the ALL-OR-NOTHING row to stay silent while... it would not:
//! the honest injection is to make GetResourceCostAt add HALF the composition price, and require
//! "the charge for a structure must be all or nothing" to go red.
//!
//! NOT ASSERTED HERE: whether any particular base has any particular slot. That is world authoring, it
//! changes every time slots.layer is edited, and pinning it would fail on every edit to the map.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Pricing a deployment for a place may only ever make it cheaper, and only ever by whole compositions.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_CompositionSlotGate_NothingIsChargedForAnUnplaceableComposition : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		if (!deployments || !deployments.m_DeploymentRegistry || !deployments.m_DeploymentRegistry.m_aDeploymentConfigs)
		{
			SetFailure("the deployment registry did not resolve, so no config could be priced");
			return true;
		}

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying || !occupying.m_Bases || occupying.m_Bases.IsEmpty())
		{
			SetFailure("no bases resolved, so there is nowhere to price a composition against");
			return true;
		}

		int checkedModules = 0;

		foreach (OVT_DeploymentConfig config : deployments.m_DeploymentRegistry.m_aDeploymentConfigs)
		{
			if (!config || !config.m_aModules)
				continue;

			foreach (OVT_BaseData base : occupying.m_Bases)
			{
				if (!base)
					continue;

				foreach (OVT_BaseDeploymentModule module : config.m_aModules)
				{
					if (!module)
						continue;

					int template = module.GetResourceCost();
					int here = module.GetResourceCostAt(base.location);

					// --- NEVER DEARER.
					if (here > template)
					{
						SetFailure(string.Format("config '%1': a module prices %2 at a base and %3 from the template - pricing for a PLACE may only ever reduce a cost, never raise one, or the manager debits a figure it never checked against the pool",
							config.m_sDeploymentName, here.ToString(), template.ToString()));
						return true;
					}

					OVT_CompositionSpawningDeploymentModule composition = OVT_CompositionSpawningDeploymentModule.Cast(module);
					if (!composition)
					{
						// --- ONLY COMPOSITIONS are position-dependent.
						if (here != template)
						{
							SetFailure(string.Format("config '%1': a NON-composition module prices differently at a base (%2) than from the template (%3) - only a composition's delivery can be refused by the ground",
								config.m_sDeploymentName, here.ToString(), template.ToString()));
							return true;
						}

						continue;
					}

					checkedModules++;

					// --- ALL OR NOTHING.
					int dropped = template - here;
					if (dropped == 0)
						continue;

					int price = composition.GetCompositionCost();

					if (dropped != price)
					{
						SetFailure(string.Format("config '%1': pricing at a base dropped %2 from a composition module whose composition costs %3 - the charge for a structure must be all or nothing",
							config.m_sDeploymentName, dropped.ToString(), price.ToString()));
						return true;
					}
				}
			}
		}

		// A pass that priced no composition module proves nothing: the shipped set carries them (Base
		// Checkpoints, Base Fortifications), so zero means the registry or the module type has changed
		// under this case.
		if (checkedModules == 0)
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
