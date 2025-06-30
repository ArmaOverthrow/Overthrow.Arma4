[ComponentEditorProps(category: "Overthrow/Components/Player", description: "")]
class OVT_PlayerWantedComponentClass: OVT_ComponentClass
{}

class OVT_PlayerWantedComponent: OVT_Component
{	
	[RplProp()]
	protected int m_iWantedLevel = 0;
	[RplProp()]
	protected bool m_bIsSeen = false;	
	[RplProp()]
	float m_fVisualRecognitionFactor = 1;
	[RplProp()]
	float m_bWantedSystemEnabled = true;
	
	[Attribute("250")]
	float m_fBaseDistanceSeenAt;
	
	int m_iWantedTimer = 0;
	int m_iLastSeen;
	
	protected bool m_bTempSeen = false;
	protected float m_fStealthMultiplier = 1.0;
	
	protected const int LAST_SEEN_MAX = 15;
	protected const int WANTED_SYSTEM_FREQUENCY = 1000;
	
	protected FactionAffiliationComponent m_Faction;
	protected SCR_CharacterFactionAffiliationComponent m_CharacterFaction;
	protected BaseWeaponManagerComponent m_Weapon;
	protected SCR_CharacterControllerComponent m_Character;
	protected SCR_CompartmentAccessComponent m_Compartment;
	protected CharacterPerceivableComponent m_Percieve;
	
	
	
	protected ref TraceParam m_TraceParams;
	
	protected ref OVT_PlayerData m_PlayerData;
	protected ref OVT_RecruitData m_RecruitData;
	
	void SetWantedLevel(int level)
	{
		m_iWantedLevel = level;	
		Replication.BumpMe();
	}
	
	void SetBaseWantedLevel(int level, string reason = "")
	{
		if(m_iWantedLevel < level){
			int oldLevel = m_iWantedLevel;
			m_iWantedLevel = level;
			Replication.BumpMe();
			
			// Send notification only when going from 0 to wanted (not when already wanted)
			if(oldLevel == 0 && level > 0 && reason != "")
			{
				SendWantedNotification(reason);
			}
		}		
	}
	
	int GetWantedLevel()
	{
		return m_iWantedLevel;
	}
	
	//! Gets the effective wanted level after applying disguise reduction
	int GetEffectiveWantedLevel()
	{
		// No disguise reduction for now - may add later
		return m_iWantedLevel;
	}
	
	//! Check if player is disguised as occupying faction
	// Track disguise state
	protected bool m_bIsDisguised = false;
	
	bool IsDisguisedAsOccupying()
	{
		return m_bIsDisguised;
	}
	
	//! Check if player is disguised as FIA/resistance
	bool IsDisguisedAsFIA()
	{
		if (!m_CharacterFaction)
			return false;
			
		Faction perceivedFaction = m_CharacterFaction.GetPerceivedFaction();
		if (!perceivedFaction)
			return false;
			
		return perceivedFaction.GetFactionKey() == OVT_Global.GetConfig().GetPlayerFaction().GetFactionKey();
	}
	
	//! Check if player is disguised as supporting faction
	bool IsDisguisedAsSupporting()
	{
		if (!m_CharacterFaction)
			return false;
			
		Faction perceivedFaction = m_CharacterFaction.GetPerceivedFaction();
		if (!perceivedFaction)
			return false;
			
		return perceivedFaction.GetFactionKey() == OVT_Global.GetConfig().GetSupportingFaction().GetFactionKey();
	}
	
	bool IsSeen()
	{
		return m_bIsSeen;
	}
		//! Check if player is visibly carrying a weapon
	bool IsVisiblyArmed()
	{
		if (!m_Weapon)
			return false;
			
		BaseWeaponComponent weapon = m_Weapon.GetCurrentWeapon();
		return weapon != null;
	}
	
	//! Send notification about becoming wanted
	protected void SendWantedNotification(string reason)
	{
		// Only send notifications to players, not recruits
		if (!m_PlayerData)
			return;
			
		OVT_NotificationManagerComponent notificationManager = OVT_NotificationManagerComponent.GetInstance();
		if (!notificationManager)
			return;
			
		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(GetOwner());
		if (playerId > 0)
		{
			notificationManager.SendTextNotification(reason, playerId);
		}
	}
	
	//! Force faction change and perceived faction override when disguise is blown
	void BlowDisguise()
	{
		if (!IsDisguisedAsOccupying())
			return;
			
		// Set disguise as blown
		m_bIsDisguised = false;
		
		// Force faction change
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if(config.m_sPlayerFaction.IsEmpty()) 
			config.m_sPlayerFaction = "FIA";
		
		if (m_Faction)
		{
			m_Faction.SetAffiliatedFactionByKey(config.m_sPlayerFaction);
		}
		
		// Override perceived faction to ensure AI sees us as enemy
		if (m_Percieve)
		{
			FactionManager factionMgr = GetGame().GetFactionManager();
			Faction playerFaction = factionMgr.GetFactionByKey(config.m_sPlayerFaction);
			if (playerFaction)
				m_Percieve.SetPerceivedFactionOverride(playerFaction);
		}
	}

	//! Enable the wanted system for this entity
	void EnableWantedSystem()
	{
		m_bWantedSystemEnabled = true;
	}
	
	//! Disable the wanted system for this entity
	void DisableWantedSystem()
	{
		m_bWantedSystemEnabled = false;
		// Reset wanted level when disabling
		SetWantedLevel(0);
	}
	
	//! Check if wanted system is enabled
	bool IsWantedSystemEnabled()
	{
		return m_bWantedSystemEnabled;
	}
	
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		if(SCR_Global.IsEditMode())
			return;		
		
		m_iWantedTimer = OVT_Global.GetConfig().m_Difficulty.wantedTimeout;
		
		m_Faction = FactionAffiliationComponent.Cast(owner.FindComponent(FactionAffiliationComponent));
		m_CharacterFaction = SCR_CharacterFactionAffiliationComponent.Cast(owner.FindComponent(SCR_CharacterFactionAffiliationComponent));
		m_Weapon = BaseWeaponManagerComponent.Cast(owner.FindComponent(BaseWeaponManagerComponent));
		m_Character = SCR_CharacterControllerComponent.Cast(owner.FindComponent(SCR_CharacterControllerComponent));
		m_Compartment = SCR_CompartmentAccessComponent.Cast(owner.FindComponent(SCR_CompartmentAccessComponent));
		m_Percieve = CharacterPerceivableComponent.Cast(owner.FindComponent(CharacterPerceivableComponent));
						
		if(!GetRpl().IsOwner()) return;
		
		GetGame().GetCallqueue().CallLater(CheckUpdate, WANTED_SYSTEM_FREQUENCY, true, owner);		
		
		OVT_Global.GetOccupyingFaction().m_OnPlayerLoot.Insert(OnPlayerLoot);

	}
	
	void OnPlayerLoot(IEntity player)
	{
		// Only increase wanted level if player is seen while looting
		if(m_bIsSeen)
		{
			SetBaseWantedLevel(2);
			CheckWanted();
			
			// Store suspicious behavior for area heat tracking (Phase 4 enhancement)
			RecordSuspiciousActivity("looting", GetOwner().GetOrigin());
		}
	}
	
	//! Records suspicious activity for area heat tracking
	protected void RecordSuspiciousActivity(string activityType, vector location)
	{
		// Performance-efficient: only record if actually seen by enemies
		if(!m_bIsSeen) return;
		
		// Store area heat in EDF database for persistence
		// This is a lightweight operation that doesn't impact performance
		OVT_TownManagerComponent townManager = OVT_Global.GetTowns();
		if(!townManager) return;
		
		OVT_TownData nearestTown = townManager.GetNearestTown(location);
		if(!nearestTown) return;
		
		// Simple heat level increase - stored with town data
		float currentHeat = nearestTown.GetAreaHeat();
		nearestTown.SetAreaHeat(currentHeat + 0.1); // Small increment per suspicious activity
	}
	
	//! Check detection when disguised as occupying faction
	protected void CheckDisguisedAsOccupying()
	{
		// Get close range distance from difficulty settings (default 15m)
		float closeRangeDistance = 15.0;
		if (OVT_Global.GetConfig() && OVT_Global.GetConfig().m_Difficulty)
		{
			closeRangeDistance = OVT_Global.GetConfig().m_Difficulty.disguiseDetectionDistance;
		}
		
		vector pos = GetOwner().GetOrigin();
		
		array<AIAgent> agents();
		AIWorld aiworld = GetGame().GetAIWorld();
		aiworld.GetAIAgents(agents);
		
		foreach(AIAgent agent : agents)
		{
			AIGroup group = AIGroup.Cast(agent);
			if(!group) continue;
			
			array<AIAgent> groupAgents();
			group.GetAgents(groupAgents);
			
			foreach(AIAgent member : groupAgents)
			{			
				IEntity entity = member.GetControlledEntity();
				if(!entity) continue;
				
				float dist = vector.Distance(entity.GetOrigin(), pos);
				if(dist > closeRangeDistance) continue;
				
				if(FilterEntities(entity))
				{
					// Check if they can see us at close range
					if(TraceLOS(entity, GetOwner()))
					{
						// Busted! Set wanted level 2
						SetBaseWantedLevel(2, "WantedDisguiseBlown");
						m_bTempSeen = true;
						BlowDisguise();
						
						RecordSuspiciousActivity("DISGUISE_BLOWN", pos);
						return;
					}
				}
			}
		}
	}
	
	void CheckUpdate()
	{
		if(!m_bWantedSystemEnabled) return;
		
		// Check if this is a player or recruit
		if(!m_PlayerData && !m_RecruitData)
		{
			int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(GetOwner());
			if(playerId > 0)
			{
				m_PlayerData = OVT_PlayerData.Get(playerId);
			}
			else
			{
				// Check if this is a recruit
				OVT_RecruitManagerComponent recruitManager = OVT_Global.GetRecruits();
				if(recruitManager)
				{
					m_RecruitData = recruitManager.GetRecruitFromEntity(GetOwner());
				}
				
				// If neither player nor recruit, exit
				if(!m_RecruitData)
					return;
			}
		}
		
		string occupyingFactionKey = OVT_Global.GetConfig().GetOccupyingFaction().GetFactionKey();
		string supportingFactionKey = OVT_Global.GetConfig().GetSupportingFaction().GetFactionKey();
		string playerFactionKey = OVT_Global.GetConfig().GetPlayerFaction().GetFactionKey();
		
		m_bTempSeen = false;
		m_iLastSeen = LAST_SEEN_MAX;
		
		m_fVisualRecognitionFactor = m_Percieve.GetVisualRecognitionFactor();
		
		// Check perceived faction for disguise
		bool skipNormalDetection = false;
		if (m_CharacterFaction)
		{
			// Debug: Force recalculation and log outfit faction info
			m_CharacterFaction.RecalculateOutfitFaction();
			
			Faction perceivedFaction = m_CharacterFaction.GetPerceivedFaction();
			Faction defaultFaction = m_CharacterFaction.GetDefaultAffiliatedFaction();
			
			// Additional debug: Check if we have faction affiliation properly set
			Faction affiliatedFaction = m_Faction.GetAffiliatedFaction();
							
			// Debug: Log outfit values
			map<Faction, int> outfitValues = new map<Faction, int>();
			int outfitCount = m_CharacterFaction.GetCharacterOutfitValues(outfitValues);
			
			// Force initial outfit calculation on first run
			if (outfitCount == 0)
			{
				m_CharacterFaction.InitPlayerOutfitFaction_S();
			}
						
			if (perceivedFaction)
			{
				string perceivedKey = perceivedFaction.GetFactionKey();
				
				// If disguised as occupying faction
				if (perceivedKey == occupyingFactionKey)
				{
					// Set disguise state
					m_bIsDisguised = true;
					// Only check close distance (15m default)
					skipNormalDetection = true;
					CheckDisguisedAsOccupying();
				}
				// If perceived as player or supporting faction - instant wanted
				else if (perceivedKey == playerFactionKey || perceivedKey == supportingFactionKey)
				{
					// Not disguised
					m_bIsDisguised = false;
					// Will be handled by normal detection at any distance
					skipNormalDetection = false;
				}
				else
				{
					// Perceived as civilian or other - not disguised
					m_bIsDisguised = false;
				}
			}
			else
			{
				// No perceived faction - not disguised
				m_bIsDisguised = false;
			}
		}
		
		// Always run normal detection to update m_bTempSeen for visibility indicator
		// Even when disguised, players should see if they're being watched
		
		//GetGame().GetWorld().QueryEntitiesBySphere(GetOwner().GetOrigin(), 250, CheckEntity, FilterEntities, EQueryEntitiesFlags.DYNAMIC);
		
		array<AIAgent> agents();
		
		AIWorld aiworld = GetGame().GetAIWorld();
		aiworld.GetAIAgents(agents);
		
		vector pos = GetOwner().GetOrigin();
		
		// Cache stealth multiplier from player or recruit
		m_fStealthMultiplier = 1.0;
		if(m_PlayerData)
		{
			m_fStealthMultiplier = m_PlayerData.stealthMultiplier;
		}
		else if(m_RecruitData)
		{
			// Recruits inherit stealth from their owner's skills
			OVT_PlayerData ownerData = OVT_Global.GetPlayers().GetPlayer(m_RecruitData.m_sOwnerPersistentId);
			if(ownerData)
			{
				m_fStealthMultiplier = ownerData.stealthMultiplier;
			}
		}
		
		float distanceSeen = 5 + (m_fBaseDistanceSeenAt * m_fStealthMultiplier); 
		
		foreach(AIAgent agent : agents)
		{
			AIGroup group = AIGroup.Cast(agent);
			if(!group) continue;
			
			array<AIAgent> groupAgents();
			group.GetAgents(groupAgents);
			
			foreach(AIAgent member : groupAgents)
			{			
				IEntity entity = member.GetControlledEntity();
				if(!entity) continue;
				float dist = vector.Distance(entity.GetOrigin(), pos);
				if(dist > distanceSeen) continue;
				if(FilterEntities(entity))
				{
					CheckEntity(entity);
				}
			}
		}		
						
		// Only increase wanted level for base/tower proximity if not disguised
		if (!m_bIsDisguised)
		{
			OVT_BaseData base = OVT_Global.GetOccupyingFaction().GetNearestBase(GetOwner().GetOrigin());
			if(base && base.IsOccupyingFaction())
			{
				float distanceToBase = vector.Distance(base.location, GetOwner().GetOrigin());
				if(m_iWantedLevel < 2 && distanceToBase < OVT_Global.GetConfig().m_Difficulty.baseCloseRange && m_bTempSeen)
				{
					SetBaseWantedLevel(2, "WantedBaseProximity");
				}		
			}
			
			OVT_RadioTowerData tower = OVT_Global.GetOccupyingFaction().GetNearestRadioTower(GetOwner().GetOrigin());
			if(tower && tower.IsOccupyingFaction())
			{
				float distanceToBase = vector.Distance(tower.location, GetOwner().GetOrigin());
				if(m_iWantedLevel < 2 && distanceToBase < 20 && m_bTempSeen)
				{
					SetBaseWantedLevel(2, "WantedBaseProximity");
				}		
			}
		}
		
		//Print("Last seen is: " + m_iLastSeen);
		
		if(m_iWantedLevel > 0 && !m_bTempSeen)
		{
			m_iWantedTimer -= WANTED_SYSTEM_FREQUENCY;
			//Print("Wanted timeout tick -1");
			if(m_iWantedTimer <= 0)
			{				
				SetWantedLevel(m_iWantedLevel - 1);
				//Print("Downgrading wanted level to " + m_iWantedLevel);
				if(m_iWantedLevel > 1)
				{
					m_iWantedTimer = OVT_Global.GetConfig().m_Difficulty.wantedTimeout;
				}else{
					m_iWantedTimer = OVT_Global.GetConfig().m_Difficulty.wantedOneTimeout;
				}
			}
		}else if(m_iWantedLevel == 1 && m_bTempSeen && !m_bIsDisguised) {
			SetWantedLevel(2);			
		}
		
		CheckWanted();
		
		if(m_bTempSeen != m_bIsSeen)
		{
			m_bIsSeen = m_bTempSeen;
			Replication.BumpMe();
		}
	}
	
	protected void CheckWanted()
	{		
		Faction currentFaction = m_Faction.GetAffiliatedFaction();
		string factionKey = currentFaction.GetFactionKey();
		
		// Handle faction changes based on wanted level and disguise
		bool isDisguised = IsDisguisedAsOccupying();
		
		if (isDisguised && m_iWantedLevel < 2)
		{
			// When disguised and not wanted, temporarily set faction to prevent AI hostility
			string occupyingFaction = OVT_Global.GetConfig().m_sOccupyingFaction;
			if (factionKey != occupyingFaction)
			{
				m_Faction.SetAffiliatedFactionByKey(occupyingFaction);
			}
			// Clear any perceived faction override only when safe (not seen and wanted level 0)
			if (m_Percieve && m_iWantedLevel == 0 && !m_bIsSeen)
			{
				m_Percieve.SetPerceivedFactionOverride(null);
			}
		}
		else if(m_iWantedLevel > 1 && factionKey == "CIV")
		{
			// When wanted and not disguised, set to FIA
			if(OVT_Global.GetConfig().m_sPlayerFaction.IsEmpty()) 
				OVT_Global.GetConfig().m_sPlayerFaction = "FIA";
			m_Faction.SetAffiliatedFactionByKey(OVT_Global.GetConfig().m_sPlayerFaction);
		}
		else if(m_iWantedLevel < 1 && factionKey != "CIV" && !IsDisguisedAsOccupying())
		{
			// When not wanted and not disguised, return to civilian
			m_Faction.SetAffiliatedFactionByKey("CIV");
			// Clear any perceived faction override only when safe (not seen and wanted level 0)
			if (m_Percieve && m_iWantedLevel == 0 && !m_bIsSeen)
			{
				m_Percieve.SetPerceivedFactionOverride(null);
			}
		}
		
	}
	
	bool CheckEntity(IEntity entity)
	{		
		//Search through every nearby enemy AI with a perception component and determine if we:
		//1. Are a known target
		//2. Can be seen in LOS
		//3. If the above is true, are we currently breaking any laws (ie open carrying a weapon)
				
		PerceptionComponent perceptComp = PerceptionComponent.Cast(entity.FindComponent(PerceptionComponent));
		if(!perceptComp) return true;
		
		if(perceptComp.GetTargetCount(ETargetCategory.FRIENDLY) == 0) return true;
		
		autoptr array<BaseTarget> targets = new array<BaseTarget>;
		
		perceptComp.GetTargetsList(targets, ETargetCategory.FRIENDLY);
		
		float dist = vector.Distance(GetOwner().GetOrigin(), entity.GetOrigin());
		
		//Is player in a vehicle?
		bool inVehicle = false;
		if(m_Compartment && m_Compartment.IsInCompartment())
		{		
			//Player is in a vehicle
			inVehicle = true;
		}		
		
		if(m_fVisualRecognitionFactor < 0.2 && dist > (10 * m_fStealthMultiplier) && !inVehicle)
		{
			//Definitely can't see you, but continue search
			return true;
		}
		
		foreach(BaseTarget possibleTarget : targets)
		{		
			IEntity realEnt = possibleTarget.GetTargetEntity();
			
			if (realEnt && realEnt == GetOwner())
			{				
				int lastSeen = possibleTarget.GetTimeSinceSeen();
				
				if(lastSeen < m_iLastSeen) m_iLastSeen = lastSeen;	
				
				int newLevel = m_iWantedLevel;
				
				if(inVehicle)
				{		
					//Player is in a vehicle
					IEntity veh = m_Compartment.GetVehicle();
					if(veh && TraceLOSVehicle(entity, veh)){	
						//Vehicle can be seen					
						m_bTempSeen = true;
					}
				}else{				
					if(TraceLOS(entity, GetOwner())){
						//Player can be seen
						m_bTempSeen = true;	
					}
				}
				
				if(m_bTempSeen && !m_bIsDisguised)
				{
					// Check if player is perceived as hostile faction first
					bool isHostileFaction = false;
					if (m_CharacterFaction)
					{
						Faction perceivedFaction = m_CharacterFaction.GetPerceivedFaction();
						if (perceivedFaction)
						{
							string perceivedKey = perceivedFaction.GetFactionKey();
							string playerFactionKey = OVT_Global.GetConfig().GetPlayerFaction().GetFactionKey();
							string supportingFactionKey = OVT_Global.GetConfig().GetSupportingFaction().GetFactionKey();
							
							if (perceivedKey == playerFactionKey || perceivedKey == supportingFactionKey)
							{
								isHostileFaction = true;
								newLevel = 2; // Instant wanted level 2 for hostile faction
							}
						}
					}
					
					// Only check weapons if not already wanted for being hostile faction
					if (!isHostileFaction && IsVisiblyArmed())
					{
						//Player is brandishing a weapon
						newLevel = 2;
					}	
					//To-Do: check for illegal attire (uniforms, etc)
					
					// Only check vehicle if not already wanted for being hostile faction
					if (!isHostileFaction && inVehicle)
					{
						IEntity veh = m_Compartment.GetVehicle();
						if(veh){
							//Check if vehicle is armed
							SCR_EditableVehicleComponent editable = SCR_EditableVehicleComponent.Cast(veh.FindComponent(SCR_EditableVehicleComponent));
							if(editable)
							{
								SCR_UIInfo uiinfo = editable.GetInfo();
								if(uiinfo)
								{
									SCR_EditableEntityUIInfo info = SCR_EditableEntityUIInfo.Cast(uiinfo);
									if(info)
									{
										autoptr array<EEditableEntityLabel> entityLabels = new array<EEditableEntityLabel>;
										info.GetEntityLabels(entityLabels);
										if(entityLabels.Contains(EEditableEntityLabel.TRAIT_ARMED))
										{
											newLevel = 4;
										}
									}
								}
							}
						}
					}
					
					if(newLevel != m_iWantedLevel)
					{
						// Determine notification reason based on why wanted level increased
						string reason = "";
						if (isHostileFaction)
						{
							reason = "WantedHostileFaction";
						}
						else if (IsVisiblyArmed())
						{
							reason = "WantedWeapon";
						}
						else if (inVehicle && newLevel >= 4)
						{
							reason = "WantedArmedVehicle";
						}
						
						SetBaseWantedLevel(newLevel, reason);						
					}
					
					//We can see and have checked this player, we don't need to continue the search
					return false; 
				}				
			}
		}
		
		//Continue search
		return true;
	}
	
	private bool TraceLOS(IEntity source, IEntity dest)
	{		
		int headBone = source.GetAnimation().GetBoneIndex("head");
		vector matPos[4];
		
		source.GetAnimation().GetBoneMatrix(headBone, matPos);
		vector headPos = source.CoordToParent(matPos[3]);
		
		headBone = dest.GetAnimation().GetBoneIndex("head");		
		dest.GetAnimation().GetBoneMatrix(headBone, matPos);
		vector destHead = dest.CoordToParent(matPos[3]);
		
		
		if (!m_TraceParams)
		{
			m_TraceParams = new TraceParam();		
			m_TraceParams.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
			m_TraceParams.LayerMask =  EPhysicsLayerDefs.Projectile;
		}						
		
		m_TraceParams.Start = headPos;
		m_TraceParams.End = destHead;		
		m_TraceParams.Exclude = source;
			
		float percent = GetGame().GetWorld().TraceMove(m_TraceParams, null);
			
		// If trace hits the target entity or travels the entire path, return true	
		GenericEntity ent = GenericEntity.Cast(m_TraceParams.TraceEnt);
		if (ent)
		{
			if ( ent == dest || ent.GetParent() == dest )
				return true;
		} 
		else if (percent == 1)
			return true;
				
		return false;
	}
	
	private bool TraceLOSVehicle(IEntity source, IEntity dest)
	{		
		int headBone = source.GetAnimation().GetBoneIndex("head");
		vector matPos[4];
		
		source.GetAnimation().GetBoneMatrix(headBone, matPos);
		vector headPos = source.CoordToParent(matPos[3]);		
		
		vector destVeh = dest.GetOrigin();		
		
		if (!m_TraceParams)
		{
			m_TraceParams = new TraceParam();		
			m_TraceParams.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
			m_TraceParams.LayerMask =  EPhysicsLayerDefs.Projectile;
		}						
		
		m_TraceParams.Start = headPos;
		m_TraceParams.End = destVeh;		
		m_TraceParams.Exclude = source;
			
		float percent = GetGame().GetWorld().TraceMove(m_TraceParams, null);
			
		// If trace hits the target entity or travels the entire path, return true	
		GenericEntity ent = GenericEntity.Cast(m_TraceParams.TraceEnt);
		if (ent)
		{
			if ( ent == dest || ent.GetParent() == dest )
				return true;
		} 
		if (percent > 0.99)
			return true;
				
		return false;
	}
	
	bool FilterEntities(IEntity ent) 
	{			
		
		if(ent == GetOwner())
			return false;
		
		DamageManagerComponent dmg = DamageManagerComponent.Cast(ent.FindComponent(DamageManagerComponent));
		if(dmg && dmg.IsDestroyed())
		{
			//Is dead, ignore
			return false;
		}
		
		FactionAffiliationComponent faction = FactionAffiliationComponent.Cast(ent.FindComponent(FactionAffiliationComponent));			
		if(!faction) return false;
		
		Faction currentFaction = faction.GetAffiliatedFaction();
		
		if(currentFaction && currentFaction.GetFactionKey() == OVT_Global.GetConfig().m_sOccupyingFaction)
			return true;
				
		return false;		
	}	
	
	override void OnDelete(IEntity owner)
	{		
		GetGame().GetCallqueue().Remove(CheckUpdate);
	}
	
}
