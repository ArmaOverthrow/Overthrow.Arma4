[EntityEditorProps(category: "Overthrow/Managers", description: "Manages all deployments across factions")]
class OVT_DeploymentManagerComponentClass : OVT_ComponentClass
{
}

class OVT_DeploymentManagerComponent : OVT_Component
{
	[Attribute(desc: "Deployment registry containing all available deployment configs")]
	ref OVT_DeploymentRegistry m_DeploymentRegistry;
	
	[Attribute(defvalue: "{53D8FEE526831693}Prefabs/GameMode/OVT_Deployment.et", desc: "Prefab to use for deployment entities")]
	ResourceName m_DeploymentPrefab;
	
	[Attribute(defvalue: "30000", desc: "Interval for deployment evaluation in milliseconds")]
	int m_iEvaluationInterval;
	
	[Attribute(defvalue: "100", desc: "Maximum deployments per faction")]
	int m_iMaxDeploymentsPerFaction;
	
	protected ref array<OVT_DeploymentComponent> m_aActiveDeployments;
	protected ref map<int, ref array<OVT_DeploymentComponent>> m_mFactionDeployments; // factionIndex -> deployments
	protected ref map<int, int> m_mFactionResources; // factionIndex -> available resources
	protected ref array<vector> m_aAvailableSlots; // Cached slot positions
	protected bool m_bInitialized;
	
	static const float THREAT_EVALUATION_RADIUS = 2000; // 2km
	static const int MIN_DEPLOYMENT_DISTANCE = 500; // 500m minimum between deployments
	
	static OVT_DeploymentManagerComponent s_Instance;
	
	private ref array<IEntity> m_aFoundSlots;
	
	//------------------------------------------------------------------------------------------------
	static OVT_DeploymentManagerComponent GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode gameMode = GetGame().GetGameMode();
			if (gameMode)
				s_Instance = OVT_DeploymentManagerComponent.Cast(gameMode.FindComponent(OVT_DeploymentManagerComponent));
		}
		return s_Instance;
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		if (!Replication.IsServer())
			return;
			
		m_aActiveDeployments = new array<OVT_DeploymentComponent>;
		m_mFactionDeployments = new map<int, ref array<OVT_DeploymentComponent>>;
		m_mFactionResources = new map<int, int>;
		m_aAvailableSlots = new array<vector>;
		
		// Initialize deployment registry if empty
		if (!m_DeploymentRegistry)
		{
			m_DeploymentRegistry = new OVT_DeploymentRegistry();
			m_DeploymentRegistry.m_sRegistryName = "Default Registry";
		}
		
		// Start evaluation timer
		GetGame().GetCallqueue().CallLater(EvaluateDeployments, m_iEvaluationInterval, true);
		
		m_bInitialized = true;
	}
	
	//------------------------------------------------------------------------------------------------
	void Init(IEntity owner)
	{		
		// Cache available slots from the world
		CacheAvailableSlots();
		
		// Initialize faction resources
		InitializeFactionResources();
		
		Print("[Overthrow] DeploymentManager initialized with " + m_DeploymentRegistry.m_aDeploymentConfigs.Count() + " deployment configs", LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	protected void CacheAvailableSlots()
	{
		m_aAvailableSlots.Clear();
		
		// Query for all slot entities in the world
		// This will depend on how slots are implemented in the base upgrade system
		m_aFoundSlots = new array<IEntity>;
		GetGame().GetWorld().QueryEntitiesBySphere(vector.Zero, 50000, FilterSlotEntities, null, EQueryEntitiesFlags.ALL);
		
		foreach (IEntity slotEntity : m_aFoundSlots)
		{
			m_aAvailableSlots.Insert(slotEntity.GetOrigin());
		}
		
		Print("[Overthrow] Cached " + m_aAvailableSlots.Count() + " available deployment slots", LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool FilterSlotEntities(IEntity entity)
	{
		SCR_EditableEntityComponent editable = EPF_Component<SCR_EditableEntityComponent>.Find(entity);
		if (editable && editable.GetEntityType() == EEditableEntityType.SLOT)
		{
			m_aFoundSlots.Insert(entity);
		}
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	protected void InitializeFactionResources()
	{
		FactionManager factionManager = GetGame().GetFactionManager();
		if (!factionManager)
			return;
		
		array<Faction> factions = new array<Faction>;
		factionManager.GetFactionsList(factions);
		
		foreach (Faction faction : factions)
		{
			int factionIndex = factionManager.GetFactionIndex(faction);
			m_mFactionResources.Set(factionIndex, 1000); // Starting resources
			m_mFactionDeployments.Set(factionIndex, new array<OVT_DeploymentComponent>);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void EvaluateDeployments()
	{
		if (!m_bInitialized || !Replication.IsServer())
			return;
		
		FactionManager factionManager = GetGame().GetFactionManager();
		if (!factionManager)
			return;
		
		// Evaluate each faction's deployment needs
		array<Faction> factions = new array<Faction>;
		factionManager.GetFactionsList(factions);
		
		foreach (Faction faction : factions)
		{
			int factionIndex = factionManager.GetFactionIndex(faction);
			EvaluateFactionDeployments(factionIndex);
		}
		
		// Clean up destroyed deployments
		CleanupDestroyedDeployments();
	}
	
	//------------------------------------------------------------------------------------------------
	protected void EvaluateFactionDeployments(int factionIndex)
	{
		array<OVT_DeploymentComponent> factionDeployments = m_mFactionDeployments.Get(factionIndex);
		if (!factionDeployments)
			return;
		
		// Skip if faction has reached deployment limit
		if (factionDeployments.Count() >= m_iMaxDeploymentsPerFaction)
			return;
		
		int availableResources = m_mFactionResources.Get(factionIndex);
		if (availableResources <= 0)
			return;
		
		// Find potential deployment locations
		array<vector> candidatePositions = FindDeploymentCandidates(factionIndex);
		
		// Evaluate each candidate position
		foreach (vector position : candidatePositions)
		{
			float threatLevel = CalculateThreatLevel(position, factionIndex);
			
			// Find suitable deployment config for this position and threat level
			OVT_DeploymentConfig bestConfig = FindBestDeploymentConfig(position, factionIndex, threatLevel, availableResources);
			if (bestConfig)
			{
				int deploymentCost = bestConfig.GetTotalResourceCost();
				if (availableResources >= deploymentCost)
				{
					CreateDeployment(bestConfig, position, factionIndex);
					availableResources -= deploymentCost;
					m_mFactionResources.Set(factionIndex, availableResources);
					break; // Only create one deployment per evaluation cycle
				}
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected array<vector> FindDeploymentCandidates(int factionIndex)
	{
		array<vector> candidates = new array<vector>;
		
		// Start with cached slot positions
		foreach (vector slotPos : m_aAvailableSlots)
		{
			if (IsPositionSuitableForDeployment(slotPos, factionIndex))
				candidates.Insert(slotPos);
		}
		
		// Add strategic positions near faction assets
		array<vector> strategicPositions = FindStrategicPositions(factionIndex);
		foreach (vector pos : strategicPositions)
		{
			if (IsPositionSuitableForDeployment(pos, factionIndex))
				candidates.Insert(pos);
		}
		
		// Limit candidates for performance
		if (candidates.Count() > 20)
		{
			// Sort by importance and take top 20
			candidates.Resize(20);
		}
		
		return candidates;
	}
	
	//------------------------------------------------------------------------------------------------
	protected array<vector> FindStrategicPositions(int factionIndex)
	{
		array<vector> positions = new array<vector>;
		
		// Add positions near faction-controlled bases
		OVT_OccupyingFactionManager ofManager = OVT_Global.GetOccupyingFaction();
		if (ofManager)
		{
			foreach (OVT_BaseData baseData : ofManager.m_Bases)
			{
				if (baseData.faction == factionIndex)
				{
					// Add positions around the base
					for (int i = 0; i < 4; i++)
					{
						float angle = (i / 4.0) * Math.PI2;
						vector offset = Vector(Math.Cos(angle) * 1000, 0, Math.Sin(angle) * 1000);
						positions.Insert(baseData.location + offset);
					}
				}
			}
		}
		
		// Add positions near towns with faction support
		OVT_TownManagerComponent townManager = OVT_Global.GetTowns();
		if (townManager)
		{
			foreach (OVT_TownData townData : townManager.m_Towns)
			{
				if (townData && IsPositionRelevantToFaction(townData.location, factionIndex))
				{
					// Add positions around the town
					float radius = Math.Max(townData.size * 100, 500); // Scale with town size
					for (int j = 0; j < 3; j++)
					{
						float angle = (j / 3.0) * Math.PI2;
						vector offset = Vector(Math.Cos(angle) * radius, 0, Math.Sin(angle) * radius);
						positions.Insert(townData.location + offset);
					}
				}
			}
		}
		
		return positions;
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool IsPositionSuitableForDeployment(vector position, int factionIndex)
	{
		// Check minimum distance to existing deployments
		foreach (OVT_DeploymentComponent deployment : m_aActiveDeployments)
		{
			float distance = vector.Distance(position, deployment.GetPosition());
			if (distance < MIN_DEPLOYMENT_DISTANCE)
				return false;
		}
		
		// Check if position is in suitable terrain
		TraceParam param = new TraceParam();
		param.Start = position + Vector(0, 100, 0);
		param.End = position + Vector(0, -100, 0);
		param.Flags = TraceFlags.WORLD;
		
		float result = GetGame().GetWorld().TraceMove(param, null);
		if (result >= 1.0)
			return false; // No ground found
		
		// Additional checks could include:
		// - Not in restricted areas
		// - Not too close to player bases
		// - Suitable for faction type
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool IsPositionRelevantToFaction(vector position, int factionIndex)
	{
		// Determine if a position is strategically relevant to a faction
		// This is a simplified implementation
		
		FactionManager factionManager = GetGame().GetFactionManager();
		if (!factionManager)
			return false;
		
		Faction faction = factionManager.GetFactionByIndex(factionIndex);
		if (!faction)
			return false;
		
		string factionKey = faction.GetFactionKey();
		
		// Resistance is interested in populated areas
		if (factionKey == OVT_Global.GetConfig().GetPlayerFaction().GetFactionKey())
		{
			// Check proximity to towns
			OVT_TownManagerComponent townManager = OVT_Global.GetTowns();
			if (townManager)
			{
				foreach (OVT_TownData townData : townManager.m_Towns)
				{
					if (townData && vector.Distance(position, townData.location) < 2000)
						return true;
				}
			}
		}
		
		// Occupying forces are interested in strategic control points
		if (factionKey == OVT_Global.GetConfig().GetOccupyingFaction().GetFactionKey())
		{
			// Always relevant for occupying forces
			return true;
		}
		
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	protected float CalculateThreatLevel(vector position, int factionIndex)
	{
		float threat = 0;
		
		// Base threat level
		threat += 10;
		
		// Increase threat based on nearby enemy assets
		array<OVT_DeploymentComponent> nearbyDeployments = GetDeploymentsInRadius(position, THREAT_EVALUATION_RADIUS);
		foreach (OVT_DeploymentComponent deployment : nearbyDeployments)
		{
			if (deployment.GetControllingFaction() != factionIndex)
			{
				threat += 20; // Enemy deployment increases threat
			}
		}
		
		// Increase threat based on player activity
		float playerProximity = GetNearestPlayerDistance(position);
		if (playerProximity < 1000)
		{
			threat += (1000 - playerProximity) / 50; // Closer players = higher threat
		}
		
		// Increase threat based on recent battles
		if (HasRecentBattleNearby(position))
		{
			threat += 30;
		}
		
		return Math.Clamp(threat, 0, 100);
	}
	
	//------------------------------------------------------------------------------------------------
	protected OVT_DeploymentConfig FindBestDeploymentConfig(vector position, int factionIndex, float threatLevel, int availableResources)
	{
		array<OVT_DeploymentConfig> suitableConfigs = new array<OVT_DeploymentConfig>;
		
		// Filter configs by conditions and resources
		foreach (OVT_DeploymentConfig config : m_DeploymentRegistry.m_aDeploymentConfigs)
		{
			if (!config.IsValidConfig())
				continue;
			
			// Check if faction can use this config
			OVT_FactionType factionType = GetFactionType(factionIndex);
			if (!config.CanFactionUse(factionType))
				continue;
			
			// Check resource cost
			int cost = config.GetTotalResourceCost();
			if (cost > availableResources)
				continue;
			
			// Check deployment conditions
			if (OVT_DeploymentComponent.CheckDeploymentConditions(config, position, factionIndex, threatLevel))
			{
				suitableConfigs.Insert(config);
			}
		}
		
		if (suitableConfigs.IsEmpty())
			return null;
		
		// Select best config based on priority and threat level
		OVT_DeploymentConfig bestConfig = null;
		int bestPriority = 999;
		
		foreach (OVT_DeploymentConfig config : suitableConfigs)
		{
			// Lower priority value = higher priority
			if (config.m_iPriority < bestPriority)
			{
				bestPriority = config.m_iPriority;
				bestConfig = config;
			}
		}
		
		return bestConfig;
	}
	
	//------------------------------------------------------------------------------------------------
	protected OVT_FactionType GetFactionType(int factionIndex)
	{
		FactionManager factionManager = GetGame().GetFactionManager();
		if (!factionManager)
			return 0;
		
		Faction faction = factionManager.GetFactionByIndex(factionIndex);
		if (!faction)
			return 0;
		
		string factionKey = faction.GetFactionKey();
		
		if (factionKey == OVT_Global.GetConfig().GetOccupyingFaction().GetFactionKey())
			return OVT_FactionType.OCCUPYING_FACTION;
		else if (factionKey == OVT_Global.GetConfig().GetPlayerFaction().GetFactionKey())
			return OVT_FactionType.RESISTANCE_FACTION;
		else if (factionKey == OVT_Global.GetConfig().GetSupportingFaction().GetFactionKey())
			return OVT_FactionType.SUPPORTING_FACTION;
		
		return 0;
	}
	
	//------------------------------------------------------------------------------------------------
	OVT_DeploymentComponent CreateDeployment(OVT_DeploymentConfig config, vector position, int factionIndex)
	{
		if (!config || !config.IsValidConfig())
			return null;
		
		// Create deployment entity
		if (!m_DeploymentPrefab || m_DeploymentPrefab.IsEmpty())
		{
			Print("Deployment prefab not configured", LogLevel.ERROR);
			return null;
		}
		
		Resource deploymentPrefab = Resource.Load(m_DeploymentPrefab);
		if (!deploymentPrefab)
		{
			Print(string.Format("Failed to load deployment prefab: %1", m_DeploymentPrefab), LogLevel.ERROR);
			return null;
		}
		
		// Create transform matrix
		vector mat[4];
		Math3D.MatrixIdentity4(mat);
		mat[3] = position;
		
		// Spawn deployment entity
		IEntity deploymentEntity = OVT_Global.SpawnEntityPrefabMatrix(deploymentPrefab.GetResource().GetResourceName(), mat);
		if (!deploymentEntity)
		{
			Print("Failed to spawn deployment entity", LogLevel.ERROR);
			return null;
		}
		
		// Get deployment component and initialize
		OVT_DeploymentComponent deployment = OVT_DeploymentComponent.Cast(deploymentEntity.FindComponent(OVT_DeploymentComponent));
		if (!deployment)
		{
			Print("Deployment entity missing OVT_DeploymentComponent", LogLevel.ERROR);
			delete deploymentEntity;
			return null;
		}
		
		deployment.InitializeDeployment(config, factionIndex);
		
		Print(string.Format("Created deployment '%1' for faction %2 at %3", config.m_sDeploymentName, factionIndex, position.ToString()), LogLevel.NORMAL);
		
		return deployment;
	}
	
	//------------------------------------------------------------------------------------------------
	void RegisterDeployment(OVT_DeploymentComponent deployment)
	{
		if (!deployment || m_aActiveDeployments.Contains(deployment))
			return;
		
		m_aActiveDeployments.Insert(deployment);
		
		// Add to faction-specific list
		int factionIndex = deployment.GetControllingFaction();
		array<OVT_DeploymentComponent> factionDeployments = m_mFactionDeployments.Get(factionIndex);
		if (factionDeployments && !factionDeployments.Contains(deployment))
		{
			factionDeployments.Insert(deployment);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void UnregisterDeployment(OVT_DeploymentComponent deployment)
	{
		if (!deployment)
			return;
		
		int index = m_aActiveDeployments.Find(deployment);
		if (index != -1)
			m_aActiveDeployments.Remove(index);
		
		// Remove from faction-specific list
		int factionIndex = deployment.GetControllingFaction();
		array<OVT_DeploymentComponent> factionDeployments = m_mFactionDeployments.Get(factionIndex);
		if (factionDeployments)
		{
			int factionIndex2 = factionDeployments.Find(deployment);
			if (factionIndex2 != -1)
				factionDeployments.Remove(factionIndex2);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected void CleanupDestroyedDeployments()
	{
		for (int i = m_aActiveDeployments.Count() - 1; i >= 0; i--)
		{
			OVT_DeploymentComponent deployment = m_aActiveDeployments[i];
			if (!deployment || !deployment.GetOwner())
			{
				m_aActiveDeployments.Remove(i);
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	// Resource management
	//------------------------------------------------------------------------------------------------
	void AddFactionResources(int factionIndex, int amount)
	{
		int current = m_mFactionResources.Get(factionIndex);
		m_mFactionResources.Set(factionIndex, current + amount);
	}
	
	//------------------------------------------------------------------------------------------------
	void SubtractFactionResources(int factionIndex, int amount)
	{
		int current = m_mFactionResources.Get(factionIndex);
		m_mFactionResources.Set(factionIndex, Math.Max(0, current - amount));
	}
	
	//------------------------------------------------------------------------------------------------
	int GetFactionResources(int factionIndex)
	{
		return m_mFactionResources.Get(factionIndex);
	}
	
	//------------------------------------------------------------------------------------------------
	map<int, int> GetAllFactionResources()
	{
		return m_mFactionResources;
	}
	
	//------------------------------------------------------------------------------------------------
	void SetAllFactionResources(map<int, int> resources)
	{
		m_mFactionResources = resources;
	}
	
	//------------------------------------------------------------------------------------------------
	// Utility methods
	//------------------------------------------------------------------------------------------------
	array<OVT_DeploymentComponent> GetDeploymentsInRadius(vector position, float radius)
	{
		array<OVT_DeploymentComponent> nearbyDeployments = new array<OVT_DeploymentComponent>;
		
		foreach (OVT_DeploymentComponent deployment : m_aActiveDeployments)
		{
			float distance = vector.Distance(position, deployment.GetPosition());
			if (distance <= radius)
				nearbyDeployments.Insert(deployment);
		}
		
		return nearbyDeployments;
	}
	
	//------------------------------------------------------------------------------------------------
	protected float GetNearestPlayerDistance(vector position)
	{
		float nearestDistance = float.MAX;
		
		array<int> players = new array<int>;
		GetGame().GetPlayerManager().GetPlayers(players);
		
		foreach (int playerId : players)
		{
			IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (!player)
				continue;
			
			float distance = vector.Distance(player.GetOrigin(), position);
			if (distance < nearestDistance)
				nearestDistance = distance;
		}
		
		return nearestDistance;
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool HasRecentBattleNearby(vector position)
	{
		// TODO: Implement battle tracking system
		// For now, return false
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	// Public API
	//------------------------------------------------------------------------------------------------
	array<OVT_DeploymentComponent> GetAllDeployments()
	{
		return m_aActiveDeployments;
	}
	
	//------------------------------------------------------------------------------------------------
	array<OVT_DeploymentComponent> GetFactionDeployments(int factionIndex)
	{
		array<OVT_DeploymentComponent> factionDeployments = m_mFactionDeployments.Get(factionIndex);
		if (!factionDeployments)
			return new array<OVT_DeploymentComponent>;
		return factionDeployments;
	}
	
	//------------------------------------------------------------------------------------------------
	void ForceCreateDeployment(OVT_DeploymentConfig config, vector position, int factionIndex)
	{
		CreateDeployment(config, position, factionIndex);
	}
	
	//------------------------------------------------------------------------------------------------
	void DestroyDeployment(OVT_DeploymentComponent deployment)
	{
		if (deployment)
			deployment.DestroyDeployment();
	}
	
	//------------------------------------------------------------------------------------------------
	void PrintDebugInfo()
	{
		Print("=== Deployment Manager Debug Info ===");
		Print(string.Format("Total Deployments: %1", m_aActiveDeployments.Count()));
		Print(string.Format("Available Configs: %1", m_DeploymentRegistry.m_aDeploymentConfigs.Count()));
		Print(string.Format("Cached Slots: %1", m_aAvailableSlots.Count()));
		
		foreach (int factionIndex, array<OVT_DeploymentComponent> deployments : m_mFactionDeployments)
		{
			int resources = m_mFactionResources.Get(factionIndex);
			Print(string.Format("Faction %1: %2 deployments, %3 resources", factionIndex, deployments.Count(), resources));
		}
	}
}

// EPF Save Data
[EPF_ComponentSaveDataType(OVT_DeploymentManagerComponent), BaseContainerProps()]
class OVT_DeploymentManagerComponentSaveDataClass : EPF_ComponentSaveDataClass
{
}

[EDF_DbName.Automatic()]
class OVT_DeploymentManagerComponentSaveData : EPF_ComponentSaveData
{
	ref map<int, int> m_mFactionResources;
	
	override EPF_EReadResult ReadFrom(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{
		OVT_DeploymentManagerComponent manager = OVT_DeploymentManagerComponent.Cast(component);
		if (!manager)
			return EPF_EReadResult.ERROR;
		
		// Save faction resources
		m_mFactionResources = new map<int, int>;
		map<int, int> factionResources = manager.GetAllFactionResources();
		foreach (int factionIndex, int resources : factionResources)
		{
			m_mFactionResources.Set(factionIndex, resources);
		}
		
		return EPF_EReadResult.OK;
	}
	
	override EPF_EApplyResult ApplyTo(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{
		OVT_DeploymentManagerComponent manager = OVT_DeploymentManagerComponent.Cast(component);
		if (!manager)
			return EPF_EApplyResult.ERROR;
		
		// Restore faction resources
		if (m_mFactionResources)
		{
			manager.SetAllFactionResources(m_mFactionResources);
		}
		
		return EPF_EApplyResult.OK;
	}
}