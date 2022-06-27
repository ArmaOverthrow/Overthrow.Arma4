class OVT_OccupyingFactionManagerClass: OVT_ComponentClass
{	
}

class OVT_OccupyingFactionManager: OVT_Component
{	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Base Controller Prefab", params: "et", category: "Controllers")]
	ResourceName m_pBaseControllerPrefab;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "QRF Controller Prefab", params: "et", category: "Controllers")]
	ResourceName m_pQRFControllerPrefab;
	
	int m_iResources;
	float m_iThreat;
	ref array<RplId> m_Bases;
	ref array<vector> m_BasesToSpawn;
	
	protected int m_iOccupyingFactionIndex;
	protected int m_iPlayerFactionIndex;
	
	OVT_QRFControllerComponent m_CurrentQRF;
	OVT_BaseControllerComponent m_CurrentQRFBase;
	
	const int OF_UPDATE_FREQUENCY = 60000;
	
	ref ScriptInvoker<IEntity> m_OnAIKilled = new ScriptInvoker<IEntity>;
	ref ScriptInvoker<OVT_BaseControllerComponent> m_OnBaseControlChanged = new ScriptInvoker<OVT_BaseControllerComponent>;
	
	static OVT_OccupyingFactionManager s_Instance;
	
	static OVT_OccupyingFactionManager GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode pGameMode = GetGame().GetGameMode();
			if (pGameMode)
				s_Instance = OVT_OccupyingFactionManager.Cast(pGameMode.FindComponent(OVT_OccupyingFactionManager));
		}

		return s_Instance;
	}
	
	void OVT_OccupyingFactionManager()
	{
		m_Bases = new array<RplId>;	
		m_BasesToSpawn = new array<vector>;	
	}
	
	void Init(IEntity owner)
	{	
		if(!Replication.IsServer()) return;
			
		m_iThreat = m_Config.m_Difficulty.baseThreat;
		m_iResources = m_Config.m_Difficulty.startingResources;
		
		Faction playerFaction = GetGame().GetFactionManager().GetFactionByKey(m_Config.m_sPlayerFaction);
		m_iPlayerFactionIndex = GetGame().GetFactionManager().GetFactionIndex(playerFaction);
		
		Faction occupyingFaction = GetGame().GetFactionManager().GetFactionByKey(m_Config.m_sOccupyingFaction);
		m_iOccupyingFactionIndex = GetGame().GetFactionManager().GetFactionIndex(occupyingFaction);
				
		OVT_Global.GetTowns().m_OnTownControlChange.Insert(OnTownControlChanged);
		
		InitializeBases();		
	}
	
	void PostGameStart()
	{
		GetGame().GetCallqueue().CallLater(CheckUpdate, OF_UPDATE_FREQUENCY / m_Config.m_iTimeMultiplier, true, GetOwner());
		GetGame().GetCallqueue().CallLater(SpawnBaseControllers, 0);
	}
	
	void OnTownControlChanged(OVT_TownData town)
	{
		if(town.faction == m_iPlayerFactionIndex)
		{
			m_iThreat += town.size * 15;
		}
	}
	
	OVT_BaseControllerComponent GetNearestBase(vector pos)
	{
		IEntity nearestBase;
		float nearest = 9999999;
		foreach(RplId id : m_Bases)
		{
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
			IEntity marker = rpl.GetEntity();
			float distance = vector.Distance(marker.GetOrigin(), pos);
			if(distance < nearest){
				nearest = distance;
				nearestBase = marker;
			}
		}
		if(!nearestBase) return null;
		return OVT_BaseControllerComponent.Cast(nearestBase.FindComponent(OVT_BaseControllerComponent));
	}
	
	void GetBasesWithinDistance(vector pos, float maxDistance, out array<OVT_BaseControllerComponent> bases)
	{
		foreach(RplId id : m_Bases)
		{
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
			IEntity marker = rpl.GetEntity();
			float distance = vector.Distance(marker.GetOrigin(), pos);
			if(distance < maxDistance){
				OVT_BaseControllerComponent base = OVT_BaseControllerComponent.Cast(marker.FindComponent(OVT_BaseControllerComponent));
				bases.Insert(base);
			}
		}
	}
	
	void RecoverResources(int resources)
	{
		m_iResources += resources;
	}
	
	void InitializeBases()
	{
		#ifdef OVERTHROW_DEBUG
		Print("Finding bases");
		#endif
		
		GetGame().GetWorld().QueryEntitiesBySphere("0 0 0", 99999999, CheckBaseAdd, FilterBaseEntities, EQueryEntitiesFlags.STATIC);
	}
	
	protected void SpawnBaseControllers()
	{
		foreach(vector pos : m_BasesToSpawn)
		{		
			IEntity baseEntity = SpawnBaseController(pos);
			
			OVT_BaseControllerComponent base = OVT_BaseControllerComponent.Cast(baseEntity.FindComponent(OVT_BaseControllerComponent));
			
			m_Bases.Insert(base.GetRpl().Id());
			base.SetControllingFaction(m_Config.GetOccupyingFactionIndex());
		}
		m_BasesToSpawn.Clear();
		m_BasesToSpawn = null;
		GetGame().GetCallqueue().CallLater(DistributeInitialResources, 100);
	}
	
	protected void DistributeInitialResources()
	{
		//Distribute initial resources
		
		int resourcesPerBase = Math.Floor(m_iResources / m_Bases.Count());
		
		foreach(RplId id : m_Bases)
		{
			OVT_BaseControllerComponent base = GetBase(id);
			m_iResources -= base.SpendResources(resourcesPerBase, m_iThreat);			
			
			if(m_iResources <= 0) break;
		}
		if(m_iResources < 0) m_iResources = 0;
		Print("OF Remaining Resources: " + m_iResources);
	}
	
	OVT_BaseControllerComponent GetBase(RplId id)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
		IEntity marker = rpl.GetEntity();
		return OVT_BaseControllerComponent.Cast(marker.FindComponent(OVT_BaseControllerComponent));
	}
	
	OVT_BaseControllerComponent GetBaseByIndex(int index)
	{
		return GetBase(m_Bases[index]);
	}
	
	int GetBaseIndex(OVT_BaseControllerComponent base)
	{
		return m_Bases.Find(base.GetRpl().Id());
	}
	
	void StartBaseQRF(OVT_BaseControllerComponent base)
	{
		if(m_CurrentQRF) return;
		
		m_CurrentQRF = SpawnQRFController(base.GetOwner().GetOrigin());
		RplComponent rpl = RplComponent.Cast(m_CurrentQRF.GetOwner().FindComponent(RplComponent));
		Rpc(RpcDo_SetQRF, rpl.Id());
		m_CurrentQRF.m_OnFinished.Insert(OnQRFFinishedBase);
		m_CurrentQRFBase = base;
	}
	
	void OnQRFFinishedBase()
	{	
		if(m_CurrentQRF.m_iWinningFaction != m_CurrentQRFBase.m_iControllingFaction)
		{
			if(m_CurrentQRFBase.IsOccupyingFaction())
			{
				m_iThreat += 50;
				OVT_Global.GetPlayers().HintMessageAll("BaseControlledResistance");
			}else{
				OVT_Global.GetPlayers().HintMessageAll("BaseControlledOccupying");
			}
			m_CurrentQRFBase.SetControllingFaction(m_CurrentQRF.m_iWinningFaction);
		}		
				
		SCR_Global.DeleteEntityAndChildren(m_CurrentQRF.GetOwner());
		m_CurrentQRF = null;
		Rpc(RpcDo_ClearQRF);
	}
	
	bool CheckBaseAdd(IEntity ent)
	{		
		#ifdef OVERTHROW_DEBUG
		OVT_TownManagerComponent townManager = OVT_Global.GetTowns();
		OVT_TownData closestTown = townManager.GetNearestTown(ent.GetOrigin());
		Print("Adding base near " + closestTown.name);
		#endif
		
		m_BasesToSpawn.Insert(ent.GetOrigin());		
		return true;
	}
	
	IEntity SpawnBaseController(vector loc)
	{
		EntitySpawnParams params = EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = loc;
		return GetGame().SpawnEntityPrefab(Resource.Load(m_pBaseControllerPrefab), null, params));
		
	}
	
	OVT_QRFControllerComponent SpawnQRFController(vector loc)
	{
		EntitySpawnParams params = EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = loc;
		IEntity qrf = GetGame().SpawnEntityPrefab(Resource.Load(m_pQRFControllerPrefab), null, params);
		return OVT_QRFControllerComponent.Cast(qrf.FindComponent(OVT_QRFControllerComponent));	
	}
	
	bool FilterBaseEntities(IEntity entity)
	{
		MapDescriptorComponent mapdesc = MapDescriptorComponent.Cast(entity.FindComponent(MapDescriptorComponent));
		if (mapdesc){	
			int type = mapdesc.GetBaseType();
			if(type == EMapDescriptorType.MDT_NAME_GENERIC) {				
				if(mapdesc.Item().GetDisplayName() == "#AR-MapLocation_Military") return true;
			}
		}
				
		return false;	
	}
	
	void CheckUpdate()
	{
		TimeContainer time = m_Time.GetTime();		
		
		//Every 6 hrs get more resources
		if((time.m_iHours == 0 
			|| time.m_iHours == 6 
			|| time.m_iHours == 12 
			|| time.m_iHours == 18)
			 && 
			time.m_iMinutes == 0)
		{
			GainResources();
		}
		
		//Every hour distribute resources we have, if any
		if(m_iResources > 0 && time.m_iMinutes == 0)
		{
			//To-Do: prioritize bases that need it/are under threat
			foreach(RplId id : m_Bases)
			{
				OVT_BaseControllerComponent base = GetBase(id);
				if(!base.IsOccupyingFaction()) continue;
				
				m_iResources -= base.SpendResources(m_iResources, m_iThreat);			
				
				if(m_iResources <= 0) {
					m_iResources = 0;
				}
			}
			Print("OF Reserve Resources: " + m_iResources);
		}
		
		//Every 15 mins reduce threat
		if(time.m_iMinutes == 0 
			|| time.m_iMinutes == 15 
			|| time.m_iMinutes == 30 
			|| time.m_iMinutes == 45)			
		{
			m_iThreat -= 1;
			if(m_iThreat < 0) m_iThreat = 0;
		}
	}
	
	void OnBaseControlChange(OVT_BaseControllerComponent base)
	{
		if(m_OnBaseControlChanged) m_OnBaseControlChanged.Invoke(base);
	}
	
	void GainResources()
	{
		float threatFactor = m_iThreat / 100;
		if(threatFactor > 1) threatFactor = 1;
		int newResources = m_Config.m_Difficulty.baseResourcesPerTick + (m_Config.m_Difficulty.resourcesPerTick * threatFactor);
		
		m_iResources += newResources;
		
		Print ("OF Gained Resources: " + newResources.ToString());			
	}
	
	void OnAIKilled(IEntity ai, IEntity insitgator)
	{
		if(!Replication.IsServer()) return;
		
		m_iThreat += 1;
		
		m_OnAIKilled.Invoke(ai);
	}
	
	//RPC Methods
	
	override bool RplSave(ScriptBitWriter writer)
	{		
		//Send JIP bases
		writer.Write(m_Bases.Count(), 32); 
		for(int i; i<m_Bases.Count(); i++)
		{
			writer.WriteRplId(m_Bases[i]);
		}
		
		return true;
	}
	
	override bool RplLoad(ScriptBitReader reader)
	{				
		//Recieve JIP towns
		int length;
		RplId id;
		
		if (!reader.Read(length, 32)) return false;
		for(int i; i<length; i++)
		{			
			if (!reader.ReadRplId(id)) return false;
			m_Bases.Insert(id);
		}
		return true;
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetQRF(RplId id)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
		IEntity marker = rpl.GetEntity();
		m_CurrentQRF = OVT_QRFControllerComponent.Cast(marker.FindComponent(OVT_QRFControllerComponent));
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_ClearQRF()
	{		
		m_CurrentQRF = null;
	}
	
	void ~OVT_OccupyingFactionManager()
	{
		GetGame().GetCallqueue().Remove(CheckUpdate);	
		
		if(m_Bases)
		{
			m_Bases.Clear();
			m_Bases = null;
		}			
	}
}