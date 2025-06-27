// Helper class for sorting candidate positions by threat level
class OVT_CandidatePosition
{
	vector position;
	float threatLevel;
	
	[SortAttribute(),NonSerialized()]
	float sortBy;
	
	void OVT_CandidatePosition(vector pos, float threat)
	{
		position = pos;
		threatLevel = threat;
		sortBy = threat;
	}
}

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
	
	protected ref array<ref EntityID> m_aActiveDeployments;
	protected ref map<int, ref array<ref EntityID>> m_mFactionDeployments; // factionIndex -> deployments
	protected ref map<int, int> m_mFactionResources; // factionIndex -> available resources
	protected ref array<vector> m_aAvailableSlots; // Cached slot positions
	protected bool m_bInitialized;
	
	static const float THREAT_EVALUATION_RADIUS = 2000; // 2km
	static const int MIN_DEPLOYMENT_DISTANCE = 100; // 100m minimum between deployments
	static const int MAX_DEPLOYMENTS_PER_EVALUATION = 10; // Maximum deployments per evaluation cycle
	
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
			
		m_aActiveDeployments = new array<ref EntityID>;
		m_mFactionDeployments = new map<int, ref array<ref EntityID>>;
		m_mFactionResources = new map<int, int>;
		m_aAvailableSlots = new array<vector>;
		
		// Initialize deployment registry if empty
		if (!m_DeploymentRegistry)
		{
			m_DeploymentRegistry = new OVT_DeploymentRegistry();
			m_DeploymentRegistry.m_sRegistryName = "Default Registry";
		}
		
		m_bInitialized = true;
	}
	
	void PostGameStart()
	{
		//First evaluation sooner
		GetGame().GetCallqueue().CallLater(EvaluateDeployments, 10000, false);
		
		// Start evaluation timer
		GetGame().GetCallqueue().CallLater(EvaluateDeployments, m_iEvaluationInterval, true);
	}
	
	//------------------------------------------------------------------------------------------------
	void Init(IEntity owner)
	{		
		// Cache available slots from the world
		CacheAvailableSlots();
				
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
		array<ref EntityID> factionDeployments = m_mFactionDeployments.Get(factionIndex);
		if (!factionDeployments)
		{
			factionDeployments = new array<ref EntityID>();
			m_mFactionDeployments.Set(factionIndex, factionDeployments);
		}
			
		
		// Skip if faction has reached deployment limit
		if (factionDeployments.Count() >= m_iMaxDeploymentsPerFaction)
			return;
		
		int availableResources = m_mFactionResources.Get(factionIndex);
		
		// Find potential deployment locations
		array<vector> candidatePositions = FindDeploymentCandidates(factionIndex);

		// Calculate threat levels for all candidates and add randomness
		array<ref OVT_CandidatePosition> candidatesWithThreat = new array<ref OVT_CandidatePosition>;
		foreach (vector position : candidatePositions)
		{
			float baseThreatLevel = CalculateThreatLevel(position, factionIndex);
			// Add randomness: ±20% of base threat level
			float randomModifier = s_AIRandomGenerator.RandFloatXY(-0.2, 0.2);
			float finalThreatLevel = baseThreatLevel * (1.0 + randomModifier);
			
			OVT_CandidatePosition candidate = new OVT_CandidatePosition(position, finalThreatLevel);
			candidatesWithThreat.Insert(candidate);
		}
		
		// Sort candidates by threat level (highest first)
		candidatesWithThreat.Sort(true);
		
		int numDeployments = 0;
		
		// Evaluate each candidate position in order of threat level
		foreach (OVT_CandidatePosition candidate : candidatesWithThreat)
		{
			// Find suitable deployment config for this position and threat level
			OVT_DeploymentConfig bestConfig = FindBestDeploymentConfig(candidate.position, factionIndex, candidate.threatLevel, availableResources);
			if (bestConfig)
			{
				// Check if we already have this type of deployment nearby
				if (HasExistingDeploymentOfType(candidate.position, factionIndex, bestConfig.m_sDeploymentName))
				{
					continue; // Skip this position, deployment already exists nearby
				}
				
				int deploymentCost = bestConfig.GetTotalResourceCost();
				if (availableResources >= deploymentCost)
				{
					CreateDeployment(bestConfig, candidate.position, factionIndex);
					availableResources -= deploymentCost;
					m_mFactionResources.Set(factionIndex, availableResources);
					numDeployments++;
				}
			}

			if (numDeployments >= MAX_DEPLOYMENTS_PER_EVALUATION)
				break;
		}
	}
	
	
	//------------------------------------------------------------------------------------------------
	protected array<vector> FindDeploymentCandidates(int factionIndex)
	{
		array<vector> candidates = new array<vector>;
		
		// Get faction type to determine which configs this faction can use
		OVT_FactionTypeFlag factionType = GetFactionType(factionIndex);
		
		// Collect all location types needed by available configs for this faction
		OVT_LocationTypeFlag neededLocationTypes = 0;
		foreach (OVT_DeploymentConfig config : m_DeploymentRegistry.m_aDeploymentConfigs)
		{
			if (config.IsValidConfig() && config.CanFactionUse(factionType))
			{
				neededLocationTypes = neededLocationTypes | config.m_iAllowedLocationTypes;
			}
		}
		
		// Generate candidates based on needed location types
		if (neededLocationTypes & OVT_LocationTypeFlag.TOWN)
		{
			candidates.InsertAll(GetTownPositions(factionIndex));
		}
		
		if (neededLocationTypes & OVT_LocationTypeFlag.BASE)
		{
			candidates.InsertAll(GetBasePositions(factionIndex));
		}
		
		if (neededLocationTypes & OVT_LocationTypeFlag.PORT)
		{
			candidates.InsertAll(GetPortPositions(factionIndex));
		}
		
		if (neededLocationTypes & OVT_LocationTypeFlag.AIRFIELD)
		{
			candidates.InsertAll(GetAirfieldPositions(factionIndex));
		}
		
		if (neededLocationTypes & OVT_LocationTypeFlag.RADIO_TOWER)
		{
			candidates.InsertAll(GetRadioTowerPositions(factionIndex));
		}
		
		if (neededLocationTypes & OVT_LocationTypeFlag.CHECKPOINT)
		{
			candidates.InsertAll(GetCheckpointPositions(factionIndex));
		}
		
		// Filter by suitability
		array<vector> suitableCandidates = new array<vector>;
		foreach (vector pos : candidates)
		{
			if (IsPositionSuitableForDeployment(pos, factionIndex))
				suitableCandidates.Insert(pos);
		}
		
		return suitableCandidates;
	}
	
	//------------------------------------------------------------------------------------------------
	protected array<vector> GetTownPositions(int factionIndex)
	{
		array<vector> positions = new array<vector>;
		
		OVT_TownManagerComponent townManager = OVT_Global.GetTowns();
		if (!townManager)
			return positions;
			
		foreach (OVT_TownData townData : townManager.m_Towns)
		{
			if (townData && IsPositionRelevantToFaction(townData.location, factionIndex))
			{
				positions.Insert(townData.location);
			}
		}
		
		return positions;
	}
	
	//------------------------------------------------------------------------------------------------
	protected array<vector> GetBasePositions(int factionIndex)
	{
		array<vector> positions = new array<vector>;
		
		OVT_OccupyingFactionManager ofManager = OVT_Global.GetOccupyingFaction();
		if (!ofManager)
			return positions;
			
		foreach (OVT_BaseData baseData : ofManager.m_Bases)
		{
			if (baseData && IsPositionRelevantToFaction(baseData.location, factionIndex))
			{
				positions.Insert(baseData.location);
			}
		}
		
		return positions;
	}
	
	//------------------------------------------------------------------------------------------------
	protected array<vector> GetPortPositions(int factionIndex)
	{
		array<vector> positions = new array<vector>;
		
		// TODO: Implement port detection based on actual port entities in the world
		
		return positions;
	}
	
	//------------------------------------------------------------------------------------------------
	protected array<vector> GetAirfieldPositions(int factionIndex)
	{
		array<vector> positions = new array<vector>;
		
		// TODO: Implement airfield detection based on actual airfield entities in the world
		
		return positions;
	}
	
	//------------------------------------------------------------------------------------------------
	protected array<vector> GetRadioTowerPositions(int factionIndex)
	{
		array<vector> positions = new array<vector>;
		
		OVT_OccupyingFactionManager ofManager = OVT_Global.GetOccupyingFaction();
		if (!ofManager)
			return positions;
			
		foreach (OVT_RadioTowerData towerData : ofManager.m_RadioTowers)
		{
			if (towerData && IsPositionRelevantToFaction(towerData.location, factionIndex))
			{
				positions.Insert(towerData.location);
			}
		}
		
		return positions;
	}
	
	//------------------------------------------------------------------------------------------------
	protected array<vector> GetCheckpointPositions(int factionIndex)
	{
		array<vector> positions = new array<vector>;
		
		// TODO: Implement checkpoint detection based on actual checkpoint entities or road intersections
		
		return positions;
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool IsPositionNearWater(vector position)
	{
		// TODO: Implement proper water detection
		// For now, return false - this would need proper terrain analysis
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool IsPositionSuitableForAirfield(vector position)
	{
		// TODO: Implement terrain analysis for airfield suitability
		// For now, return false - this would need terrain slope and size analysis
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool IsPositionBetweenMajorLocations(vector position)
	{
		// Check if position is roughly between a town and a base
		OVT_TownManagerComponent townManager = OVT_Global.GetTowns();
		OVT_OccupyingFactionManager ofManager = OVT_Global.GetOccupyingFaction();
		
		if (!townManager || !ofManager)
			return false;
			
		foreach (OVT_TownData townData : townManager.m_Towns)
		{
			if (!townData)
				continue;
				
			foreach (OVT_BaseData baseData : ofManager.m_Bases)
			{
				if (!baseData)
					continue;
					
				float townDistance = vector.Distance(position, townData.location);
				float baseDistance = vector.Distance(position, baseData.location);
				float townToBaseDistance = vector.Distance(townData.location, baseData.location);
				
				// Check if position is roughly on the line between town and base
				if (townDistance > 500 && baseDistance > 500 && // Not too close to either
					townDistance + baseDistance < townToBaseDistance * 1.2) // Roughly on the path
				{
					return true;
				}
			}
		}
		
		return false;
	}
	
	protected OVT_DeploymentComponent GetDeploymentFromEntity(IEntity entity)
	{
		return OVT_DeploymentComponent.Cast(entity.FindComponent(OVT_DeploymentComponent));
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool IsPositionSuitableForDeployment(vector position, int factionIndex)
	{
		// Check minimum distance to existing deployments
		foreach (EntityID deploymentID : m_aActiveDeployments)
		{
			IEntity entity = GetGame().GetWorld().FindEntityByID(deploymentID);
			if (!entity)
				continue;
				
			OVT_DeploymentComponent deployment = GetDeploymentFromEntity(entity);
			if (!deployment)
				continue;
				
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
	protected bool HasExistingDeploymentOfType(vector position, int factionIndex, string deploymentName, float radius = 250)
	{
		foreach (EntityID deploymentID : m_aActiveDeployments)
		{
			IEntity entity = GetGame().GetWorld().FindEntityByID(deploymentID);
			if (!entity)
				continue;
				
			OVT_DeploymentComponent deployment = GetDeploymentFromEntity(entity);
			if (!deployment)
				continue;
				
			if (deployment.GetControllingFaction() == factionIndex && 
				deployment.GetDeploymentName() == deploymentName)
			{
				float distance = vector.Distance(position, deployment.GetPosition());
				if (distance <= radius)
					return true;
			}
		}
		return false;
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
		OVT_OccupyingFactionManager ofManager = OVT_Global.GetOccupyingFaction();
		threat += ofManager.GetThreatLevel();
		
		threat += ofManager.GetThreatByLocation(position);
		
		// Increase threat based on player activity
		float playerProximity = GetNearestPlayerDistance(position);
		if (playerProximity < 1000)
		{
			threat += (1000 - playerProximity) / 50; // Closer players = higher threat
		}
		
		return threat;
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
			OVT_FactionTypeFlag factionType = GetFactionType(factionIndex);
			if (!config.CanFactionUse(factionType))
			{				
				continue;
			}
			
			// Check if location type is compatible
			OVT_LocationTypeFlag locationType = GetLocationTypeAtPosition(position);
			if (!config.CanUseLocationType(locationType))
			{
				continue;
			}
			
			// Check resource cost
			int cost = config.GetTotalResourceCost();
			if (cost > availableResources)
				continue;
			
			// Check deployment conditions
			if (OVT_DeploymentComponent.CheckDeploymentConditions(config, position, factionIndex, threatLevel))
			{
				// Check chance - if less than 1.0, roll for deployment creation
				if (config.m_fChance >= 100.0 || s_AIRandomGenerator.RandFloatXY(0,100) <= config.m_fChance)
				{
					suitableConfigs.Insert(config);
				}
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
	protected OVT_FactionTypeFlag GetFactionType(int factionIndex)
	{
		FactionManager factionManager = GetGame().GetFactionManager();
		if (!factionManager)
			return 0;
		
		Faction faction = factionManager.GetFactionByIndex(factionIndex);
		if (!faction)
			return 0;
		
		string factionKey = faction.GetFactionKey();
		
		string occupyingKey = OVT_Global.GetConfig().GetOccupyingFaction().GetFactionKey();
		string playerKey = OVT_Global.GetConfig().GetPlayerFaction().GetFactionKey();
		string supportingKey = OVT_Global.GetConfig().GetSupportingFaction().GetFactionKey();
		
		if (factionKey == occupyingKey)
			return OVT_FactionTypeFlag.OCCUPYING_FACTION;
		else if (factionKey == playerKey)
			return OVT_FactionTypeFlag.RESISTANCE_FACTION;
		else if (factionKey == supportingKey)
			return OVT_FactionTypeFlag.SUPPORTING_FACTION;
		
		return 0;
	}
	
	//------------------------------------------------------------------------------------------------
	protected OVT_LocationTypeFlag GetLocationTypeAtPosition(vector position)
	{
		OVT_TownManagerComponent townManager = OVT_Global.GetTowns();
		if (!townManager)
			return OVT_LocationTypeFlag.OPEN_TERRAIN;
		
		// Check if position is in a town
		OVT_TownData townData = townManager.GetNearestTown(position);
		if (townData && townData.IsWithinTownBounds(position))
		{
			return OVT_LocationTypeFlag.TOWN;
		}
		
		// Check if position is near a base
		OVT_OccupyingFactionManager of = OVT_Global.GetOccupyingFaction();
		if (of)
		{
			OVT_BaseData baseData = of.GetNearestBase(position);
			if (baseData && vector.Distance(position, baseData.location) < 500) // 500m radius for base
			{
				// Could differentiate between different base types here
				return OVT_LocationTypeFlag.BASE;
			}
		}
		
		// Check if position is near water (potential port)
		if (IsPositionNearWater(position))
		{
			return OVT_LocationTypeFlag.PORT;
		}
		
		// Check if position is suitable for airfield
		if (IsPositionSuitableForAirfield(position))
		{
			return OVT_LocationTypeFlag.AIRFIELD;
		}
		
		// Check if position is near an actual radio tower
		OVT_OccupyingFactionManager ofManager2 = OVT_Global.GetOccupyingFaction();
		if (ofManager2)
		{
			foreach (OVT_RadioTowerData towerData : ofManager2.m_RadioTowers)
			{
				if (towerData && vector.Distance(position, towerData.location) < 300) // 300m radius for radio tower
				{
					return OVT_LocationTypeFlag.RADIO_TOWER;
				}
			}
		}
		
		// Check if position could be a checkpoint (between towns/bases)
		// TODO: Implement proper road intersection detection
		// For now, check if it's between major locations
		if (IsPositionBetweenMajorLocations(position))
		{
			return OVT_LocationTypeFlag.CHECKPOINT;
		}
		
		// Default to open terrain
		return OVT_LocationTypeFlag.OPEN_TERRAIN;
	}
	
	//------------------------------------------------------------------------------------------------
	OVT_DeploymentComponent CreateDeployment(OVT_DeploymentConfig config, vector position, int factionIndex)
	{
		if (config)
			Print(string.Format("[Overthrow] Creating deployment '%1' for faction %2", config.m_sDeploymentName, factionIndex), LogLevel.NORMAL);
		
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
		
		string townName = OVT_Global.GetTowns().GetNearestTownName(position);
				
		Print(string.Format("[Overthrow] Created deployment '%1' for faction %2 near %3", config.m_sDeploymentName, factionIndex, townName), LogLevel.NORMAL);
		
		return deployment;
	}
	
	//------------------------------------------------------------------------------------------------
	void RegisterDeployment(OVT_DeploymentComponent deployment)
	{
		if (!deployment)
			return;
			
		EntityID deploymentID = deployment.GetOwner().GetID();
		if (m_aActiveDeployments.Contains(deploymentID))
			return;
		
		m_aActiveDeployments.Insert(deploymentID);
						
		// Add to faction-specific list
		int factionIndex = deployment.GetControllingFaction();
		array<ref EntityID> factionDeployments = m_mFactionDeployments.Get(factionIndex);
		if (factionDeployments && !factionDeployments.Contains(deploymentID))
		{
			factionDeployments.Insert(deploymentID);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void UnregisterDeployment(OVT_DeploymentComponent deployment)
	{
		if (!deployment)
			return;
		
		EntityID deploymentID = deployment.GetOwner().GetID();
		int index = m_aActiveDeployments.Find(deploymentID);
		if (index != -1)
			m_aActiveDeployments.Remove(index);
		
		// Remove from faction-specific list
		int factionIndex = deployment.GetControllingFaction();
		array<ref EntityID> factionDeployments = m_mFactionDeployments.Get(factionIndex);
		if (factionDeployments)
		{
			int deploymentIndex = factionDeployments.Find(deploymentID);
			if (deploymentIndex != -1)
				factionDeployments.Remove(deploymentIndex);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected void CleanupDestroyedDeployments()
	{
		for (int i = m_aActiveDeployments.Count() - 1; i >= 0; i--)
		{
			EntityID deploymentID = m_aActiveDeployments[i];
			IEntity deployment = GetGame().GetWorld().FindEntityByID(deploymentID);
			if (!deployment)
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
		
		foreach (EntityID deploymentID : m_aActiveDeployments)
		{
			IEntity entity = GetGame().GetWorld().FindEntityByID(deploymentID);
			if (!entity)
				continue;
				
			OVT_DeploymentComponent deployment = GetDeploymentFromEntity(entity);
			if (!deployment)
				continue;
				
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
		array<OVT_DeploymentComponent> deployments = new array<OVT_DeploymentComponent>;
		foreach (EntityID deploymentID : m_aActiveDeployments)
		{
			IEntity entity = GetGame().GetWorld().FindEntityByID(deploymentID);
			if (!entity)
				continue;
				
			OVT_DeploymentComponent deployment = GetDeploymentFromEntity(entity);
			if (deployment)
				deployments.Insert(deployment);
		}
		return deployments;
	}
	
	//------------------------------------------------------------------------------------------------
	array<OVT_DeploymentComponent> GetFactionDeployments(int factionIndex)
	{
		array<OVT_DeploymentComponent> deployments = new array<OVT_DeploymentComponent>;
		array<ref EntityID> factionDeploymentIDs = m_mFactionDeployments.Get(factionIndex);
		if (!factionDeploymentIDs)
			return deployments;
			
		foreach (EntityID deploymentID : factionDeploymentIDs)
		{
			IEntity entity = GetGame().GetWorld().FindEntityByID(deploymentID);
			if (!entity)
				continue;
				
			OVT_DeploymentComponent deployment = GetDeploymentFromEntity(entity);
			if (deployment)
				deployments.Insert(deployment);
		}
		return deployments;
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
	void DeleteDeployment(OVT_DeploymentComponent deployment)
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
		
		foreach (int factionIndex, array<ref EntityID> deploymentIDs : m_mFactionDeployments)
		{			
			int resources = m_mFactionResources.Get(factionIndex);
			Print(string.Format("Faction %1: %2 deployments, %3 resources", factionIndex, deploymentIDs.Count(), resources));
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