//------------------------------------------------------------------------------------------------
//! TIER B - the destruction facade, OVT_StructureDamage, where it meets real entities.
//!
//! WHAT IS AT STAKE. Ruin() is called from sabotage with a fallback behind it: when it answers false
//! the caller DELETES the structure instead. So the two ways this facade can be wrong are opposite
//! and both silent - answering false for a retrofitted structure turns every sabotage back into the
//! permanent deletion this feature exists to remove, and answering true for something it cannot
//! actually drive leaves a mission believing it demolished a building that is still standing.
//!
//! ⚠ NOTHING HERE DRIVES A DEPLOYMENT OR TOUCHES A PLAYER STRUCTURE. Both cases spawn their own
//! subjects far from anything, assert against them, and delete them again.
//!
//! Cases run alphabetically by class name and neither writes shared state, so the order is free; the
//! A/B prefixes are for the run log. No polling, no waiting, no maxAttempts: a phase change is
//! synchronous (only the MESH change is deferred, and nothing here asserts on the mesh).
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! The facade is safe on everything that is not a retrofitted structure: null, and an ordinary prop.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time, restored and re-compiled clean afterwards):
//!   A1. `Resolve()`'s `if (!entity) return null;` deleted. The null calls hard-error instead of
//!       answering false, and the case never reports.
//!   A2. `IsDestructible()` changed to `return true;`. Fails on "a plain prop reports destructible".
//!   A3. `Ruin()`'s no-component branch changed to `return true;`. Fails on "Ruin() claimed it ruined
//!       a prop" - which is exactly the state that would make sabotage skip its delete fallback.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_StructureDamage_AGuardsAreSafeOnNonStructures : SCR_AutotestCaseBase
{
	//! An ordinary prop with no destruction component of ours - the "plain prop" of the acceptance
	//! criteria. Any world-placeable prefab would do; this one is small, static and already used as a
	//! test subject elsewhere in this suite.
	static const ResourceName PLAIN_PROP_PREFAB = "{7FCD4E7C25D886A8}Prefabs/Structures/Signs/Traffic/SignBusStop_01.et";

	protected IEntity m_Prop;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// Null: every entry point must answer, not error.
		if (OVT_StructureDamage.Resolve(null))
		{
			SetFailure("OVT_StructureDamage.Resolve(null) returned a component");
			return true;
		}

		if (OVT_StructureDamage.Ruin(null) || OVT_StructureDamage.Repair(null) || OVT_StructureDamage.IsRuined(null) || OVT_StructureDamage.IsDestructible(null))
		{
			SetFailure("OVT_StructureDamage answered true for a null entity. Sabotage calls Ruin() on whatever a world query handed it, so a null-unsafe facade is a mission that stops mid-run.");
			return true;
		}

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns || towns.m_Towns.Count() < 1)
		{
			SetFailure("No towns are registered - nowhere sensible to spawn the subject prop");
			return true;
		}

		m_Prop = OVT_Global.SpawnEntityPrefab(PLAIN_PROP_PREFAB, towns.m_Towns[0].location + Vector(600, 0, 600));
		if (!m_Prop)
		{
			SetFailure("SpawnEntityPrefab() produced no entity from %1", PLAIN_PROP_PREFAB);
			return true;
		}

		string failure = CheckProp(m_Prop);
		if (failure != "")
		{
			SetFailure(failure);
			return FinishAndCleanUp();
		}

		Print("OVT_StructureDamage: null and a non-retrofitted prop are refused cleanly, so sabotage keeps its delete fallback for both");

		return FinishAndCleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] prop The spawned subject.
	//! \return An empty string when every claim held, or the first that did not.
	protected string CheckProp(notnull IEntity prop)
	{
		if (OVT_StructureDamage.IsDestructible(prop))
			return "A plain prop reports destructible - OVT_StructureDamage.Resolve() is finding a component that is not there, or is matching too broadly";

		if (OVT_StructureDamage.IsRuined(prop))
			return "A plain prop reports ruined";

		if (OVT_StructureDamage.Ruin(prop))
			return "Ruin() claimed it ruined a prop that carries no destruction component. Sabotage reads that as 'handled' and skips DestroyPlacedItem(), so the structure would never come down at all";

		if (OVT_StructureDamage.Repair(prop))
			return "Repair() claimed it repaired a prop that carries no destruction component";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \return Always true - the case is over either way.
	protected bool FinishAndCleanUp()
	{
		if (m_Prop)
		{
			delete m_Prop;
			m_Prop = null;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A real retrofitted structure goes intact -> ruined -> intact, is found through a parent, and is
//! the SAME entity throughout.
//!
//! WHY THE IDENTITY HALF MATTERS AS MUCH AS THE PHASE HALF. The entire persistence answer for this
//! feature is "the entity is never deleted": its save record, its RplId, its OVT_BuildableComponent
//! ownership and its inventory all survive because nothing is re-created. A phase change that
//! silently replaced the entity would still look correct here on the phase alone, and would take
//! ownership and gear with it. Both are asserted.
//!
//! THE PARENT WALK is the vehicle-maintenance-ramp shape (Phase 4): the buildable root is bare and
//! the destructible object is a CHILD. It is arranged here with AddChild() rather than by waiting for
//! that prefab, so Resolve()'s second half is covered before the retrofit lands on it.
//!
//! ⚠ NOTHING ASSERTS ON THE MESH. ChangeModel is scheduled through the call queue and lands a frame
//! or more later; only the phase, which GoToDamagePhase sets synchronously, is a safe subject here.
//! Whether the ruin mesh and its collision are right is user-gated play-test 1.U2 / 4.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time, restored and re-compiled clean afterwards):
//!   B1. `RuinIt()`'s `GoToDamagePhase(PHASE_RUINED, false)` commented out. Fails on "was still intact
//!       after Ruin()".
//!   B2. `GoToDamagePhase`'s phase-0 route into `RepairToIntact()` removed (so super's early return at
//!       :227 wins). Fails on "was still ruined after Repair()" - the exact vanilla hole this
//!       component exists to close.
//!   B3. `Resolve()`'s child walk deleted. Fails on "not found through its parent", i.e. the ramp
//!       would be invisible to every consumer.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_StructureDamage_BPhaseRoundTripOnAProbe : SCR_AutotestCaseBase
{
	//! The Guard Tower probe retrofitted in Phase 1 - one of the two prefabs that carries
	//! OVT_StructureDestructionComponent today.
	static const ResourceName PROBE_PREFAB = "{806E831E70E193F1}Prefabs/Structures/Military/Houses/GuardTower_01/OVT_GuardTower_01.et";

	//! A component-less prop used only as a parent, to exercise Resolve()'s child walk.
	static const ResourceName PARENT_PREFAB = "{7FCD4E7C25D886A8}Prefabs/Structures/Signs/Traffic/SignBusStop_01.et";

	//! Written onto the subject's buildable component before the round trip and re-read after it.
	static const string OWNER_MARKER = "OVT_TEST_Init_StructureDamage_Owner";

	protected IEntity m_Structure;
	protected IEntity m_Parent;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns || towns.m_Towns.Count() < 1)
		{
			SetFailure("No towns are registered - nowhere sensible to spawn the subject structure");
			return true;
		}

		vector origin = towns.m_Towns[0].location + Vector(700, 0, 700);

		m_Structure = OVT_Global.SpawnEntityPrefab(PROBE_PREFAB, origin);
		if (!m_Structure)
		{
			SetFailure("SpawnEntityPrefab() produced no entity from %1", PROBE_PREFAB);
			return true;
		}

		string failure = CheckRoundTrip(m_Structure);
		if (failure != "")
		{
			SetFailure(failure);
			return FinishAndCleanUp();
		}

		failure = CheckParentWalk(m_Structure, origin + Vector(0, 0, 20));
		if (failure != "")
		{
			SetFailure(failure);
			return FinishAndCleanUp();
		}

		Print("OVT_StructureDamage: a retrofitted structure ruins and repairs, is the same entity throughout, and is reachable through a parent");

		return FinishAndCleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! Intact -> ruined -> intact, with the entity's identity checked at every step.
	//! \param[in] structure The spawned subject.
	//! \return An empty string when every claim held, or the first that did not.
	protected string CheckRoundTrip(notnull IEntity structure)
	{
		if (!OVT_StructureDamage.IsDestructible(structure))
			return "The Guard Tower prefab reports NOT destructible. Its OVT_StructureDestructionComponent block is gone or disabled, so sabotage would delete it permanently again";

		if (OVT_StructureDamage.IsRuined(structure))
			return "A freshly spawned Guard Tower already reports ruined - it would be built as wreckage";

		OVT_BuildableComponent buildable = OVT_BuildableComponent.Cast(structure.FindComponent(OVT_BuildableComponent));
		if (!buildable)
			return "The Guard Tower carries no OVT_BuildableComponent - it is not a buildable at all any more";

		buildable.SetOwnerPersistentId(OWNER_MARKER);

		RplId beforeId = ResolveRplId(structure);

		if (!OVT_StructureDamage.Ruin(structure, false))
			return "Ruin() refused a retrofitted Guard Tower. Sabotage would fall back to DestroyPlacedItem() and delete it, which is the behaviour this feature removes";

		if (!OVT_StructureDamage.IsRuined(structure))
			return "The structure was still intact after Ruin() - the phase change never reached the component";

		string identityFailure = CheckIdentity(structure, buildable, beforeId, "ruin");
		if (identityFailure != "")
			return identityFailure;

		if (!OVT_StructureDamage.Repair(structure))
			return "Repair() refused a ruined Guard Tower";

		if (OVT_StructureDamage.IsRuined(structure))
			return "The structure was still ruined after Repair() - GoToDamagePhase(0) is not being routed into RepairToIntact(), so nothing in the mod can ever undo a ruin";

		return CheckIdentity(structure, buildable, beforeId, "repair");
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] structure The subject.
	//! \param[in] buildable The buildable component read before the round trip.
	//! \param[in] beforeId The replication id read before the round trip.
	//! \param[in] step Which half of the round trip is being checked, for the message.
	//! \return An empty string when the entity is demonstrably the same one.
	protected string CheckIdentity(notnull IEntity structure, notnull OVT_BuildableComponent buildable, RplId beforeId, string step)
	{
		if (structure.IsDeleted())
			return string.Format("The structure was DELETED by the %1. Every persistence guarantee in this feature rests on the entity surviving a phase change", step);

		OVT_BuildableComponent after = OVT_BuildableComponent.Cast(structure.FindComponent(OVT_BuildableComponent));
		if (after != buildable)
			return string.Format("The structure's OVT_BuildableComponent is a different instance after the %1 - the entity was re-created rather than re-phased", step);

		if (after.GetOwnerPersistentId() != OWNER_MARKER)
			return string.Format("The structure's owner was lost by the %1 ('%2'). A ruin would stop belonging to the player who built it", step, after.GetOwnerPersistentId());

		if (ResolveRplId(structure) != beforeId)
			return string.Format("The structure's RplId changed across the %1. Every client and every open reference to it would be pointing at nothing", step);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Resolve() must find the component on a CHILD too - the ramp shape.
	//! \param[in] structure The subject, which becomes the child.
	//! \param[in] parentOrigin Where the bare parent goes.
	//! \return An empty string when the walk found it.
	protected string CheckParentWalk(notnull IEntity structure, vector parentOrigin)
	{
		m_Parent = OVT_Global.SpawnEntityPrefab(PARENT_PREFAB, parentOrigin);
		if (!m_Parent)
			return string.Format("SpawnEntityPrefab() produced no parent entity from %1", PARENT_PREFAB);

		if (OVT_StructureDamage.IsDestructible(m_Parent))
			return "The bare parent prop reports destructible before anything was parented to it - the negative control is not a control";

		m_Parent.AddChild(structure, -1, EAddChildFlags.RECALC_LOCAL_TRANSFORM);

		OVT_StructureDestructionComponent throughParent = OVT_StructureDamage.Resolve(m_Parent);
		if (!throughParent)
			return "A destruction component on a CHILD was not found through its parent. The vehicle maintenance ramp is exactly this shape - a bare buildable root with the destructible object below it - so it would be un-ruinable and un-repairable";

		if (throughParent != OVT_StructureDamage.Resolve(structure))
			return "Resolving through the parent found a different component from resolving on the child itself";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] entity The subject.
	//! \return Its replication id, or an invalid one when it carries no RplComponent.
	protected RplId ResolveRplId(notnull IEntity entity)
	{
		RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		if (!rpl)
			return RplId.Invalid();

		return rpl.Id();
	}

	//------------------------------------------------------------------------------------------------
	//! \return Always true - the case is over either way.
	protected bool FinishAndCleanUp()
	{
		if (m_Parent && m_Structure)
			m_Parent.RemoveChild(m_Structure);

		if (m_Structure)
		{
			delete m_Structure;
			m_Structure = null;
		}

		if (m_Parent)
		{
			delete m_Parent;
			m_Parent = null;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Every buildable prefab spawns destructible and survives a ruin then repair round trip without
//! deleting itself.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 120)]
class OVT_TEST_Init_StructureDamage_CEveryBuildableIsRetrofitted : SCR_AutotestCaseBase
{
	//! Far enough from town centre to be clear of anything, and stepped so two subjects never share a
	//! spot even though each is deleted before the next spawns.
	static const int SUBJECT_STEP_M = 25;

	protected IEntity m_Subject;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (!resistance || !resistance.m_BuildablesConfig || !resistance.m_BuildablesConfig.m_aBuildables)
		{
			SetFailure("The buildables config is not loaded, so there is no list of structures to check");
			return true;
		}

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns || towns.m_Towns.Count() < 1)
		{
			SetFailure("No towns are registered - nowhere sensible to spawn the subjects");
			return true;
		}

		array<ref OVT_Buildable> buildables = resistance.m_BuildablesConfig.m_aBuildables;
		vector anchor = towns.m_Towns[0].location + Vector(600, 0, 600);
		int index = 0;

		foreach (OVT_Buildable buildable : buildables)
		{
			if (!buildable || !buildable.m_aPrefabs)
			{
				SetFailure("A buildables config entry carries no prefab list");
				return CleanUp();
			}

			foreach (ResourceName prefab : buildable.m_aPrefabs)
			{
				string failure = CheckOne(buildable.m_sName, prefab, anchor + Vector(0, 0, index * SUBJECT_STEP_M));
				index++;

				if (failure != "")
				{
					SetFailure(failure);
					return CleanUp();
				}
			}
		}

		Print(string.Format("OVT_StructureDamage: all %1 buildable prefabs spawn destructible and survive a ruin -> repair round trip", index.ToString()));

		return CleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns one prefab, checks it, and removes it again whatever the answer was.
	//! \param[in] name The config entry's name, for the failure message.
	//! \param[in] prefab The prefab to spawn.
	//! \param[in] origin Where to put it.
	//! \return An empty string when every claim held, or the first that did not.
	protected string CheckOne(string name, ResourceName prefab, vector origin)
	{
		if (prefab == ResourceName.Empty)
			return string.Format("The buildable '%1' lists an empty prefab", name);

		m_Subject = OVT_Global.SpawnEntityPrefab(prefab, origin);
		if (!m_Subject)
			return string.Format("SpawnEntityPrefab() produced no entity for '%1' from %2", name, prefab);

		string failure = CheckRoundTrip(name, m_Subject);

		if (m_Subject && !m_Subject.IsDeleted())
			delete m_Subject;

		m_Subject = null;

		return failure;
	}

	//------------------------------------------------------------------------------------------------
	//! Intact -> ruined -> intact on one real structure, plus the phase-0 gate every ruin-inert surface
	//! shares.
	//! \param[in] name The config entry's name, for the failure message.
	//! \param[in] structure The spawned subject.
	//! \return An empty string when every claim held, or the first that did not.
	protected string CheckRoundTrip(string name, notnull IEntity structure)
	{
		if (!OVT_StructureDamage.IsDestructible(structure))
			return string.Format("'%1' carries no reachable OVT_StructureDestructionComponent. Either the retrofit never reached its prefab, or the engine DROPPED the component on load - which is what an authored damage manager with no hit zone does. Sabotage would delete this structure permanently", name);

		if (OVT_StructureDamage.IsRuined(structure))
			return string.Format("'%1' spawns already ruined - it would be built as wreckage", name);

		if (!OVT_StructureDamage.IsUsable(structure))
			return string.Format("'%1' reports unusable while intact, so its actions, shop and parking would be hidden on a working structure", name);

		if (!OVT_StructureDamage.Ruin(structure, false))
			return string.Format("Ruin() refused '%1'. Sabotage would fall back to deleting it", name);

		if (structure.IsDeleted())
			return string.Format("'%1' DELETED ITSELF when ruined. m_bDeleteAfterFinalPhase defaults to 1 and must be authored 0 - this is the exact behaviour the feature exists to remove", name);

		if (!OVT_StructureDamage.IsRuined(structure))
			return string.Format("'%1' was still intact after Ruin() - no damage phase is authored on it, so the component refused rather than driving past the final phase", name);

		if (OVT_StructureDamage.IsUsable(structure))
			return string.Format("'%1' still reports usable while ruined - every phase-0 gate reads this, so a wreck would keep recruiting, refuelling and selling", name);

		if (!OVT_StructureDamage.Repair(structure))
			return string.Format("Repair() refused the ruined '%1'", name);

		if (OVT_StructureDamage.IsRuined(structure))
			return string.Format("'%1' was still ruined after Repair()", name);

		if (!OVT_StructureDamage.IsUsable(structure))
			return string.Format("'%1' still reports unusable after Repair()", name);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \return Always true - the case is over either way.
	protected bool CleanUp()
	{
		if (m_Subject)
		{
			if (!m_Subject.IsDeleted())
				delete m_Subject;

			m_Subject = null;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! GEAR SURVIVES A PHASE ROUND TRIP, and a ruin's storage is closed while it lasts (D15, plan §3.8).
//!
//! WHY THE SUBJECT IS ASSEMBLED HERE. No shipped buildable is a container - every ammo box, cabinet
//! and crate is a PLACEABLE, and the eight buildable prefabs carry no storage component anywhere in
//! their chains (re-verified in Phase 6 by walking all eight to their vanilla roots). So the
//! container-carrying buildable this case needs is built at runtime out of two real prefabs: the
//! retrofitted Guard Tower, and an Overthrow ammo box parented to it. That is exactly the shape
//! core/storage will produce when a buildable gains a storage - the phase lives on the root, the
//! storage on something below it - and it is what the gate's root-parent walk exists for.
//!
//! WHAT IS AT STAKE. Two separate claims, and they pull in opposite directions:
//!   the CONTENTS must survive (nothing in a phase change touches an inventory, and repairing is
//!   what gives the gear back), while the ACCESS must not (reaching into rubble for a rifle reads
//!   wrong, and hiding the action is what makes repair worth its price). A regression that deleted
//!   the entity on ruin would break the first; a gate that never closed would break the second.
//!
//! ⚠ THE IDENTITY INVARIANT IS NOT REPEATED HERE. Case B already asserts that the STRUCTURE is the
//! same entity, with the same RplId and the same OVT_BuildableComponent, across both halves of the
//! round trip. This case asserts the same thing about the CONTAINER and its contents instead, which
//! is the half case B cannot reach.
//!
//! PROVEN ABLE TO FAIL (each fault traced to the assertion that trips on it; the injections are
//! read from the code path rather than run here, since the suites are the orchestrator's to run):
//!   D1. `OVT_StructureDamage.IsUsable()` changed to `return true;`. Fails on "the storage was still
//!       usable while the structure was a ruin" - every storage action would stay on the wreck.
//!   D2. `IsUsable()`'s `GetRootParent()` replaced with the entity itself. The box is not the entity
//!       that carries the phase, so the gate never closes and D1's assertion trips again.
//!   D3. `RuinIt()` changed to delete the owner instead of driving the phase (the pre-BD5 behaviour
//!       this feature removes). Fails on "the container was deleted by the ruin", and the contents
//!       assertion behind it would fail too.
//!   D4. The storage actions' `CanBeShownScript` gate removed from OVT_OpenStorageAction. Fails on
//!       "OVT_OpenStorageAction still offered itself on a ruin".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_StructureDamage_DGearSurvivesAPhaseRoundTrip : SCR_AutotestCaseBase
{
	//! The retrofitted Guard Tower - a real buildable, and the same probe case B uses.
	static const ResourceName STRUCTURE_PREFAB = "{806E831E70E193F1}Prefabs/Structures/Military/Houses/GuardTower_01/OVT_GuardTower_01.et";

	//! The Overthrow placed ammo box: a real container carrying the three gated storage actions.
	static const ResourceName CONTAINER_PREFAB = "{0AAFD134C3BEE963}Prefabs/Props/Military/AmmoBoxes/OVT_AmmoBox_Placed.et";

	//! The gear. A small item referenced by every shipped difficulty preset, so its resource is known
	//! good and it fits an ammo box without a size argument.
	static const ResourceName ITEM_PREFAB = "{E1A5D4B878AA8980}Prefabs/Items/Equipment/Radios/Radio_R148.et";

	protected IEntity m_Structure;
	protected IEntity m_Container;
	protected IEntity m_Item;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns || towns.m_Towns.Count() < 1)
		{
			SetFailure("No towns are registered - nowhere sensible to spawn the subject structure");
			return true;
		}

		vector origin = towns.m_Towns[0].location + Vector(800, 0, 800);

		string failure = BuildSubject(origin);
		if (failure != "")
		{
			SetFailure(failure);
			return CleanUp();
		}

		failure = CheckRoundTrip();
		if (failure != "")
		{
			SetFailure(failure);
			return CleanUp();
		}

		Print("OVT_StructureDamage: a container on a ruined structure closes its storage actions and gives every item back on repair");

		return CleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns the structure, parents a container to it and puts one item in that container.
	//! \param[in] origin Where the structure goes.
	//! \return An empty string when the subject stands, or what stopped it.
	protected string BuildSubject(vector origin)
	{
		m_Structure = OVT_Global.SpawnEntityPrefab(STRUCTURE_PREFAB, origin);
		if (!m_Structure)
			return string.Format("SpawnEntityPrefab() produced no structure from %1", STRUCTURE_PREFAB);

		if (!OVT_StructureDamage.IsDestructible(m_Structure))
			return "The Guard Tower reports NOT destructible, so there is no phase to round-trip";

		m_Container = OVT_Global.SpawnEntityPrefab(CONTAINER_PREFAB, origin + Vector(2, 0, 0));
		if (!m_Container)
			return string.Format("SpawnEntityPrefab() produced no container from %1", CONTAINER_PREFAB);

		m_Structure.AddChild(m_Container, -1, EAddChildFlags.RECALC_LOCAL_TRANSFORM);

		SCR_InventoryStorageManagerComponent storage = ResolveStorage();
		if (!storage)
			return "The ammo box carries no SCR_InventoryStorageManagerComponent - it is not a container any more, so this case has no subject";

		m_Item = OVT_Global.SpawnEntityPrefab(ITEM_PREFAB, origin + Vector(4, 0, 0));
		if (!m_Item)
			return string.Format("SpawnEntityPrefab() produced no item from %1", ITEM_PREFAB);

		if (!storage.TryInsertItem(m_Item))
			return "The item would not go into the ammo box, so there is no gear to lose";

		if (!storage.Contains(m_Item))
			return "The ammo box reports it does not hold the item that was just inserted into it";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Intact -> ruined -> intact, asserting the access gate and the contents at every step.
	//! \return An empty string when every claim held, or the first that did not.
	protected string CheckRoundTrip()
	{
		bool openBefore = StorageActionIsOffered();

		if (!OVT_StructureDamage.IsUsable(m_Container))
			return "The storage on an INTACT structure already reports unusable - the gate is closed on a working container and no one could ever reach the gear";

		if (!OVT_StructureDamage.Ruin(m_Structure, false))
			return "Ruin() refused the Guard Tower";

		if (!m_Container || m_Container.IsDeleted())
			return "The container was DELETED by the ruin. Everything in it went with it, which is precisely the behaviour this feature removes";

		if (OVT_StructureDamage.IsUsable(m_Container))
			return "The storage was still usable while the structure was a ruin. Every storage action reads this gate first, so players would reach into rubble for their gear instead of repairing for it";

		if (StorageActionIsOffered())
			return "OVT_OpenStorageAction still offered itself on a ruin - its CanBeShownScript gate is gone or is asking the wrong entity";

		string failure = CheckContents("ruin");
		if (failure != "")
			return failure;

		if (!OVT_StructureDamage.Repair(m_Structure))
			return "Repair() refused the ruined Guard Tower";

		if (!OVT_StructureDamage.IsUsable(m_Container))
			return "The storage was still unusable after the repair, so a repaired structure would never give its contents back";

		if (StorageActionIsOffered() != openBefore)
			return "The storage action's visibility did not return to what it was before the ruin, so a repair does not restore access";

		return CheckContents("repair");
	}

	//------------------------------------------------------------------------------------------------
	//! The gear itself: same entity, still in the same container.
	//! \param[in] step Which half of the round trip is being checked, for the message.
	//! \return An empty string when the contents survived.
	protected string CheckContents(string step)
	{
		if (!m_Item || m_Item.IsDeleted())
			return string.Format("The stored item was DELETED by the %1 - contents are supposed to be untouched by a phase change", step);

		SCR_InventoryStorageManagerComponent storage = ResolveStorage();
		if (!storage)
			return string.Format("The container lost its storage manager across the %1", step);

		if (!storage.Contains(m_Item))
			return string.Format("The item is no longer in the container after the %1. A phase change moves a mesh and nothing else, so anything that empties the storage is a real regression", step);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the generic open-storage action on the container currently shows itself.
	//!
	//! Asked of the REAL action instances, not of a copy of their rule, so the gate is exercised the
	//! way a player's context menu exercises it. A null user is fine: neither the phase gate nor the
	//! vanilla half of that answer reads one.
	//! \return True when at least one storage action would be listed.
	protected bool StorageActionIsOffered()
	{
		if (!m_Container || m_Container.IsDeleted())
			return false;

		ActionsManagerComponent actions = ActionsManagerComponent.Cast(m_Container.FindComponent(ActionsManagerComponent));
		if (!actions)
			return false;

		array<BaseUserAction> list = {};
		actions.GetActionsList(list);

		foreach (BaseUserAction action : list)
		{
			OVT_OpenStorageAction storageAction = OVT_OpenStorageAction.Cast(action);
			if (!storageAction)
				continue;

			if (storageAction.CanBeShownScript(null))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The container's storage manager, or null.
	protected SCR_InventoryStorageManagerComponent ResolveStorage()
	{
		if (!m_Container || m_Container.IsDeleted())
			return null;

		return SCR_InventoryStorageManagerComponent.Cast(m_Container.FindComponent(SCR_InventoryStorageManagerComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! \return Always true - the case is over either way.
	protected bool CleanUp()
	{
		// The item lives INSIDE the container's storage, so it goes with it - deleting it separately
		// would leave the storage holding a dead entity for as long as the box outlives it.
		if (m_Structure && m_Container && !m_Structure.IsDeleted() && !m_Container.IsDeleted())
			m_Structure.RemoveChild(m_Container);

		if (m_Container && !m_Container.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(m_Container);

		m_Container = null;
		m_Item = null;

		if (m_Structure && !m_Structure.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(m_Structure);

		m_Structure = null;

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Used by case E: one hit delivered the way a projectile or a contact delivers it - a
//! damage context handed to the damage manager, which routes it through the authored hit zone
//! (multipliers, threshold) and on into SCR_DestructionMultiPhaseComponent.OnDamage().
//------------------------------------------------------------------------------------------------
class OVT_TEST_StructureDamageHits
{
	//------------------------------------------------------------------------------------------------
	//! \param[in] component The structure's damage manager.
	//! \param[in] damage The raw damage value, before the hit zone's multipliers.
	//! \param[in] type The damage type the hit zone multiplies by.
	static void Hit(notnull OVT_StructureDestructionComponent component, float damage, EDamageType type)
	{
		IEntity owner = component.GetOwner();
		if (!owner)
			return;

		vector hitPosDirNorm[3];
		hitPosDirNorm[0] = owner.GetOrigin() + Vector(0, 1, 0);
		hitPosDirNorm[1] = vector.Forward;
		hitPosDirNorm[2] = vector.Up;

		SCR_DamageContext context = new SCR_DamageContext(type, damage, hitPosDirNorm, owner, component.GetDefaultHitZone(), Instigator.CreateInstigator(null), null, -1, -1);
		component.HandleDamage(context);
	}
}

//------------------------------------------------------------------------------------------------
//! CASE G - a ruin hides the structure's children, and a repair puts back exactly those.
//!
//! A phase change swaps the ROOT's mesh only, so decorative children outlive the collapse: the
//! reported case was a barracks whose furniture and interior lights hung in mid-air above the rubble
//! (2026-08-22). The subject is chosen for having a child at all - most buildables do not.
//!
//! The second half is the one that can regress quietly: a repair must restore only what the ruin
//! hid, never everything it finds, because pre-hidden children are a real vanilla shape.
//!
//! PROVEN ABLE TO FAIL: drop the HideChildren() call from OnBecameRuin() - red on "child still
//! visible"; drop RestoreChildren() from OnBecameIntact() - red on "child not restored".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_StructureDamage_GRuinHidesChildrenAndRepairRestoresThem : SCR_AutotestCaseBase
{
	//! The reported structure. It carries Barracks_01_military_furniture_01 as a child.
	static const ResourceName SUBJECT_PREFAB = "{048EA1F9675A05E6}Prefabs/Structures/Military/Houses/Barracks_01/OVT_Barracks.et";

	protected IEntity m_Subject;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns || towns.m_Towns.Count() < 1)
		{
			SetFailure("No towns are registered - nowhere sensible to spawn the subject");
			return true;
		}

		m_Subject = OVT_Global.SpawnEntityPrefab(SUBJECT_PREFAB, towns.m_Towns[0].location + Vector(1200, 0, 1200));
		if (!m_Subject)
		{
			SetFailure("SpawnEntityPrefab() produced no entity from " + SUBJECT_PREFAB);
			return true;
		}

		IEntity child = m_Subject.GetChildren();
		if (!child)
		{
			SetFailure("The subject has no children - this case can no longer prove anything; pick a structure that has some");
			return CleanUp();
		}

		if (!(child.GetFlags() & EntityFlags.VISIBLE))
		{
			SetFailure("The subject's first child is already hidden before the ruin - the fixture proves nothing");
			return CleanUp();
		}

		if (!OVT_StructureDamage.Ruin(m_Subject))
		{
			SetFailure("Ruin() refused the subject");
			return CleanUp();
		}

		if (child.GetFlags() & EntityFlags.VISIBLE)
		{
			SetFailure("A ruined structure's child is still visible - furniture and lights hang in the rubble");
			return CleanUp();
		}

		if (!OVT_StructureDamage.Repair(m_Subject))
		{
			SetFailure("Repair() refused the ruined subject");
			return CleanUp();
		}

		if (!(child.GetFlags() & EntityFlags.VISIBLE))
		{
			SetFailure("A repaired structure's child was not restored - the building is back but empty");
			return CleanUp();
		}

		return CleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! \return Always true - the case is over either way.
	protected bool CleanUp()
	{
		if (m_Subject && !m_Subject.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(m_Subject);

		m_Subject = null;

		return true;
	}
}
