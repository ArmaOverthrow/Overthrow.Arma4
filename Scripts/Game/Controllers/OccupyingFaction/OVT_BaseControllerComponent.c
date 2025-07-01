class OVT_BaseControllerComponentClass: OVT_ComponentClass
{
};

class OVT_BaseControllerComponent: OVT_Component
{
	[Attribute("")]
	string m_sName;
	
	[Attribute(defvalue: "1", UIWidgets.EditBox, desc: "Initial Resource Multiplier")]
	float m_fStartingResourcesMultiplier;

	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_BaseUpgrade> m_aBaseUpgrades;
	
	[Attribute("400", UIWidgets.Slider, "Minimum distance to spawn QRF", "50 1000 25")]
	int m_iAttackDistanceMin;
	
	[Attribute("800", UIWidgets.Slider, "Maximum distance to spawn QRF", "100 1000 25")]
	int m_iAttackDistanceMax;
	
	[Attribute("-1", UIWidgets.Slider, "Preferred direction to spawn QRF (randomized slightly, -1 means any direction)", "-1 359 1")]
	int m_iAttackPreferredDirection;

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

	protected OVT_OccupyingFactionManager m_occupyingFactionManager;

	protected const int UPGRADE_UPDATE_FREQUENCY = 10000;
	
	void InitBaseClient()
	{
		if(Replication.IsServer()) return;
		SCR_FactionAffiliationComponent affiliation = EPF_Component<SCR_FactionAffiliationComponent>.Find(GetOwner());
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
		
		SCR_FactionAffiliationComponent affiliation = EPF_Component<SCR_FactionAffiliationComponent>.Find(GetOwner());
		if(affiliation)
		{
			affiliation.GetOnFactionChanged().Insert(OnFactionChanged);
		}

		InitializeBase();

		GetGame().GetCallqueue().CallLater(UpdateUpgrades, UPGRADE_UPDATE_FREQUENCY, true, GetOwner());
	}
	
	OVT_BaseData GetData()
	{
		OVT_OccupyingFactionManager of = OVT_Global.GetOccupyingFaction();
		return of.GetNearestBase(GetOwner().GetOrigin());
	}

	protected void UpdateUpgrades()
	{
		if(!IsOccupyingFaction()) return;

		foreach(OVT_BaseUpgrade upgrade : m_aBaseUpgrades)
		{
			upgrade.OnUpdate(UPGRADE_UPDATE_FREQUENCY);
		}
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
		
		SCR_FlagComponent flag = EPF_Component<SCR_FlagComponent>.Find(GetOwner());
		if (!flag)
			return;
		
		flag.ChangeMaterial(scrFaction.GetFactionFlagMaterial());
	}

	bool IsOccupyingFaction()
	{
		SCR_FactionAffiliationComponent affiliation = EPF_Component<SCR_FactionAffiliationComponent>.Find(GetOwner());
		Faction occupyingFactionData = OVT_Global.GetConfig().GetOccupyingFactionData();
		FactionKey occupyingFaction = occupyingFactionData.GetFactionKey();
		
		Faction affiliatedFactionData = affiliation.GetAffiliatedFaction();
		FactionKey affiliatedFaction = affiliatedFactionData.GetFactionKey();
		return affiliatedFaction == occupyingFaction;
	}

	int GetControllingFaction()
	{
		SCR_FactionAffiliationComponent affiliation = EPF_Component<SCR_FactionAffiliationComponent>.Find(GetOwner());

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
		SCR_FactionAffiliationComponent affiliation = EPF_Component<SCR_FactionAffiliationComponent>.Find(GetOwner());
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

		FindSlots();
		FindParking();

		foreach(OVT_BaseUpgrade upgrade : m_aBaseUpgrades)
		{
			upgrade.Init(this, m_occupyingFactionManager, OVT_Global.GetConfig());
		}

	}

	OVT_BaseUpgrade FindUpgrade(string type, string tag = "")
	{
		foreach(OVT_BaseUpgrade upgrade : m_aBaseUpgrades)
		{
			if(tag != "")
			{
				OVT_BaseUpgradeComposition comp = OVT_BaseUpgradeComposition.Cast(upgrade);
				if(!comp) continue;
				if(comp.m_sCompositionTag == tag)
				{
					return upgrade;
				}else{
					continue;
				}
			}
			if(upgrade.ClassName() == type) return upgrade;
		}
		return null;
	}

	void FindSlots()
	{
		GetGame().GetWorld().QueryEntitiesBySphere(GetOwner().GetOrigin(),  OVT_Global.GetConfig().m_Difficulty.baseRange, CheckSlotAddToArray, FilterSlotEntities);
	}

	bool FilterSlotEntities(IEntity entity)
	{
		SCR_EditableEntityComponent editable = EPF_Component<SCR_EditableEntityComponent>.Find(entity);
		if(editable && editable.GetEntityType() == EEditableEntityType.SLOT)
		{
			return true;
		}

		SCR_AISmartActionSentinelComponent action = EPF_Component<SCR_AISmartActionSentinelComponent>.Find(entity);
		if(action) {
			SCR_MapDescriptorComponent mapdes = EPF_Component<SCR_MapDescriptorComponent>.Find(entity);
			if(mapdes)
			{
				EMapDescriptorType type = mapdes.GetBaseType();
				if(type == EMapDescriptorType.MDT_TOWER) return false; //Towers are handled by OVT_BaseUpgradeTowerGuard
			}
			return true;
		}
		return false;
	}

	bool CheckSlotAddToArray(IEntity entity)
	{
		SCR_AISmartActionSentinelComponent action = EPF_Component<SCR_AISmartActionSentinelComponent>.Find(entity);
		if(action)
		{
			vector pos = entity.GetOrigin();
			if(!m_aDefendPositions.Contains(pos))
				m_aDefendPositions.Insert(entity.GetOrigin());
			return true;
		}

		SCR_EditableEntityComponent editable = EPF_Component<SCR_EditableEntityComponent>.Find(entity);
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

	int SpendResources(int resources, float threat = 0)
	{
		int spent = 0;

		for(int priority = 1; priority < 20; priority++)
		{
			if(resources <= 0) break;
			foreach(OVT_BaseUpgrade upgrade : m_aBaseUpgrades)
			{
				if(resources <= 0) break;
				if(upgrade.m_iMinimumThreat > threat) continue;
				if(upgrade.m_iPriority == priority)
				{
					int allocate = upgrade.m_iResourceAllocation * OVT_Global.GetConfig().m_Difficulty.baseResourceCost;
					int newres = 0;
					if(allocate < 0)
					{
						//Ignore allocation, spend recklessly
						newres = upgrade.Spend(resources, threat);
					}else{
						if(resources < allocate) allocate = resources;
						newres = upgrade.SpendToAllocation(threat);
					}

					spent += newres;
					resources -= newres;
				}
			}
		}

		return spent;
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
	protected ref Shape m_aDirectionArrow;
	
	//Draw attack preferred direction as an arrow
	override int _WB_GetAfterWorldUpdateSpecs(IEntity owner, IEntitySource src)
	{
		return EEntityFrameUpdateSpecs.CALL_WHEN_ENTITY_SELECTED;
	}
	
	protected override void _WB_AfterWorldUpdate(IEntity owner, float timeSlice)
	{
		if (m_iAttackPreferredDirection != -1)
		{
			vector basePos = owner.GetOrigin();
			float directionRad = m_iAttackPreferredDirection * Math.DEG2RAD;
			
			// Calculate arrow start and end points
			vector from = basePos + Vector(Math.Cos(directionRad) * m_iAttackDistanceMax, 0, Math.Sin(directionRad) * m_iAttackDistanceMax);
			vector to = basePos + Vector(Math.Cos(directionRad) * m_iAttackDistanceMin, 0, Math.Sin(directionRad) * m_iAttackDistanceMin);
			
			// Draw arrow with semi-transparent red color
			m_aDirectionArrow = Shape.CreateArrow(from, to, 5, Color.FromRGBA(255, 0, 0, 255).PackToInt(),ShapeFlags.ONCE | ShapeFlags.NOZBUFFER | ShapeFlags.TRANSP | ShapeFlags.DOUBLESIDE | ShapeFlags.NOOUTLINE);
		}
		
		super._WB_AfterWorldUpdate(owner, timeSlice);
	}
#endif

	//RPC methods



}
