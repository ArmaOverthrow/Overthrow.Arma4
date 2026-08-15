//------------------------------------------------------------------------------------------------
//! The five Game Master icon prefab blocks are present and correctly authored.
//!
//! WHY THIS FILE EXISTS. Everything these cases assert lives in a .et prefab, and a .et prefab is not
//! compiled. A missing OVT_GMEditableCampaignComponent block, a WHEN_SPAWNED auto-register, a wrong
//! flag sum, a LOCAL flag on the wrong family - none of them produce a compile error, and only the
//! last one produces a log line. The visible result is simply that a Game Master opens the editor and
//! sees nothing over the campaign, with nothing anywhere to say why. These cases are the only
//! automated gate on that, which is why they assert the AUTHORED VALUES (type, flags) and not merely
//! "a component exists".
//!
//! NONE OF THE FIVE PREFABS IS LOCAL, INCLUDING THE RADIO TOWERS. This corrects the assumption the
//! feature was planned on. Overthrow's TransmitterTower_01*_base.et files are same-GUID, same-path
//! DELTAS over the vanilla prefabs of those names ({68CA807F237FFB7A}, {01E409EF2B75692E},
//! {4DE187A3508D5A46}) - which is where the towers get their mesh, ladder and map descriptor - and
//! every one of those vanilla prefabs carries an RplComponent. A LOCAL flag on them would make the
//! engine log "flagged as LOCAL, but contains RplComponent" and NULL the component, so no tower
//! would ever carry an icon. The last case below is the standing tripwire for that.
//!
//! WHAT THE TEST WORLD COVERS, AND WHAT IT CANNOT.
//! Worlds/MP/OVT_Campaign_Test_Layers/default.layer contains exactly one OVT_TownController.et
//! (:298), one OVT_BaseController.et (:216) and one TransmitterTower_01_small_base.et (:150), so
//! three of the five prefabs are proven here. The remaining two - TransmitterTower_01_base.et and
//! TransmitterTower_01_medium_base.et - are NOT in the test world and no suite can see them; they are
//! covered by eye in the campaign world (Eden) at Phase 5 Step 2 of
//! docs/features/gm/hud-icons/implementation.md. Their blocks are byte-identical to the small
//! variant's apart from m_vIconPos, so this file failing is still the first thing to read if they are
//! wrong.
//!
//! NOTHING HERE IS A RETRY. Every editable component is installed during world load
//! (SCR_EditableEntityComponent.OnPostInit) and both manager registries are filled during Init, which
//! is precisely what makes this the Init tier - so a null here means "never", not "not yet".
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Shared probe: the world query for radio towers, and the per-entity claims each case makes.
//! Every check answers with the SENTENCE TO FAIL WITH, or an empty string when the entity is fine,
//! so the diagnosis lives next to the invariant it belongs to rather than being retyped per case.
//------------------------------------------------------------------------------------------------
class OVT_TEST_GMIconProbe
{
	protected ref array<ref EntityID> m_aTowers = {};

	//------------------------------------------------------------------------------------------------
	//! Finds every radio tower in the world, mirroring the manager's own filter
	//! (OVT_OccupyingFactionManager.FilterTransmitterTowerEntities) so the set under test is exactly
	//! the set Overthrow itself treats as radio towers.
	//! \return Number of towers found.
	int CollectTransmitterTowers()
	{
		m_aTowers.Clear();
		GetGame().GetWorld().QueryEntitiesBySphere("0 0 0", 99999999, CollectTower, FilterTower, EQueryEntitiesFlags.STATIC);

		return m_aTowers.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The collected tower entity ids. Empty until CollectTransmitterTowers() has run.
	array<ref EntityID> GetTowers()
	{
		return m_aTowers;
	}

	//------------------------------------------------------------------------------------------------
	protected bool CollectTower(IEntity entity)
	{
		m_aTowers.Insert(entity.GetID());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool FilterTower(IEntity entity)
	{
		OVT_TowerControllerComponent controller = OVT_TowerControllerComponent.Cast(entity.FindComponent(OVT_TowerControllerComponent));
		if (controller)
			return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The claims every icon-carrying entity makes, whatever family it belongs to: the component is
	//! there, it is the SYSTEM type the icon layout and the 48 px slot are chosen for, and it is
	//! flagged NON_DELETABLE so a Game Master cannot delete the campaign object out from under the
	//! managers.
	//! \param[in] entity Entity to check.
	//! \param[in] label What to call it in a failure ("town controller", "base controller", ...).
	//! \param[in] prefab The prefab whose block would have to be missing or wrong.
	//! \return Empty string when the entity is correct, otherwise the failure sentence.
	static string CheckIconEntity(IEntity entity, string label, string prefab)
	{
		if (!entity)
			return string.Format("The %1 entity does not resolve in the world, so its prefab block cannot be checked at all", label);

		OVT_GMEditableCampaignComponent editable = OVT_GMEditableCampaignComponent.Cast(entity.FindComponent(OVT_GMEditableCampaignComponent));
		if (!editable)
			return string.Format("The %1 carries no OVT_GMEditableCampaignComponent. %2 has lost its component block, so this campaign object is invisible in the Game Master editor - no icon, no hover, no selection, and nothing in any log.", label, prefab);

		EEditableEntityType type = editable.GetEntityType();
		if (type != EEditableEntityType.SYSTEM)
			return string.Format("The %1's editable component reports type %2, expected SYSTEM. %3 has the wrong m_EntityType: the icon is drawn with another type's layout and slot size, and GENERIC's 24 px slot is illegible at a strategic altitude. Note that GENERIC is also what a component nulled by the LOCAL/RplComponent check reports.", label, typename.EnumToString(EEditableEntityType, type), prefab);

		if (!editable.HasEntityFlag(EEditableEntityFlag.NON_DELETABLE))
			return string.Format("The %1's editable component is not flagged NON_DELETABLE. %2 has the wrong m_Flags sum (2052 = NON_DELETABLE 2048 + HAS_FACTION 4, on all five prefabs), so a Game Master can delete the campaign object and take its manager registration with it.", label, prefab);

		if (editable.HasEntityFlag(EEditableEntityFlag.NON_INTERACTIVE))
			return string.Format("The %1's editable component is flagged NON_INTERACTIVE. %2 has the NON_INTERACTIVE bit (16) in its m_Flags sum: the icon cannot be hovered or selected (SCR_HoverEditableEntityFilter returns null for it), which is the entire feature.", label, prefab);

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! The invariant SCR_EditableEntityComponent.OnPostInit (:2237-2256) enforces at world load with
	//! an ERROR print and by NULLING the component: LOCAL exactly when there is no RplComponent.
	//! Getting this wrong is the single most likely silent failure of this feature, and the wrong
	//! half of it is silent in a different way - a nulled component still exists as an object, it just
	//! answers every question wrongly.
	//! \param[in] entity Entity to check.
	//! \param[in] label What to call it in a failure.
	//! \return Empty string when the flag and the component agree, otherwise the failure sentence.
	static string CheckLocalMatchesReplication(IEntity entity, string label)
	{
		if (!entity)
			return string.Format("The %1 entity does not resolve in the world, so the LOCAL/RplComponent invariant cannot be checked", label);

		OVT_GMEditableCampaignComponent editable = OVT_GMEditableCampaignComponent.Cast(entity.FindComponent(OVT_GMEditableCampaignComponent));
		if (!editable)
			return string.Format("The %1 carries no OVT_GMEditableCampaignComponent, so the LOCAL/RplComponent invariant cannot be checked", label);

		bool isLocal = editable.HasEntityFlag(EEditableEntityFlag.LOCAL);
		bool hasRpl = RplComponent.Cast(entity.FindComponent(RplComponent)) != null;

		if (isLocal && hasRpl)
			return string.Format("The %1 is flagged LOCAL but carries an RplComponent. The engine logs \"flagged as LOCAL, but contains RplComponent\" and NULLS the editable component at world load - the icon never appears and the component silently answers as an unregistered GENERIC entity.", label);

		if (!isLocal && !hasRpl)
			return string.Format("The %1 is NOT flagged LOCAL and has no RplComponent. The engine logs \"missing RplComponent\" and NULLS the editable component at world load - same silent outcome. Either the prefab lost its RplComponent, or it needs LOCAL (m_Flags + 8) - and if it is a radio tower, check first that its vanilla same-GUID base prefab still carries the RplComponent this feature relies on.", label);

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! The per-instance UI info answers both questions the vanilla tooltip asks it.
	//!
	//! THE HasDescription() TRAP IS THE REASON THIS EXISTS. SCR_DescriptionTooltipDetail.InitDetail
	//! (:17) returns false when entity.GetInfo().HasDescription() is false, and
	//! SCR_EntityTooltipDetail.CreateDetail (:37-46) then REMOVES the description widget - so an
	//! OVT_GMCampaignUIInfo.GetDescription() that returns an empty string for any reachable state
	//! deletes the tooltip line silently: no compile error, no log, no widget, just a tooltip that
	//! shows the name and nothing else. HasDescription() IS GetDescription() (SCR_UIDescription.c:39-41),
	//! so calling it here exercises the real composition path against real campaign state.
	//!
	//! The name is checked in the same breath because it costs one call and covers the other half of
	//! the info contract: an empty name means SetInfoInstance() did not happen at all.
	//! \param[in] entity Entity to check.
	//! \param[in] label What to call it in a failure.
	//! \return Empty string when the info answers both, otherwise the failure sentence.
	static string CheckInfoDescribes(IEntity entity, string label)
	{
		if (!entity)
			return string.Format("The %1 entity does not resolve in the world, so its tooltip info cannot be checked", label);

		OVT_GMEditableCampaignComponent editable = OVT_GMEditableCampaignComponent.Cast(entity.FindComponent(OVT_GMEditableCampaignComponent));
		if (!editable)
			return string.Format("The %1 carries no OVT_GMEditableCampaignComponent, so it has no tooltip info to check", label);

		SCR_UIInfo info = editable.GetInfo();
		if (!info)
			return string.Format("The %1's editable component has no UI info at all. SetInfoInstance() did not run and the prefab authors no m_UIInfo either - the icon has no name, no description and no image.", label);

		string name = info.GetName();
		if (name == string.Empty)
			return string.Format("The %1's UI info returns an empty name. The icon and the tooltip header are blank; the usual cause is that OVT_GMCampaignUIInfo.GetName() fell through every kind branch.", label);

		if (!info.HasDescription())
			return string.Format("The %1's UI info reports HasDescription() false, i.e. OVT_GMCampaignUIInfo.GetDescription() returned an EMPTY string for this entity. The vanilla tooltip destroys its description widget in that case (SCR_DescriptionTooltipDetail.InitDetail :17), so the hover shows the name with no campaign line and nothing anywhere says why. Every unresolved branch must return the fallback line, never string.Empty.", label);

		return string.Empty;
	}
}

//------------------------------------------------------------------------------------------------
//! The town controller prefab carries the icon component, typed SYSTEM and non-deletable.
//!
//! CAN-FAIL METHOD: remove the OVT_GMEditableCampaignComponent block from
//! Prefabs/Controllers/OVT_TownController.et and re-run - this case then fails with "The town
//! controller carries no OVT_GMEditableCampaignComponent". Setting m_EntityType to GENERIC, or
//! m_Flags to 4, fails the other two claims by name.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_GMIcons_TownControllerIsEditable : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
		{
			SetFailure("OVT_Global.GetTowns() is null - see OVT_TEST_Init_Globals_ManagersResolve for the wiring this depends on");
			return true;
		}

		// >= 1, never a magic count: the test world has exactly one town controller.
		if (towns.m_TownControllers.Count() < 1)
		{
			SetFailure("No town controllers are registered, so there is no town entity to check a prefab block on. This is OVT_TEST_Init_Controllers_AreRegistered's territory, not an icon defect.");
			return true;
		}

		IEntity townEntity = GetGame().GetWorld().FindEntityByID(towns.m_TownControllers[0]);

		string failure = OVT_TEST_GMIconProbe.CheckIconEntity(townEntity, "town controller", "Prefabs/Controllers/OVT_TownController.et");
		if (failure != string.Empty)
		{
			SetFailure("%1", failure);
			return true;
		}

		PrintFormat("GM icons: town controller carries OVT_GMEditableCampaignComponent (SYSTEM, NON_DELETABLE); %1 town controller(s) registered",
			towns.m_TownControllers.Count().ToString());

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The base controller prefab carries the icon component, typed SYSTEM and non-deletable.
//!
//! Reached through OVT_OccupyingFactionManager.m_Bases[0].entId - the manager's own handle on the
//! base entity - so a base that is registered but whose entity has drifted away from the prefab under
//! test is reported as such rather than silently skipped.
//!
//! CAN-FAIL METHOD: remove the OVT_GMEditableCampaignComponent block from
//! Prefabs/Controllers/OVT_BaseController.et.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_GMIcons_BaseControllerIsEditable : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
		{
			SetFailure("OVT_Global.GetOccupyingFaction() is null - see OVT_TEST_Init_Globals_ManagersResolve for the wiring this depends on");
			return true;
		}

		// >= 1, never a magic count: the test world has exactly one base.
		if (occupying.m_Bases.Count() < 1)
		{
			SetFailure("No bases are registered, so there is no base entity to check a prefab block on. This is OVT_TEST_Init_Controllers_AreRegistered's territory, not an icon defect.");
			return true;
		}

		IEntity baseEntity = GetGame().GetWorld().FindEntityByID(occupying.m_Bases[0].entId);

		string failure = OVT_TEST_GMIconProbe.CheckIconEntity(baseEntity, "base controller", "Prefabs/Controllers/OVT_BaseController.et");
		if (failure != string.Empty)
		{
			SetFailure("%1", failure);
			return true;
		}

		PrintFormat("GM icons: base controller carries OVT_GMEditableCampaignComponent (SYSTEM, NON_DELETABLE); %1 base(s) registered",
			occupying.m_Bases.Count().ToString());

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! EVERY radio tower in the world carries the icon component, and none of them is LOCAL.
//!
//! The count is asserted at >= 1 first, so the case cannot pass vacuously on a world with no towers -
//! which is exactly what an "every tower" loop does when the query returns nothing.
//!
//! THE ABSENCE OF LOCAL IS PART OF THE CLAIM, not a detail. A tower inherits an RplComponent from
//! the vanilla prefab its Overthrow file deltas over (see the file header), so LOCAL would make the
//! engine null the component at world load - the icon would simply never exist, and the only clue
//! would be one ERROR line per tower in the world-load log. It also means a tower is NOT
//! un-draggable for free the way a LOCAL entity would be; that job belongs entirely to
//! OVT_GMEditableCampaignComponent.IsReplicated().
//!
//! CAN-FAIL METHOD: remove the OVT_GMEditableCampaignComponent block from
//! Prefabs/Structures/Infrastructure/Towers/TransmitterTower_01/TransmitterTower_01_small_base.et -
//! the only variant in the test world - or set its m_Flags to 2060.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_GMIcons_TowersAreEditable : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_TEST_GMIconProbe probe = new OVT_TEST_GMIconProbe();
		int towerCount = probe.CollectTransmitterTowers();

		if (towerCount < 1)
		{
			SetFailure("No radio towers were found in the world by the manager's own filter (an entity carrying OVT_TowerControllerComponent). Without one, this case would pass while saying nothing - the test world is expected to carry TransmitterTower_01_small_base.et.");
			return true;
		}

		foreach (EntityID towerId : probe.GetTowers())
		{
			IEntity tower = GetGame().GetWorld().FindEntityByID(towerId);

			string failure = OVT_TEST_GMIconProbe.CheckIconEntity(tower, "radio tower", "Prefabs/Structures/Infrastructure/Towers/TransmitterTower_01/TransmitterTower_01_small_base.et");
			if (failure != string.Empty)
			{
				SetFailure("%1", failure);
				return true;
			}

			OVT_GMEditableCampaignComponent editable = OVT_GMEditableCampaignComponent.Cast(tower.FindComponent(OVT_GMEditableCampaignComponent));
			if (editable.HasEntityFlag(EEditableEntityFlag.LOCAL))
			{
				SetFailure("A radio tower's editable component IS flagged LOCAL (m_Flags must be 2052, not 2060). Overthrow's tower prefabs are same-GUID deltas over vanilla prefabs that carry an RplComponent, so the engine logs \"flagged as LOCAL, but contains RplComponent\" and nulls the component - the tower has no icon at all.");
				return true;
			}
		}

		PrintFormat("GM icons: all %1 radio tower(s) carry OVT_GMEditableCampaignComponent (SYSTEM, NON_DELETABLE, not LOCAL)", towerCount.ToString());

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Across all three families: LOCAL is set exactly when the entity has no RplComponent.
//!
//! This is a SEPARATE case from the three above on purpose. Those ask "is the block there and does it
//! say what it should"; this one asks the question the ENGINE asks at world load, and answers it for
//! every family in one place. It is two tripwires in one: a future Reforger release renumbering
//! EEditableEntityFlag (flag values are a durable coupling this feature creates, and a renumbering
//! would change behaviour with no other symptom), and a future Reforger release dropping the
//! RplComponent from the vanilla transmitter-tower prefabs the Overthrow ones delta over - at which
//! point the towers WOULD need LOCAL, and this case says so in its failure text.
//!
//! CAN-FAIL METHOD: flip the LOCAL bit on any one of the five prefabs - 2052 <-> 2060.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_GMIcons_LocalFlagMatchesReplication : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();

		if (!towns || !occupying)
		{
			SetFailure("OVT_Global.GetTowns() or GetOccupyingFaction() is null - see OVT_TEST_Init_Globals_ManagersResolve");
			return true;
		}

		if (towns.m_TownControllers.Count() < 1 || occupying.m_Bases.Count() < 1)
		{
			SetFailure("No town controller or no base is registered, so this invariant cannot be checked on the replicated families");
			return true;
		}

		string failure = OVT_TEST_GMIconProbe.CheckLocalMatchesReplication(
			GetGame().GetWorld().FindEntityByID(towns.m_TownControllers[0]), "town controller");
		if (failure != string.Empty)
		{
			SetFailure("%1", failure);
			return true;
		}

		failure = OVT_TEST_GMIconProbe.CheckLocalMatchesReplication(
			GetGame().GetWorld().FindEntityByID(occupying.m_Bases[0].entId), "base controller");
		if (failure != string.Empty)
		{
			SetFailure("%1", failure);
			return true;
		}

		OVT_TEST_GMIconProbe probe = new OVT_TEST_GMIconProbe();
		int towerCount = probe.CollectTransmitterTowers();

		if (towerCount < 1)
		{
			SetFailure("No radio towers were found in the world, so the LOCAL half of this invariant would go unchecked");
			return true;
		}

		foreach (EntityID towerId : probe.GetTowers())
		{
			failure = OVT_TEST_GMIconProbe.CheckLocalMatchesReplication(
				GetGame().GetWorld().FindEntityByID(towerId), "radio tower");
			if (failure != string.Empty)
			{
				SetFailure("%1", failure);
				return true;
			}
		}

		PrintFormat("GM icons: LOCAL flag matches RplComponent presence on the town controller, the base controller and all %1 radio tower(s)", towerCount.ToString());

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Across all three families: the per-instance UI info has a name AND a non-empty description.
//!
//! THIS GUARDS THE HasDescription() TRAP. An empty description does not fail loudly - it makes the
//! vanilla tooltip destroy its own description widget before it renders
//! (SCR_DescriptionTooltipDetail.c:17 gating SCR_EntityTooltipDetail.CreateDetail :37-46), so the
//! symptom is a tooltip that shows the entity name and simply omits the Overthrow line, with no
//! compile error, no script error and no log entry. HasDescription() calls GetDescription()
//! (SCR_UIDescription.c:39-41), so this case runs the real composition - town support and stability,
//! base faction and garrison, tower status - against the live managers of the test world, and the
//! fallback branches inside it are what keep the answer non-empty when a record is missing.
//!
//! It is a separate case from the four above because it asserts a SCRIPT invariant, not a prefab
//! one: those fail when a .et loses a block, this one fails when OVT_GMCampaignUIInfo grows a branch
//! that returns string.Empty.
//!
//! CAN-FAIL METHOD: in Scripts/Game/Components/GM/OVT_GMCampaignUIInfo.c, change DescribeFallback()
//! to return string.Empty and make GetDescription() return DescribeFallback() unconditionally. This
//! case then fails with "The town controller's UI info reports HasDescription() false...".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_GMIcons_InfoHasNameAndDescription : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();

		if (!towns || !occupying)
		{
			SetFailure("OVT_Global.GetTowns() or GetOccupyingFaction() is null - see OVT_TEST_Init_Globals_ManagersResolve");
			return true;
		}

		if (towns.m_TownControllers.Count() < 1 || occupying.m_Bases.Count() < 1)
		{
			SetFailure("No town controller or no base is registered, so there is no info instance to ask for a name and a description");
			return true;
		}

		string failure = OVT_TEST_GMIconProbe.CheckInfoDescribes(
			GetGame().GetWorld().FindEntityByID(towns.m_TownControllers[0]), "town controller");
		if (failure != string.Empty)
		{
			SetFailure("%1", failure);
			return true;
		}

		failure = OVT_TEST_GMIconProbe.CheckInfoDescribes(
			GetGame().GetWorld().FindEntityByID(occupying.m_Bases[0].entId), "base controller");
		if (failure != string.Empty)
		{
			SetFailure("%1", failure);
			return true;
		}

		OVT_TEST_GMIconProbe probe = new OVT_TEST_GMIconProbe();
		int towerCount = probe.CollectTransmitterTowers();

		if (towerCount < 1)
		{
			SetFailure("No radio towers were found in the world, so the tower half of the tooltip contract would go unchecked");
			return true;
		}

		foreach (EntityID towerId : probe.GetTowers())
		{
			failure = OVT_TEST_GMIconProbe.CheckInfoDescribes(
				GetGame().GetWorld().FindEntityByID(towerId), "radio tower");
			if (failure != string.Empty)
			{
				SetFailure("%1", failure);
				return true;
			}
		}

		PrintFormat("GM icons: UI info returns a name and a non-empty description for the town controller, the base controller and all %1 radio tower(s)", towerCount.ToString());

		return true;
	}
}
