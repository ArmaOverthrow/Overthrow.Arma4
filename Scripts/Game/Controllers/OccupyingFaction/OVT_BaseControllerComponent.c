class OVT_BaseControllerComponentClass: OVT_ComponentClass
{
};

//------------------------------------------------------------------------------------------------
//! The marker component on a military base: WHERE ITS BUILDABLE GROUND IS, who holds it, and where a
//! QRF should come from.
//!
//! ⚠ IT NO LONGER BUYS, SPENDS, TICKS OR SPAWNS ANYTHING. Until the base-defense migration this
//! component also owned a whole parallel economy - a runtime list of upgrade objects copied from a
//! config, a 10 s timer that ticked them, a 1..19 priority sweep the occupying faction handed a
//! per-base budget to, and a by-class-name lookup the save path replayed old records into. All of it
//! is gone: base defense is nine Configs/Deployment/Deployment_Base*.conf deployments, bought out of
//! the deployment framework's own resource pool and virtualized by the core, and NOTHING may
//! reintroduce a second spender here (that is decision G2 - one accounting path, grep-enforced).
//!
//! WHAT SURVIVES, AND WHY IT IS STILL THE ONLY PLACE THAT KNOWS IT:
//!  - THE SLOT REGISTRY (m_AllSlots / m_AllCloseSlots / the six sized lists / m_Parking /
//!    m_aSlotsFilled / m_aDefendPositions / m_aVehiclePatrolSpawns). This is discovered by one world
//!    query per base at init and read by the composition, parked-vehicle and defend-position
//!    deployment modules, by the resistance's FOB garrison path and by QRF placement. m_aSlotsFilled
//!    in particular ROUND-TRIPS THROUGH THE SAVE and is what stops a deployment re-using a slot a
//!    structure is already standing in;
//!  - the faction/flag half, which is what a capture actually changes;
//!  - the QRF spawn geometry attributes, read by OVT_QRFControllerComponent.
//------------------------------------------------------------------------------------------------
class OVT_BaseControllerComponent: OVT_Component
{
	[Attribute("")]
	string m_sName;
	
	[Attribute(defvalue: "1", UIWidgets.EditBox, desc: "Initial Resource Multiplier")]
	float m_fStartingResourcesMultiplier;

	[Attribute("400", UIWidgets.Slider, "Minimum distance to spawn QRF", "50 1000 25")]
	int m_iAttackDistanceMin;
	
	[Attribute("800", UIWidgets.Slider, "Maximum distance to spawn QRF", "100 1000 25")]
	int m_iAttackDistanceMax;
	
	[Attribute("-1", UIWidgets.Slider, "Preferred direction to spawn QRF (randomized slightly, -1 means any direction)", "-1 359 1")]
	int m_iAttackPreferredDirection;
	
	[Attribute("30", UIWidgets.Slider, "Direction variance in degrees (QRF can spawn within +/- this many degrees from preferred direction)", "0 180 5")]
	int m_iAttackDirectionVariance;

	ref array<ref EntityID> m_AllSlots;
	ref array<ref EntityID> m_AllCloseSlots;
	ref array<ref EntityID> m_SmallSlots;
	ref array<ref EntityID> m_MediumSlots;
	ref array<ref EntityID> m_LargeSlots;
	ref array<ref EntityID> m_SmallRoadSlots;
	ref array<ref EntityID> m_MediumRoadSlots;
	ref array<ref EntityID> m_LargeRoadSlots;
	ref array<ref EntityID> m_Parking;
	ref array<ref EntityID> m_aSlotsFilled;
	ref array<ref vector> m_aDefendPositions;
	ref array<ref EntityID> m_aVehiclePatrolSpawns;

	protected OVT_OccupyingFactionManager m_occupyingFactionManager;

	void InitBaseClient()
	{
		if(Replication.IsServer()) return;
		SCR_FactionAffiliationComponent affiliation = OVT_ComponentFinder<SCR_FactionAffiliationComponent>.Find(GetOwner());
		if(affiliation)
		{
			affiliation.GetOnFactionChanged().Insert(OnFactionChanged);
		}
	}
		
	void InitBase()
	{
		if(!Replication.IsServer()) return;
		if (SCR_Global.IsEditMode()) return;

		m_occupyingFactionManager = OVT_Global.GetOccupyingFaction();
		
		SCR_FactionAffiliationComponent affiliation = OVT_ComponentFinder<SCR_FactionAffiliationComponent>.Find(GetOwner());
		if(affiliation)
		{
			affiliation.GetOnFactionChanged().Insert(OnFactionChanged);
		}

		InitializeBase();
	}
	
	OVT_BaseData GetData()
	{
		OVT_OccupyingFactionManager of = OVT_Global.GetOccupyingFaction();
		return of.GetNearestBase(GetOwner().GetOrigin());
	}

	void OnFactionChanged(FactionAffiliationComponent owner, Faction previousFaction, Faction newFaction)
	{
		// Get the faction index
		FactionManager factionManager = GetGame().GetFactionManager();
		int factionIndex = factionManager.GetFactionIndex(newFaction);
						
		// Update flag
		UpdateFlagMaterial(factionIndex);	
	}
	
	void UpdateFlagMaterial(int factionIndex)
	{
		FactionManager factionManager = GetGame().GetFactionManager();
		Faction faction = factionManager.GetFactionByIndex(factionIndex);
		if (!faction)
			return;
		
		SCR_Faction scrFaction = SCR_Faction.Cast(faction);
		if (!scrFaction)
			return;
		
		SCR_FlagComponent flag = OVT_ComponentFinder<SCR_FlagComponent>.Find(GetOwner());
		if (!flag)
			return;
		
		flag.ChangeMaterial(scrFaction.GetFactionFlagMaterial());
	}

	bool IsOccupyingFaction()
	{
		SCR_FactionAffiliationComponent affiliation = OVT_ComponentFinder<SCR_FactionAffiliationComponent>.Find(GetOwner());
		Faction occupyingFactionData = OVT_Global.GetConfig().GetOccupyingFactionData();
		FactionKey occupyingFaction = occupyingFactionData.GetFactionKey();
		
		Faction affiliatedFactionData = affiliation.GetAffiliatedFaction();
		FactionKey affiliatedFaction = affiliatedFactionData.GetFactionKey();
		return affiliatedFaction == occupyingFaction;
	}

	int GetControllingFaction()
	{
		SCR_FactionAffiliationComponent affiliation = OVT_ComponentFinder<SCR_FactionAffiliationComponent>.Find(GetOwner());

		return GetGame().GetFactionManager().GetFactionIndex(affiliation.GetAffiliatedFaction());
	}

	void SetControllingFaction(string key, bool suppressEvents = false)
	{
		FactionManager mgr = GetGame().GetFactionManager();
		Faction faction = mgr.GetFactionByKey(key);
		int index = mgr.GetFactionIndex(faction);
		SetControllingFaction(index, suppressEvents);
	}

	void SetControllingFaction(int index, bool suppressEvents = false)
	{
		if(!suppressEvents)
			m_occupyingFactionManager.OnBaseControlChange(this);

		Faction fac = GetGame().GetFactionManager().GetFactionByIndex(index);
		SCR_FactionAffiliationComponent affiliation = OVT_ComponentFinder<SCR_FactionAffiliationComponent>.Find(GetOwner());
		affiliation.SetAffiliatedFaction(fac);
	}

	void InitializeBase()
	{
		m_AllSlots = new array<ref EntityID>;
		m_AllCloseSlots = new array<ref EntityID>;
		m_SmallSlots = new array<ref EntityID>;
		m_MediumSlots = new array<ref EntityID>;
		m_LargeSlots = new array<ref EntityID>;
		m_SmallRoadSlots = new array<ref EntityID>;
		m_MediumRoadSlots = new array<ref EntityID>;
		m_LargeRoadSlots = new array<ref EntityID>;
		m_Parking = new array<ref EntityID>;
		m_aSlotsFilled = new array<ref EntityID>;
		m_aDefendPositions = new array<ref vector>;
		m_aVehiclePatrolSpawns = new array<ref EntityID>;

		FindSlots();
		FindParking();
	}

	void FindSlots()
	{
		GetGame().GetWorld().QueryEntitiesBySphere(GetOwner().GetOrigin(),  OVT_Global.GetConfig().m_Difficulty.baseRange, CheckSlotAddToArray, FilterSlotEntities);
	}

	bool FilterSlotEntities(IEntity entity)
	{
		OVT_VehiclePatrolSpawn vehicleSpawn = OVT_VehiclePatrolSpawn.Cast(entity);
		if(vehicleSpawn)
		{
			m_aVehiclePatrolSpawns.Insert(entity.GetID());
			return true;
		}
		
		SCR_EditableEntityComponent editable = OVT_ComponentFinder<SCR_EditableEntityComponent>.Find(entity);
		if(editable && editable.GetEntityType() == EEditableEntityType.SLOT)
		{
			return true;
		}

		SCR_AISmartActionSentinelComponent action = OVT_ComponentFinder<SCR_AISmartActionSentinelComponent>.Find(entity);
		if(action) {
			SCR_MapDescriptorComponent mapdes = OVT_ComponentFinder<SCR_MapDescriptorComponent>.Find(entity);
			if(mapdes)
			{
				EMapDescriptorType type = mapdes.GetBaseType();
				//Towers are handled by OVT_TowerCoverPostPlacementProvider (Deployment_BaseTowerGuards.conf),
				//which finds them by map descriptor itself - a tower left in this sweep would be manned twice
				if(type == EMapDescriptorType.MDT_TOWER) return false;
			}
			return true;
		}
		return false;
	}

	bool CheckSlotAddToArray(IEntity entity)
	{
		SCR_AISmartActionSentinelComponent action = OVT_ComponentFinder<SCR_AISmartActionSentinelComponent>.Find(entity);
		if(action)
		{
			vector pos = entity.GetOrigin();
			if(!m_aDefendPositions.Contains(pos))
				m_aDefendPositions.Insert(entity.GetOrigin());
			return true;
		}

		SCR_EditableEntityComponent editable = OVT_ComponentFinder<SCR_EditableEntityComponent>.Find(entity);
		if(editable && editable.GetEntityType() == EEditableEntityType.SLOT)
		{
			SCR_EditableEntityUIInfo uiinfo = SCR_EditableEntityUIInfo.Cast(editable.GetInfo());
			if(!uiinfo) return true;

			m_AllSlots.Insert(entity.GetID());

			float distance = vector.Distance(entity.GetOrigin(), GetOwner().GetOrigin());
			if(distance <  OVT_Global.GetConfig().m_Difficulty.baseCloseRange)
			{
				m_AllCloseSlots.Insert(entity.GetID());
			}

			string name = entity.GetPrefabData().GetPrefabName();
			if(uiinfo.HasEntityLabel(EEditableEntityLabel.SLOT_FLAT_SMALL)) m_SmallSlots.Insert(entity.GetID());
			if(uiinfo.HasEntityLabel(EEditableEntityLabel.SLOT_FLAT_MEDIUM)) m_MediumSlots.Insert(entity.GetID());
			if(uiinfo.HasEntityLabel(EEditableEntityLabel.SLOT_FLAT_LARGE)) m_LargeSlots.Insert(entity.GetID());
			if(uiinfo.HasEntityLabel(EEditableEntityLabel.SLOT_ROAD_SMALL)) m_SmallRoadSlots.Insert(entity.GetID());
			if(uiinfo.HasEntityLabel(EEditableEntityLabel.SLOT_ROAD_MEDIUM)) m_MediumRoadSlots.Insert(entity.GetID());
			if(uiinfo.HasEntityLabel(EEditableEntityLabel.SLOT_ROAD_LARGE)) m_LargeRoadSlots.Insert(entity.GetID());
		}

		return true;
	}

	void FindParking()
	{
		GetGame().GetWorld().QueryEntitiesBySphere(GetOwner().GetOrigin(), OVT_Global.GetConfig().m_Difficulty.baseCloseRange, null, FilterParkingEntities, EQueryEntitiesFlags.ALL);
	}

	bool FilterParkingEntities(IEntity entity)
	{
		if(entity.FindComponent(OVT_ParkingComponent)) {
			m_Parking.Insert(entity.GetID());
		}
		return false;
	}

	IEntity GetNearestSlot(vector pos)
	{
		IEntity nearest;
		float nearestDist = -1;
		foreach(EntityID id : m_AllSlots)
		{
			IEntity ent = GetGame().GetWorld().FindEntityByID(id);
			float dist = vector.Distance(pos, ent.GetOrigin());
			if(nearestDist == -1 || dist < nearestDist)
			{
				nearest = ent;
				nearestDist = dist;
			}
		}
		return nearest;
	}

#ifdef WORKBENCH
	protected ref Shape m_aDirectionArrowCenter;
	protected ref Shape m_aDirectionArrowMin;
	protected ref Shape m_aDirectionArrowMax;
	
	//Draw attack preferred direction as arrows showing variance
	override int _WB_GetAfterWorldUpdateSpecs(IEntity owner, IEntitySource src)
	{
		return EEntityFrameUpdateSpecs.CALL_WHEN_ENTITY_SELECTED;
	}
	
	protected override void _WB_AfterWorldUpdate(IEntity owner, float timeSlice)
	{
		if (m_iAttackPreferredDirection != -1)
		{
			vector basePos = owner.GetOrigin();
			
			// Draw center arrow (main direction)
			float centerRad = m_iAttackPreferredDirection * Math.DEG2RAD;
			vector fromCenter = basePos + Vector(Math.Sin(centerRad) * m_iAttackDistanceMax, 0, -Math.Cos(centerRad) * m_iAttackDistanceMax);
			vector toCenter = basePos + Vector(Math.Sin(centerRad) * m_iAttackDistanceMin, 0, -Math.Cos(centerRad) * m_iAttackDistanceMin);
			m_aDirectionArrowCenter = Shape.CreateArrow(fromCenter, toCenter, 10, Color.FromRGBA(255, 0, 0, 255).PackToInt(), ShapeFlags.ONCE | ShapeFlags.NOZBUFFER | ShapeFlags.TRANSP | ShapeFlags.DOUBLESIDE | ShapeFlags.NOOUTLINE);
			
			// Draw variance arrows (showing the extremes)
			float minRad = (m_iAttackPreferredDirection - m_iAttackDirectionVariance) * Math.DEG2RAD;
			vector fromMin = basePos + Vector(Math.Sin(minRad) * m_iAttackDistanceMax, 0, -Math.Cos(minRad) * m_iAttackDistanceMax);
			vector toMin = basePos + Vector(Math.Sin(minRad) * m_iAttackDistanceMin, 0, -Math.Cos(minRad) * m_iAttackDistanceMin);
			m_aDirectionArrowMin = Shape.CreateArrow(fromMin, toMin, 6, Color.FromRGBA(255, 0, 0, 128).PackToInt(), ShapeFlags.ONCE | ShapeFlags.NOZBUFFER | ShapeFlags.TRANSP | ShapeFlags.DOUBLESIDE | ShapeFlags.NOOUTLINE);
			
			float maxRad = (m_iAttackPreferredDirection + m_iAttackDirectionVariance) * Math.DEG2RAD;
			vector fromMax = basePos + Vector(Math.Sin(maxRad) * m_iAttackDistanceMax, 0, -Math.Cos(maxRad) * m_iAttackDistanceMax);
			vector toMax = basePos + Vector(Math.Sin(maxRad) * m_iAttackDistanceMin, 0, -Math.Cos(maxRad) * m_iAttackDistanceMin);
			m_aDirectionArrowMax = Shape.CreateArrow(fromMax, toMax, 6, Color.FromRGBA(255, 0, 0, 128).PackToInt(), ShapeFlags.ONCE | ShapeFlags.NOZBUFFER | ShapeFlags.TRANSP | ShapeFlags.DOUBLESIDE | ShapeFlags.NOOUTLINE);
		}
		
		super._WB_AfterWorldUpdate(owner, timeSlice);
	}
#endif

	//RPC methods

	//------------------------------------------------------------------------------------------------
	//! Get a random vehicle patrol spawn point from the base
	//! Returns true if a spawn point was found, false if none exist
	//! @param[out] outPosition The spawn position
	//! @param[out] outAngles The spawn angles in format "yaw pitch roll"
	bool GetRandomVehiclePatrolSpawn(out vector outPosition, out vector outAngles)
	{
		// Check if we have any vehicle patrol spawns
		if (m_aVehiclePatrolSpawns.IsEmpty())
			return false;
		
		// Get a random spawn
		int randomIndex = Math.RandomInt(0, m_aVehiclePatrolSpawns.Count());
		EntityID spawnID = m_aVehiclePatrolSpawns[randomIndex];
		
		IEntity spawnEntity = GetGame().GetWorld().FindEntityByID(spawnID);
		if (!spawnEntity)
		{
			// Clean up invalid entity reference
			m_aVehiclePatrolSpawns.Remove(randomIndex);
			
			// Try again if we still have spawns
			if (!m_aVehiclePatrolSpawns.IsEmpty())
				return GetRandomVehiclePatrolSpawn(outPosition, outAngles);
			
			return false;
		}
		
		// Get position and angles
		outPosition = spawnEntity.GetOrigin();
		outAngles = spawnEntity.GetAngles();
		
		return true;
	}


}