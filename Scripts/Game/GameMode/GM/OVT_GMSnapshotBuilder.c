//------------------------------------------------------------------------------------------------
//! SERVER-SIDE walk of the campaign's managers into the four per-entity record arrays that
//! OVT_GMRequestComponent fans to one Game Master.
//!
//! !! IT IS STRICTLY READ-ONLY, AND THAT IS THE WHOLE POINT OF THE CLASS.
//! Every call it makes is a getter or a pure query. It assigns nothing on any manager, and it calls
//! no method that accumulates, allocates, spends or spawns. The trap this rule exists for is real and
//! is documented in the plan: OVT_OccupyingFactionManager.GainResources() computes the next
//! distribution AND THEN ADDS IT, so a "predict the next tick" call from here would hand the occupying
//! faction a free income tick every time a GM opened the editor. The same shape of mistake is
//! available on the base upgrades (Spend/SpendToAllocation) and on the deployment manager
//! (AllocateDeploymentResources). If a future field needs a number that only a mutating method
//! produces, EXTRACT THE ARITHMETIC into a pure static - do not call the mutator.
//!
//! THE ONE PERMITTED MUTATION is OVT_GMGroupRegistry.Sweep(), which drops registry entries whose
//! group entity no longer exists. That is the registry's own bookkeeping, not campaign state: it
//! changes nothing a player or the campaign can observe, and it is what makes "tag at spawn, never
//! untag" correct (the alternative was 27 untag sites in code that already leaks in three documented
//! ways). It has to happen HERE, immediately before the registry walk, so a GM never sees a record
//! for a dead group.
//!
//! WHY IT IS NOT ON THE COMPONENT. OVT_ControllerRequestComponent's stated rule is that a request
//! component is a thin seam onto a fat manager and carries no domain state. This walk is the fat part.
//! It is a plain class rather than a manager because it has no entity, no replication, no config
//! surface and exactly one caller.
//!
//! IT IS ALLOCATED ONCE AND REUSED. Build() clears its arrays rather than replacing them, so a poll
//! every 8 seconds per GM does not churn the allocator.
//------------------------------------------------------------------------------------------------
class OVT_GMSnapshotBuilder
{
	//! One record per occupying-faction base, index-aligned with nothing - the base index is IN the
	//! record.
	ref array<ref OVT_GMBaseRecord> m_aBases = new array<ref OVT_GMBaseRecord>();

	//! One record per NON-EMPTY base upgrade. See BuildBases() for what "non-empty" buys.
	ref array<ref OVT_GMBaseUpgradeRecord> m_aBaseUpgrades = new array<ref OVT_GMBaseUpgradeRecord>();

	//! One record per active deployment that has a resolvable RplComponent.
	ref array<ref OVT_GMDeploymentRecord> m_aDeployments = new array<ref OVT_GMDeploymentRecord>();

	//! One record per tagged, still-alive AI group that has a resolvable RplComponent.
	ref array<ref OVT_GMGroupRecord> m_aGroups = new array<ref OVT_GMGroupRecord>();

	//! Records emitted by the last Build(), across all four classes.
	protected int m_iRecordCount;

	//! The cap for the build currently running, from OVT_GMRequestComponent.m_iMaxRecordsPerSnapshot.
	protected int m_iMaxRecords;

	//! The record class the cap first truncated, or "" when the build fit. Named in the warning.
	protected string m_sTruncatedClass;

	//! Scratch reused by the registry walk so a poll does not allocate two arrays per GM per 8s.
	protected ref array<EntityID> m_aGroupIds = new array<EntityID>();
	protected ref array<ref OVT_GMGroupOrigin> m_aGroupOrigins = new array<ref OVT_GMGroupOrigin>();

	//------------------------------------------------------------------------------------------------
	//! Walks every source into the record arrays.
	//!
	//! \param[in] maxRecords Most records this snapshot may carry, across all four classes. A build
	//! that hits it stops emitting and logs ONE warning - a silently truncated snapshot is a bug that
	//! looks exactly like missing data.
	//! \param[in] debugTiming When true, print the per-class counts and the build time in ms.
	//! \return Total records emitted, which is what SnapshotEnd's recordCount is derived from.
	int Build(int maxRecords, bool debugTiming)
	{
		int startTick = System.GetTickCount();

		m_aBases.Clear();
		m_aBaseUpgrades.Clear();
		m_aDeployments.Clear();
		m_aGroups.Clear();

		m_iRecordCount = 0;
		m_sTruncatedClass = "";
		m_iMaxRecords = maxRecords;
		if(m_iMaxRecords <= 0) m_iMaxRecords = int.MAX;

		BuildBases();
		BuildDeployments();
		BuildGroups();

		if(m_sTruncatedClass != "")
		{
			Print(string.Format("[Overthrow.GMSnapshot] Snapshot TRUNCATED at the %1 record cap while emitting %2 records - a Game Master is seeing an incomplete campaign. Raise OVT_GMRequestComponent.m_iMaxRecordsPerSnapshot or lengthen the poll interval. Emitted: %3 base(s), %4 upgrade(s), %5 deployment(s), %6 group(s)",
				m_iMaxRecords, m_sTruncatedClass, m_aBases.Count(), m_aBaseUpgrades.Count(), m_aDeployments.Count(), m_aGroups.Count()), LogLevel.WARNING);
		}

		if(debugTiming)
		{
			Print(string.Format("[Overthrow.GMSnapshot] Built %1 record(s) in %2ms: %3 base(s), %4 upgrade(s), %5 deployment(s), %6 group(s)",
				m_iRecordCount, System.GetTickCount() - startTick, m_aBases.Count(), m_aBaseUpgrades.Count(), m_aDeployments.Count(), m_aGroups.Count()));
		}

		return m_iRecordCount;
	}

	//------------------------------------------------------------------------------------------------
	//! Records emitted by the last Build().
	//! \return The total across all four arrays.
	int GetRecordCount()
	{
		return m_iRecordCount;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the last Build() hit the cap.
	//! \return True when at least one record was dropped.
	bool WasTruncated()
	{
		return m_sTruncatedClass != "";
	}

	//------------------------------------------------------------------------------------------------
	// SOURCES
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Bases and their upgrades, in ONE walk because the base aggregate is the sum of the upgrade rows.
	//!
	//! The base list is walked BY INDEX: the positional index into m_Bases is the join key clients
	//! already hold through the occupying faction's JIP stream, and OVT_BaseData.id is NOT it.
	//!
	//! THE NON-EMPTY FILTER. An upgrade row is emitted only when the upgrade holds resources or fields
	//! groups. A base runs every upgrade in its config from the first tick, most of them at zero until
	//! the occupying faction has spent enough on them, so this roughly halves the largest record class
	//! for free and costs a consumer nothing: the base record's m_iUpgrades still counts ALL of them,
	//! so "12 upgrades, 4 rows" reads as "8 of them are empty", not as a truncated fan.
	//!
	//! GetNumGroups() lives on OVT_BasePatrolUpgrade, not on OVT_BaseUpgrade, so it is reached through
	//! a cast. Every group-fielding upgrade class in the tree derives from it (defence patrol, defence
	//! position, sniper position, tower guard, town patrol, specops, checkpoints, slotted/composition);
	//! the classes that do not - parked vehicles - genuinely field no groups and correctly report 0.
	protected void BuildBases()
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if(!occupying) return;

		BaseWorld world = GetGame().GetWorld();
		if(!world) return;

		for(int i = 0; i < occupying.m_Bases.Count(); i++)
		{
			OVT_BaseData data = occupying.m_Bases[i];
			if(!data) continue;

			// Resolved here rather than through GetBase(), which dereferences the marker entity
			// without a null check.
			IEntity marker = world.FindEntityByID(data.entId);
			if(!marker) continue;

			OVT_BaseControllerComponent controller = OVT_BaseControllerComponent.Cast(marker.FindComponent(OVT_BaseControllerComponent));
			if(!controller) continue;
			if(!controller.m_aBaseUpgrades) continue;

			int baseResources = 0;
			int baseGroups = 0;

			foreach(OVT_BaseUpgrade upgrade : controller.m_aBaseUpgrades)
			{
				if(!upgrade) continue;

				int resources = upgrade.GetResources();

				int groups = 0;
				OVT_BasePatrolUpgrade patrol = OVT_BasePatrolUpgrade.Cast(upgrade);
				if(patrol) groups = patrol.GetNumGroups();

				baseResources += resources;
				baseGroups += groups;

				if(resources <= 0 && groups <= 0) continue;

				if(!TakeBudget("OVT_GMBaseUpgradeRecord")) return;

				OVT_GMBaseUpgradeRecord upgradeRecord = new OVT_GMBaseUpgradeRecord();
				upgradeRecord.m_iBaseIndex = i;
				upgradeRecord.m_sType = upgrade.ClassName();
				upgradeRecord.m_iResources = resources;
				upgradeRecord.m_iGroups = groups;
				m_aBaseUpgrades.Insert(upgradeRecord);
			}

			if(!TakeBudget("OVT_GMBaseRecord")) return;

			OVT_GMBaseRecord baseRecord = new OVT_GMBaseRecord();
			baseRecord.m_iBaseIndex = i;
			baseRecord.m_iResources = baseResources;
			baseRecord.m_iGroups = baseGroups;
			baseRecord.m_iUpgrades = controller.m_aBaseUpgrades.Count();
			m_aBases.Insert(baseRecord);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Active deployments.
	//!
	//! GetAllDeployments() is the manager's own public read accessor over m_aActiveDeployments; it
	//! already drops ids whose entity has gone. A deployment with no RplComponent cannot be NAMED
	//! across the network at all, so it is skipped silently rather than sent with a meaningless key -
	//! a record a consumer can never match to anything on screen is worse than no record.
	protected void BuildDeployments()
	{
		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if(!manager) return;

		array<OVT_DeploymentComponent> deployments = manager.GetAllDeployments();
		if(!deployments) return;

		foreach(OVT_DeploymentComponent deployment : deployments)
		{
			if(!deployment) continue;

			IEntity owner = deployment.GetOwner();
			if(!owner) continue;

			RplComponent rpl = RplComponent.Cast(owner.FindComponent(RplComponent));
			if(!rpl) continue;

			if(!TakeBudget("OVT_GMDeploymentRecord")) return;

			OVT_GMDeploymentRecord record = new OVT_GMDeploymentRecord();
			record.m_RplId = rpl.Id();
			record.m_sName = deployment.GetDeploymentName();
			record.m_iFaction = deployment.GetControllingFaction();
			record.m_iResourcesInvested = deployment.GetResourcesInvested();
			record.m_bActive = deployment.IsDeploymentActive();
			m_aDeployments.Insert(record);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Tagged AI groups, swept first.
	//!
	//! Sweep() is the ONE mutation this class performs and it is registry bookkeeping (see the class
	//! header). It runs immediately before the walk so a dead group cannot produce a record, and it is
	//! the only removal path the registry has - there are no untag call sites anywhere.
	//!
	//! A group with no RplComponent is skipped silently. Should that turn out to be the common case
	//! rather than the exception it is a FINDING about the epic's join-key contract, not something to
	//! work around here - the fallback (position matching) is a re-plan.
	protected void BuildGroups()
	{
		OVT_GMGroupRegistry registry = OVT_GMGroupRegistry.GetInstance();
		if(!registry) return;

		registry.Sweep();

		registry.GetAll(m_aGroupIds, m_aGroupOrigins);

		BaseWorld world = GetGame().GetWorld();
		if(!world) return;

		for(int i = 0; i < m_aGroupIds.Count(); i++)
		{
			OVT_GMGroupOrigin origin = m_aGroupOrigins[i];
			if(!origin) continue;

			IEntity group = world.FindEntityByID(m_aGroupIds[i]);
			if(!group) continue;

			RplComponent rpl = RplComponent.Cast(group.FindComponent(RplComponent));
			if(!rpl) continue;

			if(!TakeBudget("OVT_GMGroupRecord")) return;

			OVT_GMGroupRecord record = new OVT_GMGroupRecord();
			record.m_RplId = rpl.Id();
			record.m_iOriginType = origin.m_iType;
			record.m_iOriginIndex = origin.m_iIndex;
			record.m_sReason = origin.m_sReason;
			m_aGroups.Insert(record);
		}
	}

	//------------------------------------------------------------------------------------------------
	// BUDGET
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Claims one record's worth of the snapshot budget.
	//!
	//! The FIRST class to be refused is the one named in the warning, because that is the one a reader
	//! will find missing; later refusals are silent so a badly over-budget campaign produces one log
	//! line per build and not one per record.
	//! \param[in] recordClass Name of the record class being emitted.
	//! \return True when the record may be emitted.
	protected bool TakeBudget(string recordClass)
	{
		if(m_iRecordCount >= m_iMaxRecords)
		{
			if(m_sTruncatedClass == "") m_sTruncatedClass = recordClass;

			return false;
		}

		m_iRecordCount += 1;

		return true;
	}
}
