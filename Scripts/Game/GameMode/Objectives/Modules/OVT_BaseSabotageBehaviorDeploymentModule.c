//------------------------------------------------------------------------------------------------
//! BASE SABOTAGE: a team the objective director sent infiltrates a base the resistance took, holds
//! it while nobody is looking, and DEMOLISHES WHAT THE PLAYERS BUILT - one structure at a time,
//! cheapest first, permanently.
//!
//! 🔴 THIS MODULE DESTROYS PLAYER PROPERTY AND NOTHING GIVES IT BACK. There is no rubble, no repair
//! action, no refund and no undo; a destroyed structure does not come back on the next load either,
//! because a deleted entity is simply not saved. It is the base-objective mirror of the town ramp's
//! support slide: the price of letting the occupying faction work on a base you captured is that the
//! base gets emptier. Everything below that looks like defensive caution - the hold, the
//! nobody-nearby rule, the per-mission cap, the cheapest-first order and the one notification - is
//! there because the effect is irreversible.
//!
//! WHAT IT DOES, PER UPDATE (~10 s):
//!   resolve the base under this deployment, and REFUSE if the occupying faction already holds it;
//!   count the living members of this deployment's force inside m_fClearRadius of the base centre;
//!   ask whether a DEFENDER is standing inside the same circle - a player, or any player's living
//!     recruit (author, 2026-08-21: *"recruits should still count as well as players"* - he watched a
//!     structure go while his squad fought over it and he sat just outside the circle);
//!   and hand both to EvaluateDemolition(), which owns the whole decision.
//! Every time the interval elapses, ONE structure goes and the interval restarts. At
//! objectiveSabotageStructuresPerMission, or when there is nothing left to take, the mission reports
//! to the director and the deployment asks to be collected.
//!
//! ⚠ THE CLOCK PAUSES, IT NEVER RESETS, exactly as the town harassment and tower recapture modules'
//! do. A defender who walks past once every few minutes should DELAY the next demolition, not
//! immunise the base by rolling the timer back to full.
//!
//! ⚠ CHEAPEST FIRST, AND COST IS THE ONLY ORDERING KEY THAT EXISTS. Neither OVT_Buildable nor
//! OVT_Placeable carries a size, a footprint or an importance - m_iCost is the game's own statement
//! of what a structure is worth. The shipped prices make the intended progression fall out on their
//! own (bunkers 750 -> tents 1000 -> guard tower 1200 -> ramp/helipad 1500 -> fuel depot 2000 ->
//! garage 8000), so a base is nibbled from the edges inward and the expensive thing the player cares
//! about most is the last to go - which is the difference between "I have time to respond" and "I
//! logged in and the garage was gone".
//!
//! ⚠ BUILDABLES ONLY. PLACEABLES ARE NOT TARGETS, AND THAT IS WHAT MAKES THE LADDER ABOVE THE ONE
//! THAT ACTUALLY RUNS (user, 2026-08-19: "placeables dont actually make any sense to sabotage, they
//! are just sandbags, furniture, lights, etc"). The filter used to take EITHER ownership component,
//! and because placeables are priced 5-250 while the cheapest buildable is a 750 bunker, "cheapest
//! first" meant a base's clutter - sandbags (20), signs (20), furniture (25), cots (30), hedgehogs
//! (60), camps (250), floodlights (250) - was ALWAYS demolished before a single built structure. A
//! play-test watched a team take an 80-resource ammobox while a recruitment tent stood untouched.
//! Restricting the filter to OVT_BuildableComponent is therefore not a narrowing of scope for its own
//! sake: it is what makes the demolition ladder land on the things the player built and cares about.
//!
//! ⚠ THE COST OF THAT: A BASE WITH NOTHING BUILT ON IT NOW YIELDS NO CANDIDATES AT ALL, so its
//! mission completes on the first interval as "there was nothing left to demolish" - a SUCCESS that
//! advances the counter-attack ramp on a base that visibly has objects on it. That behaviour is
//! unchanged and deliberate (see DemolishNextStructure), but dropping placeables widens the set of
//! bases it applies to from "bases with nothing on them" to "bases with nothing BUILT on them".
//!
//! ⚠ IT NEITHER DELETES NOR TOUCHES THE NAVMESH ITSELF. Removal goes through
//! OVT_ResistanceFactionManager.DestroyPlacedItem(), the one shared path that captures the navmesh
//! carve BEFORE the entity stops existing. Copying those two lines here would be the same mistake in
//! a fifth place, and getting their ORDER wrong leaves the AI refusing to walk through ground that is
//! now empty - with no symptom anyone would trace back to a demolished tent.
//!
//! ⚠ AUTHOR THIS MODULE BEFORE THE REINFORCEMENT MODULE IN A CONFIG'S m_aModules, the same ordering
//! constraint the other two objective behaviour modules record: a mission that completes after the
//! reinforcement module has already run in the same pass would have its team rebought on the way out.
//!
//! THE SPLIT: EvaluateDemolition() and IsSabotageTarget() take their inputs as arguments so the two
//! claims worth asserting - "nothing is destroyed unless the team holds the place unopposed for the
//! whole interval" and "only this base's player-built structures are ever candidates" - are
//! answerable with no live deployment, no base and no AI. The pattern is
//! OVT_TownHarassmentBehaviorDeploymentModule.EvaluateHold().
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_BaseSabotageBehaviorDeploymentModule : OVT_BaseBehaviorDeploymentModule
{
	[Attribute(desc: "Name of this module")]
	string m_sModuleName;

	[Attribute(defvalue: "150", desc: "How close to the base centre the team has to be to count as holding it. Also the circle a DEFENDER has to be inside to stop the demolitions - a player, or any living recruit belonging to any player")]
	float m_fClearRadius;

	[Attribute(defvalue: "500", desc: "Radius searched for player structures around the base centre. Matches OVT_ItemLimitChecker.CountItemsForLocation's own radius for EOVTBaseType.BASE - bases are large and the two must agree, or a structure the placement limit counted cannot be found again")]
	float m_fSearchRadius;

	[Attribute(defvalue: "300", desc: "Maximum distance from the deployment to the base it may sabotage. A base objective is created AT the base's own position, so this only has to survive a spawn nudge")]
	float m_fMaxBaseDistance;

	//! =============================================================================================
	//! ⚠ THE DIFFICULTY CONVENTION IS "-1 MEANS ASK THE CAMPAIGN", AND IT WAS FLIPPED HERE ON
	//! 2026-08-21 (occupying/objectives build phase 4).
	//! =============================================================================================
	//! It used to be the opposite - "FALLBACK ONLY: the campaign's value is used WHENEVER difficulty
	//! settings are loaded" - which meant an authored number was silently IGNORED in every real
	//! campaign. A .conf field a server owner can tune and that then does nothing is worse than a
	//! missing one, because the whole authored surface loses its credibility with it.
	//!
	//! ⚠ THE FLIP IS BEHAVIOUR-NEUTRAL ONLY BECAUSE THE SHIPPED CONFIG WAS RE-AUTHORED TO -1 IN THE
	//! SAME CHANGE. A config left holding its old positive number would now be HONOURED instead of
	//! overridden, and the campaign would stop scaling with difficulty. That is exactly why
	//! OVT_TowerRecaptureBehaviorDeploymentModule was NOT flipped with the other two - see its own
	//! header - and why OVT_BaseRepairBehaviorDeploymentModule, which left for the deployments
	//! framework as a pure relocation, is excluded as well.
	//!
	//! ⚠ AN AUTHORED ZERO IS NOW AN AUTHORED ZERO, not "unauthored". Every resolver below still clamps
	//! to its own floor, so a zero reads as the smallest sane value rather than as a divide-by-nothing.
	[Attribute(defvalue: "-1", desc: "Seconds between demolitions. -1 = the campaign's objectiveSabotageHoldSeconds difficulty setting (180 on Easy down to 60 on Insane), which is what the shipped config authors. Any other value OVERRIDES difficulty for this config")]
	int m_iHoldSeconds;

	[Attribute(defvalue: "-1", desc: "Structures destroyed before the mission reports and stands down. -1 = the campaign's objectiveSabotageStructuresPerMission difficulty setting (1 on Easy up to 3 on Insane), which is what the shipped config authors. Any other value OVERRIDES difficulty for this config - see m_iHoldSeconds for the convention")]
	int m_iStructuresPerMission;

	//! One deployment update, in seconds. See OVT_TownHarassmentBehaviorDeploymentModule.UPDATE_SECONDS.
	static const int UPDATE_SECONDS = 10;

	//! The broadcast notification preset sent once per mission, authored in
	//! Configs/overthrowBroadcastMessages.conf. ⚠ MATCHED BY STRING and by nothing else.
	static const string SABOTAGE_NOTIFICATION = "ObjectiveSabotage";

	//! Fired-once latch. NOT an attribute, NOT persisted and NOT copied by CloneModule - a clone is a
	//! fresh deployment's module and has demolished nothing. Not persisting it is safe: a restored
	//! deployment starts its interval again, which delays the next demolition rather than repeating
	//! one, and the structures already destroyed are already gone.
	protected bool m_bMissionReported;

	//! Structures this mission has taken, against objectiveSabotageStructuresPerMission.
	protected int m_iDestroyed;

	//! Whether the one-per-mission notification has gone out.
	protected bool m_bNotified;

	//! Updates left before the next demolition. Armed lazily on the first update, because the
	//! difficulty settings this reads are not guaranteed to be loaded when a module is cloned.
	protected int m_iTicksLeft;

	protected bool m_bArmed;

	//! Candidates found by the current sphere query, and their authored costs, parallel.
	//! Rebuilt from scratch on every demolition, so a handle can never outlive the entity it names.
	protected ref array<IEntity> m_aTargets;
	protected ref array<int> m_aTargetCosts;

	//! Query state, read by the collect callback. Set immediately before the query and used only by it.
	//! ⚠ THE MANAGER IS CACHED HERE RATHER THAN RESOLVED IN THE CALLBACK: a sphere query over a base can
	//! offer hundreds of entities, and a callback that resolved the game mode's component for each one
	//! would pay for it every time. It is also the only reference the callback has that could be null.
	protected string m_sTargetBaseId;
	protected int m_iTargetBaseFaction;
	protected int m_iMyFaction;
	protected OVT_ResistanceFactionManager m_QueryResistance;

	//------------------------------------------------------------------------------------------------
	//! One observation of the base.
	//! \param[in] deltaTime Milliseconds nominally elapsed. Unused - the interval is counted in
	//!            updates, not in wall time; see UPDATE_SECONDS.
	override void OnUpdate(int deltaTime)
	{
		super.OnUpdate(deltaTime);

		if (m_bMissionReported || !m_ParentDeployment)
			return;

		OVT_BaseData base = ResolveTargetBase();
		if (!base)
			return;

		if (!m_bArmed)
		{
			m_iTicksLeft = ResolveIntervalTicks();
			m_bArmed = true;
		}

		int aliveInside = CountAliveRegisteredMembersWithin(base.location, m_fClearRadius);
		// ⚠ DEFENDERS, NOT JUST PLAYERS. A squad of the player's recruits fighting over this base
		// contests it exactly as much as he would standing here - see DefenderWithin(), and see the
		// play-test that made it necessary.
		bool enemyPresent = DefenderWithin(base.location, m_fClearRadius);

		if (!EvaluateDemolition(aliveInside, enemyPresent, m_iTicksLeft))
			return;

		DemolishNextStructure(base);
	}

	//------------------------------------------------------------------------------------------------
	//! THE DEMOLITION DECISION, with every input passed in.
	//!
	//! ⚠ IT DOES NOT LATCH, unlike its two sibling modules' decisions. A sabotage mission fires
	//! REPEATEDLY - once per interval, up to the per-mission cap - so the fired-once latch here belongs
	//! to the MISSION (m_bMissionReported), not to a single demolition. The caller re-arms the interval.
	//!
	//! \param[in] aliveInside Living members of this deployment's force inside the clear radius.
	//! \param[in] enemyPresent Whether a DEFENDER - a player or any player's living recruit - is standing
	//!            inside the same circle.
	//! \param[inout] ticksLeft Updates still owed. PAUSED, never reset, on an interrupted tick.
	//! \return True when THIS call completed an interval and one structure may now be taken.
	bool EvaluateDemolition(int aliveInside, bool enemyPresent, inout int ticksLeft)
	{
		if (m_bMissionReported)
			return false;

		// Nobody there: the team is dead, still on the road, or scattered. The clock waits for them.
		if (aliveInside < 1)
			return false;

		// Contested - by a player or by his recruits. Men being shot at are not quietly rigging charges.
		// Pause; do not roll back.
		if (enemyPresent)
			return false;

		// ⚠ A NON-POSITIVE INTERVAL MUST STILL TAKE ONE TICK. A misauthored zero would otherwise
		// demolish something on the update the team is registered - before they are out of the truck,
		// and with no warning at all for an effect that cannot be undone.
		if (ticksLeft > 0)
			ticksLeft = ticksLeft - 1;

		if (ticksLeft > 0)
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! WHETHER ONE STRUCTURE IS A LEGITIMATE TARGET, with every input passed in.
	//!
	//! Three exclusions, and each of them is a way this module could otherwise destroy something it has
	//! no business destroying:
	//!   1. THE BASE IS OURS. If the occupying faction holds the base, the structures on it are its own
	//!      spoils - a base that flipped while the team was walking there must not be stripped by the
	//!      side that just took it. (The config's OVT_BaseControlConditionDeploymentModule collects the
	//!      deployment for the same reason, but a condition module runs on its own schedule and this
	//!      one is checked on the update that matters.)
	//!   2. IT BELONGS SOMEWHERE ELSE. A structure associated with a different base - or with a camp or
	//!      a forward base that happens to sit inside the search radius - is not part of this base.
	//!   3. IT BELONGS NOWHERE. An empty association is an object placed outside any location; the base
	//!      does not own it and neither does this mission.
	//! \param[in] associatedBaseId The structure's recorded association.
	//! \param[in] associatedType The structure's recorded association type.
	//! \param[in] targetBaseId The base being sabotaged.
	//! \param[in] targetBaseFaction Which faction currently holds that base.
	//! \param[in] myFaction This deployment's controlling faction.
	//! \return True when the structure may be destroyed by this mission.
	static bool IsSabotageTarget(string associatedBaseId, EOVTBaseType associatedType, string targetBaseId, int targetBaseFaction, int myFaction)
	{
		if (targetBaseFaction == myFaction)
			return false;

		if (targetBaseId == "")
			return false;

		if (associatedType != EOVTBaseType.BASE)
			return false;

		return associatedBaseId == targetBaseId;
	}

	//------------------------------------------------------------------------------------------------
	//! 🔴 ANYTHING THE PLAYER KEEPS GEAR IN IS OFF LIMITS, AND THIS IS A GAMEPLAY DECISION RATHER THAN
	//! A TECHNICAL EXCLUSION (user, 2026-08-19).
	//!
	//! A play-test watched a sabotage team demolish an ammobox - the cheapest thing on the base, so the
	//! first thing taken - and with it every weapon, magazine and piece of kit the player had stored in
	//! it. Losing a structure you paid 750 for is a setback you can see coming and rebuild; losing the
	//! contents of a box is losing the campaign's accumulated loot with no warning, no wreck to loot back
	//! and no notification that named the box. The user's decision was explicit: the mechanics for
	//! recovering gear from a destroyed container will be developed later, and until they exist a
	//! sabotage team simply leaves the boxes alone.
	//!
	//! ⚠ THE TEST IS THE STORAGE COMPONENT, NOT THE PREFAB AND NOT THE PLACEABLE TYPE STRING. A prefab
	//! list would have to be maintained in parallel with placeables.conf, and the type string
	//! ("Ammobox") is authored per prefab and is exactly the kind of join that already disagrees with
	//! itself elsewhere in this tree - GetStructureCost()'s header records seven of the eight buildables
	//! whose type string does NOT match their config entry. BaseInventoryStorageComponent is the
	//! CATEGORY: it is what makes a thing a container at all, it is what every future gear box will have
	//! without anybody remembering to add it to a list, and it is the same component the inventory menu
	//! opens.
	//!
	//! WHAT IT CATCHES, verified against the shipped data (2026-08-19): all four prefabs of the
	//! "Ammobox" placeable entry - OVT_AmmoBox_Placed (through OVT_AmmoBox_Base),
	//! OVT_CabinetMetal_01_grey_V1, and the two FIA equipment boxes - each of which authors an
	//! SCR_UniversalInventoryStorageComponent, which derives from this class.
	//!
	//! WHAT IT DOES NOT CATCH, verified the same way: every other shipped placeable (posters, camps,
	//! sandbags, hedgehogs, floodlights, signs, furniture, cots, the pirate radio) and every shipped
	//! buildable (guard tower, both tents, the maintenance ramp, bunkers, garage, helipad, fuel depot)
	//! carries no storage component on its root at all. The exclusion therefore costs the mission nothing
	//! it was meant to have.
	//!
	//! 🔴 AND AS OF THE BUILDABLES-ONLY FILTER IT IS UNREACHABLE ON SHIPPED DATA, DELIBERATELY KEPT
	//! (2026-08-19). All four containers it was written for are Ammobox PLACEABLES, and a placeable is no
	//! longer a candidate at all, so re-checking every buildable prefab and its vanilla parents found not
	//! one with storage on its root - nothing this can currently refuse ever gets this far. It stays for
	//! two reasons and neither is sentiment: it is the STANDING STATEMENT of a decision that is coming
	//! back ("when its done right", once destroyed containers give their contents up somehow), and it is
	//! the automatic guard for the first buildable that IS a container - a weapon cache, a supply crate,
	//! a modded armoury - which would otherwise be priced, sorted and demolished with a player's kit
	//! inside it before anybody remembered this conversation. Deleting it would cost a component lookup's
	//! worth of nothing and buy a silent regression.
	//! \param[in] entity The candidate structure.
	//! \return True when the structure is a container and must be left standing.
	static bool IsGearContainer(IEntity entity)
	{
		if (!entity)
			return false;

		BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(entity.FindComponent(BaseInventoryStorageComponent));

		return storage != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes exactly one structure, and decides whether the mission is over.
	//!
	//! ⚠ THE CANDIDATE LIST IS REBUILT EVERY TIME rather than cached across intervals. Two minutes pass
	//! between demolitions; in that time a player can build something new, another mission can take
	//! something, and the base can change hands. A cached list would hand deleted entities to the
	//! removal path and miss everything built since the mission started.
	//! \param[in] base The base being sabotaged.
	protected void DemolishNextStructure(notnull OVT_BaseData base)
	{
		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (!resistance)
			return;

		CollectTargets(base, resistance);

		int index = OVT_ObjectiveSelection.NextTargetIndex(m_aTargetCosts, null);
		if (index == OVT_ObjectiveSelection.NOTHING_TO_SELECT)
		{
			// NOTHING LEFT TO TAKE IS STILL A COMPLETED MISSION. A base the resistance built nothing on
			// would otherwise send mission after mission for the whole phase and never open the gate -
			// a ramp that cannot finish, with no log line to explain it.
			//
			// ⚠ "NOTHING LEFT" NOW MEANS "NOTHING BUILT LEFT". Since the filter dropped placeables, a base
			// carrying only sandbags, lights and furniture reports this on its first interval, so the
			// player sees a sabotage objective succeed against a base that visibly still has things on it.
			// That is the accepted price of not demolishing clutter; the log line below is the only trace.
			CompleteMission("there was nothing left to demolish");
			return;
		}

		IEntity target = m_aTargets[index];
		if (!target)
			return;

		NotifyOnce(base);

		OVT_DeploymentLog.Debug(string.Format("[Overthrow] Sabotage: demolishing a structure worth %1 at %2",
			m_aTargetCosts[index], target.GetOrigin().ToString()));

		// ⚠ RUIN FIRST, REMOVE ONLY IF IT CANNOT BE RUINED. A retrofitted structure stays in the world as
		// wreckage the resistance can repair; anything without a destruction component is removed exactly
		// the way it always was. DestroyPlacedItem stays the only path that takes a structure out of the
		// world - see the class header.
		if (!OVT_StructureDamage.Ruin(target))
			resistance.DestroyPlacedItem(target);

		m_iDestroyed = m_iDestroyed + 1;

		if (m_iDestroyed >= ResolveStructuresPerMission())
		{
			CompleteMission("the mission met its quota");
			return;
		}

		// More to take: start the next interval. The team has to keep holding the base for it.
		m_iTicksLeft = ResolveIntervalTicks();
	}

	//------------------------------------------------------------------------------------------------
	//! Finds every player structure this mission may take, cheapest-first ordering supplied separately.
	//!
	//! THE SHAPE IS OVT_ItemLimitChecker.CountItemsForLocation, COLLECTING INSTEAD OF COUNTING and with
	//! the filter narrowed to buildables - there is no registry of placed structures anywhere in the
	//! tree, so a sphere query is the only enumerator that exists. The radius matches that method's own
	//! figure for EOVTBaseType.BASE (500 m): the two have to agree, because a structure the placement
	//! limit counted towards this base and this module could not find would be an object the player was
	//! charged for and can never lose.
	//! \param[in] base The base being sabotaged.
	//! \param[in] resistance The manager that prices a structure, cached for the callback.
	protected void CollectTargets(notnull OVT_BaseData base, notnull OVT_ResistanceFactionManager resistance)
	{
		if (!m_aTargets)
		{
			m_aTargets = new array<IEntity>();
			m_aTargetCosts = new array<int>();
		}

		m_aTargets.Clear();
		m_aTargetCosts.Clear();

		m_sTargetBaseId = base.id.ToString();
		m_iTargetBaseFaction = base.faction;
		m_iMyFaction = m_ParentDeployment.GetControllingFaction();
		m_QueryResistance = resistance;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		world.QueryEntitiesBySphere(
			base.location,
			m_fSearchRadius,
			CollectTargetCallback,
			FilterStructureCallback,
			EQueryEntitiesFlags.ALL);

		m_QueryResistance = null;
	}

	//------------------------------------------------------------------------------------------------
	//! WHETHER A STRUCTURE IS THE KIND OF THING A SABOTAGE TEAM DEMOLISHES AT ALL, with both inputs
	//! passed in so the rule is answerable without a world, a prefab or a spawned entity.
	//!
	//! ⚠ A BUILDABLE IS A TARGET; A PLACEABLE IS NOT. This is the whole rule, and it is a GAMEPLAY
	//! decision rather than a technical one - see the class header. A placeable is sandbags, a sign, a
	//! lamp, a chair; blowing one up is not an attack on the base, it is vandalism, and because
	//! placeables are all cheaper than every buildable the cost sort meant they were the only thing
	//! that ever came down.
	//!
	//! ⚠ AN ENTITY CARRYING BOTH COMPONENTS IS A TARGET. Nothing shipped is authored that way, but a
	//! thing that can be BUILT is a built structure whatever else it also is, and the alternative -
	//! letting a placeable component veto a buildable one - would give a mod a way to make a
	//! structure permanently immune by adding a component that means nothing here.
	//! \param[in] hasPlaceable Whether the entity carries OVT_PlaceableComponent.
	//! \param[in] hasBuildable Whether the entity carries OVT_BuildableComponent.
	//! \return True when the structure may be considered at all.
	static bool IsCandidateStructure(bool hasPlaceable, bool hasBuildable)
	{
		return hasBuildable;
	}

	//------------------------------------------------------------------------------------------------
	//! Filter callback - the OVT_ItemLimitChecker.FilterItemCallback shape, narrowed to buildables.
	//! \param[in] entity The entity being offered.
	//! \return True to pass it to the collect callback.
	protected bool FilterStructureCallback(IEntity entity)
	{
		if (!entity)
			return false;

		bool hasPlaceable = OVT_PlaceableComponent.Cast(entity.FindComponent(OVT_PlaceableComponent)) != null;
		bool hasBuildable = OVT_BuildableComponent.Cast(entity.FindComponent(OVT_BuildableComponent)) != null;

		return IsCandidateStructure(hasPlaceable, hasBuildable);
	}

	//------------------------------------------------------------------------------------------------
	//! Collect callback - keeps the structures IsSabotageTarget accepts, with their authored costs.
	//! \param[in] entity The entity that passed the filter.
	//! \return Always true, to keep searching.
	protected bool CollectTargetCallback(IEntity entity)
	{
		if (!entity)
			return true;

		// ⚠ THE BUILDABLE COMPONENT IS RE-RESOLVED RATHER THAN TRUSTED FROM THE FILTER. The filter is a
		// separate callback with no channel back to this one, and an entity that reaches here without it
		// is simply not this mission's business.
		OVT_BuildableComponent buildable = OVT_BuildableComponent.Cast(entity.FindComponent(OVT_BuildableComponent));
		if (!buildable)
			return true;

		string associatedId = buildable.GetAssociatedBaseId();
		EOVTBaseType associatedType = buildable.GetBaseType();

		if (!IsSabotageTarget(associatedId, associatedType, m_sTargetBaseId, m_iTargetBaseFaction, m_iMyFaction))
			return true;

		// 🔴 THE PLAYER'S GEAR IS NOT COLLATERAL. See IsGearContainer() - a deliberate carve-out pending
		// a gear-recovery mechanic, not a bug in the cost sort. Excluded HERE rather than in
		// FilterStructureCallback() so the exclusion sits beside the target rule it qualifies, where the
		// next person reading "what may this mission take" will find it.
		if (IsGearContainer(entity))
			return true;

		// ⚠ A RUIN IS NOT A TARGET. Demolishing one again would burn a mission interval and a quota slot
		// on a structure that is already down, and a base whose only buildable is a ruin would never
		// report "nothing left to demolish".
		if (OVT_StructureDamage.IsRuined(entity))
			return true;

		if (!m_QueryResistance)
			return true;

		m_aTargets.Insert(entity);
		m_aTargetCosts.Insert(m_QueryResistance.GetStructureCost(entity));

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Tells the resistance, ONCE PER MISSION AND NOT ONCE PER STRUCTURE.
	//!
	//! ⚠ A DELIBERATE ADDITION BEYOND THE LETTER OF THE REQUIREMENTS (D12), justified by the legibility
	//! bar: a recruitment tent silently vanishing while the player is on the other side of the map is
	//! the opposite of a ramp anyone can read. It names the BASE and nothing else - not the attacker,
	//! not the objective, not the forward base, which stays silent by explicit requirement - so it
	//! creates no intel surface.
	//!
	//! ⚠ BROADCAST (playerId -1). There is no faction-scoped send on the notification manager, and a
	//! structure being demolished is public knowledge in a campaign the resistance is running.
	//! \param[in] base The base being sabotaged.
	protected void NotifyOnce(notnull OVT_BaseData base)
	{
		if (m_bNotified)
			return;

		m_bNotified = true;

		OVT_NotificationManagerComponent notify = OVT_Global.GetNotify();
		if (!notify)
			return;

		notify.SendTextNotification(SABOTAGE_NOTIFICATION, -1, ResolveBaseName(base));
	}

	//------------------------------------------------------------------------------------------------
	//! A display name for the base, resolved SAFELY.
	//!
	//! The base marker's own name first, then the current objective's label (a base objective's name is
	//! this base's), then empty. ⚠ It deliberately does NOT call OVT_TownManagerComponent.GetTownName(),
	//! which dereferences a town's map marker without checking it is there - the hazard the director's
	//! own name resolution documents avoiding.
	//! \param[in] base The base.
	//! \return Its name, or an empty string.
	protected string ResolveBaseName(notnull OVT_BaseData base)
	{
		IEntity marker = GetGame().GetWorld().FindEntityByID(base.entId);
		if (marker)
		{
			OVT_BaseControllerComponent controller = OVT_BaseControllerComponent.Cast(marker.FindComponent(OVT_BaseControllerComponent));
			if (controller && controller.m_sName != "")
				return controller.m_sName;
		}

		OVT_ObjectiveDirectorComponent director = OVT_ObjectiveDirectorComponent.GetInstance();
		if (director)
			return director.GetObjectiveName();

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Stands the mission down WITHOUT reporting it as a success.
	//!
	//! ⚠ "THE MISSION IS OVER" AND "THE MISSION SUCCEEDED" ARE TWO DIFFERENT STATEMENTS, and only
	//! CompleteMission() makes the second. This one makes the first: it stops the module demolishing
	//! anything further and does nothing else - no director call, so no sabotage success is banked, and
	//! no deferred collection, so the deployment's own condition modules decide its fate.
	//!
	//! ⚠ IT HAS NO PRODUCTION CALLER TODAY AND STILL EARNS ITS PLACE. Anything that ever has to cancel
	//! an in-flight mission - a teardown, an objective reset, a Game Master intervention, an
	//! initialisation-tier case arranging the stopped state - will reach for the nearest thing that
	//! stops the module, and the nearest thing must not be CompleteMission(): banking a success for a
	//! mission that was cancelled would advance the counter-attack gate for work nobody did.
	void AbortMission()
	{
		m_bMissionReported = true;
	}

	//------------------------------------------------------------------------------------------------
	//! The success path: tell the director, and stand the operation down.
	//! \param[in] reason Why the mission ended, for the log.
	protected void CompleteMission(string reason)
	{
		// Latch BEFORE anything else, so a re-entrant path through the director or the deployment
		// framework cannot find the latch still down and demolish one more.
		AbortMission();

		OVT_DeploymentLog.Debug(string.Format("[Overthrow] Sabotage mission complete after %1 structure(s): %2",
			m_iDestroyed, reason));

		// 🔴 IT REPORTS INTO THE OBJECTIVE'S BAG AND DOES NOTHING ELSE. The signal is PULLED by the
		// director's tick, which compares the counter against a mark; a report that advanced a phase or
		// re-armed a timer would put a transition somewhere other than behind the tick's three early
		// returns. See OVT_ObjectiveDirectorComponent.ReportObjectiveProgress().
		OVT_ObjectiveDirectorComponent director = OVT_ObjectiveDirectorComponent.GetInstance();
		if (director)
			director.ReportObjectiveProgress(OVT_ObjectiveInstance.BAG_SABOTAGE_SUCCESSES, 1);

		// The operation is over. Collected NEXT FRAME rather than here - see
		// OVT_BaseBehaviorDeploymentModule.RequestDeploymentCollection for why an inline delete from a
		// non-last behavior module walks off the end of the module list it is being iterated in.
		RequestDeploymentCollection("a sabotage mission completed at a base objective");
	}

	//------------------------------------------------------------------------------------------------
	//! The base this deployment is working on, or null when there is nothing valid under it.
	//!
	//! ⚠ THE RANGE TEST IS NOT OPTIONAL. GetNearestBase() answers with the nearest base at ANY distance,
	//! so without it a team that ended up somewhere unexpected would start demolishing the nearest base
	//! on the map. A base objective is created at the base's own position, so m_fMaxBaseDistance only
	//! has to survive a spawn nudge.
	//! \return The base, or null.
	protected OVT_BaseData ResolveTargetBase()
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
			return null;

		vector deploymentPos = m_ParentDeployment.GetPosition();

		OVT_BaseData base = occupying.GetNearestBase(deploymentPos);
		if (!base)
			return null;

		if (vector.Distance(deploymentPos, base.location) > m_fMaxBaseDistance)
			return null;

		// The occupying faction does not strip a base it already holds - see IsSabotageTarget.
		if (base.faction == m_ParentDeployment.GetControllingFaction())
			return null;

		return base;
	}

	//------------------------------------------------------------------------------------------------
	//! How many updates one demolition interval is worth: the authored attribute, or the campaign's
	//! setting when the attribute is the -1 sentinel. See m_iHoldSeconds for the convention and for why
	//! it was flipped.
	//!
	//! PUBLIC so an initialisation-tier case can assert the precedence and the clamp against the
	//! campaign's real authored values - the precedent is OVT_DeploymentManagerComponent making
	//! ApplyObjectiveAnchorBias public for the same reason. Driving the whole module instead would mean
	//! standing up a deployment, a base and a sphere query full of real structures.
	//! \return At least one update.
	int ResolveIntervalTicks()
	{
		int difficultyValue = -1;

		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if (difficulty)
			difficultyValue = difficulty.objectiveSabotageHoldSeconds;

		int seconds = OVT_ObjectivePlanRules.ResolveWithDifficulty(m_iHoldSeconds, difficultyValue);

		if (seconds < UPDATE_SECONDS)
			return 1;

		return seconds / UPDATE_SECONDS;
	}

	//------------------------------------------------------------------------------------------------
	//! How many structures this mission may take: the authored attribute, or the campaign's setting when
	//! the attribute is the -1 sentinel.
	//!
	//! PUBLIC for the same reason ResolveIntervalTicks() is. The clamp is defensive only - the quota is
	//! tested AFTER a demolition, so a zero and a one behave identically - but an authored or preset
	//! zero should read as "one per mission" rather than as an accident that happens to work.
	//! \return At least one.
	int ResolveStructuresPerMission()
	{
		int difficultyValue = -1;

		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if (difficulty)
			difficultyValue = difficulty.objectiveSabotageStructuresPerMission;

		int count = OVT_ObjectivePlanRules.ResolveWithDifficulty(m_iStructuresPerMission, difficultyValue);

		if (count < 1)
			return 1;

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! EVERY attribute has to appear here, and the latches, the counters and the armed timer must NOT:
	//! a clone belongs to a different deployment and has demolished nothing.
	//!
	//! What a dropped line would cost here: drop m_fClearRadius and it clones as 0, so nothing ever
	//! counts as holding the base and no base is ever sabotaged - a base objective that ramps to
	//! nothing; drop m_fSearchRadius and the query finds no structures, so every mission reports "there
	//! was nothing left to demolish" on its first interval and the gate opens without a single thing
	//! being destroyed; drop m_iStructuresPerMission and a world with no difficulty settings loaded
	//! takes one structure per mission instead of the authored two.
	override OVT_BaseDeploymentModule CloneModule()
	{
		OVT_BaseSabotageBehaviorDeploymentModule clone = new OVT_BaseSabotageBehaviorDeploymentModule();

		clone.m_sModuleName = m_sModuleName;
		clone.m_fClearRadius = m_fClearRadius;
		clone.m_fSearchRadius = m_fSearchRadius;
		clone.m_fMaxBaseDistance = m_fMaxBaseDistance;
		clone.m_iHoldSeconds = m_iHoldSeconds;
		clone.m_iStructuresPerMission = m_iStructuresPerMission;

		return clone;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Whether this mission has already reported to the director.
	bool HasMissionReported()
	{
		return m_bMissionReported;
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many structures this mission has destroyed.
	int GetDestroyedCount()
	{
		return m_iDestroyed;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Updates still owed on the current interval, or 0 before the first update arms it.
	int GetIntervalTicksLeft()
	{
		return m_iTicksLeft;
	}

	//------------------------------------------------------------------------------------------------
	void PrintDebugInfo()
	{
		Print(string.Format("Base Sabotage Behavior Module: %1", m_sModuleName));
		Print(string.Format("  Clear Radius: %1m  Search Radius: %2m", m_fClearRadius, m_fSearchRadius));
		Print(string.Format("  Interval: %1 update(s) left  Destroyed: %2", m_iTicksLeft, m_iDestroyed));
		string reported = "No";
		if (m_bMissionReported)
			reported = "Yes";
		Print(string.Format("  Mission Reported: %1", reported));
	}
}
