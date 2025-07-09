class OVT_PlaceContext : OVT_UIContext
{
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Layout to show when placing", params: "layout")]
	ResourceName m_PlaceLayout;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Layout to show when removing", params: "layout")]
	ResourceName m_RemovalLayout;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Icon to show on remove card", params: "edds")]
	ResourceName m_RemoveIcon;

	protected Widget m_PlaceWidget;
	protected Widget m_RemovalWidget;

	ref OVT_PlaceMenuWidgets m_Widgets;

	protected IEntity m_ePlacingEntity;
	protected ResourceName m_pPlacingPrefab;
	protected OVT_Placeable m_Placeable;

	protected vector[] m_vCurrentTransform[4];

	protected const float TRACE_DIS = 15;
	protected const float MAX_PREVIEW_DIS = 15;
	protected const float MAX_HOUSE_PLACE_DIS = 30;
	protected const float MAX_FOB_PLACE_DIS = 100;
	protected const float MAX_CAMP_PLACE_DIS = 75;
	
	protected OVT_RealEstateManagerComponent m_RealEstate;
	protected OVT_OccupyingFactionManager m_OccupyingFaction;
	protected OVT_ResistanceFactionManager m_Resistance;
	protected OVT_TownManagerComponent m_Towns;
	protected ref OVT_ItemLimitChecker m_ItemLimitChecker;

	bool m_bPlacing = false;
	bool m_bRemovalMode = false;
	protected IEntity m_eHighlightedEntity = null;
	int m_iPrefabIndex = 0;
	int m_iPageNum = 0;
	int m_iNumPages = 0;

	override void PostInit()
	{
		m_Widgets = new OVT_PlaceMenuWidgets();
		m_RealEstate = OVT_Global.GetRealEstate();
		m_OccupyingFaction = OVT_Global.GetOccupyingFaction();
		m_Resistance = OVT_Global.GetResistanceFaction();
		m_Towns = OVT_Global.GetTowns();
		m_ItemLimitChecker = new OVT_ItemLimitChecker();
	}

	override void OnFrame(float timeSlice)
	{
		if (m_bPlacing)
		{
			m_InputManager.ActivateContext("OverthrowPlaceContext");

			if(m_ePlacingEntity)
			{
				vector normal = vector.Zero;
				m_ePlacingEntity.SetOrigin(GetPlacePosition(normal));
				m_ePlacingEntity.GetTransform(m_vCurrentTransform);
				if(m_Placeable.m_bPlaceOnWall)
				{
					vector ypr = m_ePlacingEntity.GetYawPitchRoll();
					ypr[0] = normal.ToYaw();
					ypr[1] = 0;
					m_ePlacingEntity.SetYawPitchRoll(ypr);
				}
				m_ePlacingEntity.Update();
			}
		}
		
		if (m_bRemovalMode)
		{
			m_InputManager.ActivateContext("OverthrowPlaceContext");
			HighlightRemovableItems();
		}
	}

	override void OnShow()
	{
		m_Widgets.Init(m_wRoot);
		m_iPageNum = 0;
		
		// Set up the previous button
		Widget prevButton = m_wRoot.FindAnyWidget("PrevButton");
		SCR_InputButtonComponent btn = SCR_InputButtonComponent.Cast(prevButton.FindHandler(SCR_InputButtonComponent));
		btn.m_OnActivated.Insert(PreviousPage);
		
		// Set up the next button
		Widget nextButton = m_wRoot.FindAnyWidget("NextButton");
		btn = SCR_InputButtonComponent.Cast(nextButton.FindHandler(SCR_InputButtonComponent));
		btn.m_OnActivated.Insert(NextPage);
		
		// Set up the close button
		Widget closeButton = m_wRoot.FindAnyWidget("CloseButton");
		btn = SCR_InputButtonComponent.Cast(closeButton.FindHandler(SCR_InputButtonComponent));		
		btn.m_OnActivated.Insert(CloseLayout);
		
		
		Refresh();
	}
	
	void PreviousPage()
	{
		if(!m_wRoot) return;
		m_iPageNum--;
		if(m_iPageNum < 0) m_iPageNum = 0;
		
		Refresh();
	}
	
	void NextPage()
	{
		if(!m_wRoot) return;
		m_iPageNum++;
		if(m_iPageNum > m_iNumPages-1) m_iPageNum = m_iNumPages-1;
		
		Refresh();
	}
	
	void Refresh()
	{
		if(!m_wRoot) return;
		
		TextWidget pages = TextWidget.Cast(m_wRoot.FindAnyWidget("Pages"));
		
		int done = 0;
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		
		// Show Remove card as first item
		Widget removeCard = m_Widgets.m_BrowserGrid.FindWidget("PlaceMenu_Card" + done);
		OVT_PlaceMenuCardComponent removeCardComponent = OVT_PlaceMenuCardComponent.Cast(removeCard.FindHandler(OVT_PlaceMenuCardComponent));
		if(removeCardComponent)
		{
			removeCardComponent.InitRemoveCard(this, m_RemoveIcon);
		}
		removeCard.SetOpacity(1);
		done++;
		
		// Calculate pages based on remaining items
		int totalPlaceables = m_Resistance.m_PlaceablesConfig.m_aPlaceables.Count();
		m_iNumPages = Math.Ceil(totalPlaceables / 14); // 14 items per page (leaving room for remove card)
		if(m_iPageNum >= m_iNumPages) m_iPageNum = 0;
		string pageNumText = (m_iPageNum + 1).ToString();
		
		pages.SetText(pageNumText + "/" + m_iNumPages);
		
		// Show placeables for current page (14 items instead of 15)
		for(int i = m_iPageNum * 14; i < (m_iPageNum + 1) * 14 && i < m_Resistance.m_PlaceablesConfig.m_aPlaceables.Count(); i++)
		{
			OVT_Placeable placeable = m_Resistance.m_PlaceablesConfig.m_aPlaceables[i];
			Widget w = m_Widgets.m_BrowserGrid.FindWidget("PlaceMenu_Card" + done);
			OVT_PlaceMenuCardComponent card = OVT_PlaceMenuCardComponent.Cast(w.FindHandler(OVT_PlaceMenuCardComponent));

			// Check if placeable can be placed at current location
			string reason;
			bool canPlace = CanPlace(placeable, player.GetOrigin(), reason);
			
			card.Init(placeable, this, canPlace, reason);
			w.SetOpacity(1);

			done++;
		}

		// Hide unused cards
		for(int i=done; i < 15; i++)
		{
			Widget w = m_Widgets.m_BrowserGrid.FindWidget("PlaceMenu_Card" + i);
			w.SetOpacity(0);
		}
	}

	override void RegisterInputs()
	{
		super.RegisterInputs();
		if(!m_InputManager) return;

		m_InputManager.AddActionListener("MenuSelect", EActionTrigger.DOWN, DoPlace);
		m_InputManager.AddActionListener("OverthrowRotateLeft", EActionTrigger.PRESSED, RotateLeft);
		m_InputManager.AddActionListener("OverthrowRotateRight", EActionTrigger.PRESSED, RotateRight);
		m_InputManager.AddActionListener("OverthrowNextItem", EActionTrigger.DOWN, NextItem);
		m_InputManager.AddActionListener("OverthrowPrevItem", EActionTrigger.DOWN, PrevItem);
		m_InputManager.AddActionListener("MenuBack", EActionTrigger.DOWN, Cancel);
	}

	override void UnregisterInputs()
	{
		super.UnregisterInputs();
		if(!m_InputManager) return;

		m_InputManager.RemoveActionListener("MenuSelect", EActionTrigger.DOWN, DoPlace);
		m_InputManager.RemoveActionListener("OverthrowRotateLeft", EActionTrigger.PRESSED, RotateLeft);
		m_InputManager.RemoveActionListener("OverthrowRotateRight", EActionTrigger.PRESSED, RotateRight);
		m_InputManager.RemoveActionListener("OverthrowNextItem", EActionTrigger.DOWN, NextItem);
		m_InputManager.RemoveActionListener("OverthrowPrevItem", EActionTrigger.DOWN, PrevItem);
		m_InputManager.RemoveActionListener("MenuBack", EActionTrigger.DOWN, Cancel);
	}

	void Cancel(float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		if(m_bPlacing)
		{
			m_bPlacing = false;
			RemoveGhost();
			if(m_PlaceWidget)
				m_PlaceWidget.RemoveFromHierarchy();
			return;
		}
		
		if(m_bRemovalMode)
		{
			m_bRemovalMode = false;
			ClearHighlights();
			if(m_RemovalWidget)
				m_RemovalWidget.RemoveFromHierarchy();
			return;
		}
	}


	bool CanPlace(OVT_Placeable placeable, vector pos, out string reason)
	{
		reason = "#OVT-CannotPlaceHere";
		if(placeable.m_bIgnoreLocation) return true;
		
		if(!m_ItemLimitChecker.CanPlaceItem(pos, m_sPlayerID, reason))
		{
			return false;
		}

		float dist;
		OVT_TownData town = m_Towns.GetNearestTown(pos);
		
		if(placeable.m_bAwayFromCamps)
		{
			OVT_CampData camp = m_Resistance.GetNearestCampData(pos);
			if(camp)
			{
				dist = vector.Distance(camp.location, pos);
				if(dist < 100) // 100m minimum distance from other camps
				{
					reason = "#OVT-TooCloseCamp";
					return false;
				}
			}

			return true;
		}

		if(placeable.m_bAwayFromBases)
		{
			OVT_BaseData base = m_OccupyingFaction.GetNearestBase(pos);
			dist = vector.Distance(base.location,pos);
			if(dist < OVT_Global.GetConfig().m_Difficulty.baseRange)
			{
				reason = "#OVT-TooCloseBase";
				return false;
			}			

			return true;
		}
		
		if(placeable.m_bAwayFromTownsBases)
		{
			IEntity building = m_RealEstate.GetNearestBuilding(pos, MAX_HOUSE_PLACE_DIS);
			if(building)
			{
				dist = vector.Distance(building.GetOrigin(), pos);
				if(dist < MAX_HOUSE_PLACE_DIS)
				{
					reason = "#OVT-TooCloseBuilding";
					return false;
				}
			}

			OVT_BaseData base = m_OccupyingFaction.GetNearestBase(pos);


			dist = vector.Distance(base.location,pos);
			if(dist < OVT_Global.GetConfig().m_Difficulty.baseRange)
			{
				reason = "#OVT-TooCloseBase";
				return false;
			}

			//Smaller town ranges for the "too close" option
			//Allows camps a bit closer
			int townRange = m_Towns.m_iCityRange - 400;
			if(town.size < 3) townRange = m_Towns.m_iTownRange - 200;
			if(town.size < 2) townRange = m_Towns.m_iVillageRange - 50;

			dist = vector.Distance(town.location,pos);
			if(dist < townRange)
			{
				reason = "#OVT-TooCloseTown";
				return false;
			}
			
			vector fob = m_Resistance.GetNearestCamp(pos);	
			if(fob[0] != 0)
			{
				dist = vector.Distance(fob, pos);
				if(dist < 250)
				{
					reason = "#OVT-TooCloseCamp";
					return false;
				}
			}

			return true;
		}

		if(placeable.m_bAwayFromBases)
		{
			OVT_BaseData base = m_OccupyingFaction.GetNearestBase(pos);
			
			dist = vector.Distance(base.location,pos);
			if(dist < OVT_Global.GetConfig().m_Difficulty.baseRange)
			{
				reason = "#OVT-TooCloseBase";
				return false;
			}

			return true;
		}

		if(placeable.m_bNearTown)
		{
			dist = vector.Distance(town.location,pos);
			if(dist > m_Towns.GetTownRange(town))
			{
				reason = "#OVT-MustBeNearTown";
				return false;
			}else{
				return true;
			}
		}

		IEntity house = m_RealEstate.GetNearestOwned(m_sPlayerID, pos);
		if(house)
		{
			dist = vector.Distance(house.GetOrigin(), pos);
			if(dist < MAX_HOUSE_PLACE_DIS) return true;
		}
		
		OVT_CampData camp = m_Resistance.GetNearestCampData(pos);	
		if(camp)
		{	
			dist = vector.Distance(camp.location, pos);
			if(dist < MAX_CAMP_PLACE_DIS && camp.owner == m_sPlayerID) return true;	
		}
		
		OVT_FOBData fob = m_Resistance.GetNearestFOBData(pos);	
		if(fob)
		{	
			dist = vector.Distance(fob.location, pos);
			if(dist < MAX_FOB_PLACE_DIS) return true;	
		}
		
		OVT_BaseData base = m_OccupyingFaction.GetNearestBase(pos);
		dist = vector.Distance(base.location,pos);
		if(!base.IsOccupyingFaction() && dist < OVT_Global.GetConfig().m_Difficulty.baseRange)
		{
			return true;
		}

		return false;
	}

	void StartPlace(OVT_Placeable placeable)
	{
		if(m_bIsActive) CloseLayout();
		
		// Already placing something, remove the old one
		if (m_bPlacing)
		{
			RemoveGhost();
			m_bPlacing = false;
		}

		IEntity player = SCR_PlayerController.GetLocalControlledEntity();

		m_Placeable = placeable;

		string reason;
		if(!CanPlace(m_Placeable, player.GetOrigin(), reason))
		{
			ShowHint(reason);
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.ERROR);
			return;
		}

		if(!m_Economy.PlayerHasMoney(m_sPlayerID, OVT_Global.GetConfig().GetPlaceableCost(placeable)))
		{
			ShowHint("#OVT-CannotAfford");
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.ERROR);
			return;
		}

		if (!m_PlaceWidget)
		{
			WorkspaceWidget workspace = GetGame().GetWorkspace();
			m_PlaceWidget = workspace.CreateWidgets(m_PlaceLayout);
		}

		m_bPlacing = true;
		if (m_Placeable.m_bRandomizePrefab && m_Placeable.m_aPrefabs.Count() > 1)
		{
			m_iPrefabIndex = Math.RandomInt(0, m_Placeable.m_aPrefabs.Count());
		}
		else
		{
			m_iPrefabIndex = 0;
		}

		SpawnGhost();
	}

	protected void SpawnGhost()
	{
		vector normal = vector.Zero;
		vector pos = GetPlacePosition(normal);

		m_pPlacingPrefab = m_Placeable.m_aPrefabs[m_iPrefabIndex];
		m_ePlacingEntity = OVT_Global.SpawnEntityPrefab(m_pPlacingPrefab, pos, "0 0 0", false);

		if(m_vCurrentTransform)
		{
			m_ePlacingEntity.SetTransform(m_vCurrentTransform);
		}
		//SCR_Global.SetMaterial(m_ePlacingEntity, "{E0FECF0FE7457A54}Assets/Editor/PlacingPreview/Preview_03.emat", true);

		Physics phys = m_ePlacingEntity.GetPhysics();
		if(phys)
		{
			phys.SetActive(0);
		}

		m_ePlacingEntity.SetFlags(EntityFlags.VISIBLE, true);

		if(m_Placeable.m_bPlaceOnWall)
		{
			vector ypr = Vector(normal.ToYaw(), 0, 0);
			m_ePlacingEntity.SetYawPitchRoll(ypr);
		}
	}

	protected void RemoveGhost()
	{
		SCR_EntityHelper.DeleteEntityAndChildren(m_ePlacingEntity);
		m_ePlacingEntity = null;
	}

	void DoPlace(float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		if(m_bRemovalMode)
		{
			DoRemove();
			return;
		}
		
		if(!m_bPlacing) return;

		int cost = OVT_Global.GetConfig().GetPlaceableCost(m_Placeable);
		vector mat[4];

		if(m_ePlacingEntity)
		{
			m_ePlacingEntity.GetTransform(mat);
			RemoveGhost();
			string error;
			if(!CanPlace(m_Placeable, mat[3], error))
			{
				ShowHint(error);
				SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.ERROR);
				
				// Close layout if item limit was reached during placement attempt
				if(error == "#OVT-ItemLimitReached")
				{
					CloseLayout();
				}
				return;
			}

			if(!m_Economy.PlayerHasMoney(m_sPlayerID, cost))
			{
				ShowHint("#OVT-CannotAfford");
				SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.ERROR);
				return;
			}

			vector angles = Math3D.MatrixToAngles(mat);
			int placeableIndex = m_Resistance.m_PlaceablesConfig.m_aPlaceables.Find(m_Placeable);
			int prefabIndex = m_Placeable.m_aPrefabs.Find(m_pPlacingPrefab);
			OVT_Global.GetServer().PlaceItem(placeableIndex, prefabIndex, mat[3], angles, m_iPlayerID);
						
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.CLICK);
		}

		if(m_Economy.PlayerHasMoney(m_sPlayerID, cost))
		{
			if (m_Placeable.m_bRandomizePrefab && m_Placeable.m_aPrefabs.Count() > 1)
			{
				m_iPrefabIndex = Math.RandomInt(0, m_Placeable.m_aPrefabs.Count());
			}
			SpawnGhost(); //Start all over again
		}else{
			Cancel();
		}
	}

	void NextItem(float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		if(!m_bPlacing) return;
		int newIndex = m_iPrefabIndex + 1;
		if(newIndex > m_Placeable.m_aPrefabs.Count() - 1)
		{
			newIndex = 0;
		}

		if(newIndex != m_iPrefabIndex)
		{
			RemoveGhost();
			m_iPrefabIndex = newIndex;
			SpawnGhost();
		}
	}

	void PrevItem(float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		if(!m_bPlacing) return;
		int newIndex = m_iPrefabIndex - 1;
		if(newIndex < 0)
		{
			newIndex = m_Placeable.m_aPrefabs.Count() - 1;
		}

		if(newIndex != m_iPrefabIndex)
		{
			RemoveGhost();
			m_iPrefabIndex = newIndex;
			SpawnGhost();
		}
	}

	void RotateLeft(float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		if(m_ePlacingEntity)
		{
			vector angles = m_ePlacingEntity.GetYawPitchRoll();
			if(m_Placeable.m_bPlaceOnWall)
			{
				angles[2] = angles[2] + 1;
			}else{
				angles[0] = angles[0] - 1;
			}
			m_ePlacingEntity.SetYawPitchRoll(angles);
		}
	}

	void RotateRight(float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		if(m_ePlacingEntity)
		{
			vector angles = m_ePlacingEntity.GetYawPitchRoll();
			if(m_Placeable.m_bPlaceOnWall)
			{
				angles[2] = angles[2] - 1;
			}else{
				angles[0] = angles[0] + 1;
			}
			m_ePlacingEntity.SetYawPitchRoll(angles);
		}
	}

	vector GetPlacePosition(out vector normal)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		BaseWorld world = GetGame().GetWorld();

		float screenW, screenH;
		workspace.GetScreenSize(screenW, screenH);
		vector cameraDir;
		vector cameraPos = workspace.ProjScreenToWorldNative(screenW / 2, screenH / 2, cameraDir, world, -1);

		//--- Find object/ground intersection, or use maximum distance when none is found
		float traceDis = GetTraceDis(cameraPos, cameraDir * TRACE_DIS, cameraPos[1], normal);
		if (traceDis == 1)
			traceDis = MAX_PREVIEW_DIS;
		else
			traceDis *= TRACE_DIS;

		vector endPos = cameraPos + cameraDir * traceDis;

		return endPos;
	}

	protected float GetTraceDis(vector pos, vector dir, float cameraHeight, out vector hitNormal)
	{
		BaseWorld world = GetGame().GetWorld();
		autoptr TraceParam trace = new TraceParam();
		trace.Start = pos;
		trace.End = trace.Start + dir;
		if (cameraHeight >= world.GetOceanBaseHeight())
			trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS | TraceFlags.OCEAN;
		else
			trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS; //--- Don't check for water intersection when under water
		trace.Exclude = m_ePlacingEntity;

		float dis = world.TraceMove(trace, SCR_Global.FilterCallback_IgnoreCharacters);
		hitNormal = trace.TraceNorm;

		return dis;
	}
	
	//! Start removal mode
	void StartRemovalMode()
	{
		if(m_bIsActive) CloseLayout();
		
		m_bRemovalMode = true;
		m_bPlacing = false;
		
		if(m_ePlacingEntity)
		{
			RemoveGhost();
		}
		
		if (!m_RemovalWidget && m_RemovalLayout != "")
		{
			WorkspaceWidget workspace = GetGame().GetWorkspace();
			m_RemovalWidget = workspace.CreateWidgets(m_RemovalLayout);
		}
		
		ShowHint("#OVT-RemovalModeActive");
	}
	
	//! Highlight the item the player is looking at (if removable)
	void HighlightRemovableItems()
	{
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		if(!player) return;
		
		// Raycast to find what the player is looking at
		vector cameraPos, cameraDir;
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		BaseWorld world = GetGame().GetWorld();
		
		float screenW, screenH;
		workspace.GetScreenSize(screenW, screenH);
		cameraPos = workspace.ProjScreenToWorldNative(screenW / 2, screenH / 2, cameraDir, world, -1);
		
		// Trace to find what the player is looking at
		autoptr TraceParam trace = new TraceParam();
		trace.Start = cameraPos;
		trace.End = cameraPos + cameraDir * 50; // 50m range
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		
		float traceDis = world.TraceMove(trace, null);
		if(traceDis < 1)
		{
			IEntity hitEntity = trace.TraceEnt;
			if(hitEntity)
			{
				OVT_PlaceableComponent placeableComp = OVT_PlaceableComponent.Cast(hitEntity.FindComponent(OVT_PlaceableComponent));
				if(placeableComp && CanRemoveItem(placeableComp))
				{
					// Only highlight if it's a different entity
					if(hitEntity != m_eHighlightedEntity)
					{
						ClearHighlights();
						m_eHighlightedEntity = hitEntity;
						// Apply red highlighting using the CannotBuild material for consistency
						SCR_Global.SetMaterial(hitEntity, "{14A9DCEA57D1C381}Assets/Conflict/CannotBuild.emat");
					}
					return;
				}
			}
		}
		
		// No valid target found, clear highlights
		ClearHighlights();
	}
	
	//! Check if player can remove an item
	bool CanRemoveItem(OVT_PlaceableComponent placeableComp)
	{
		// Officers can remove any item
		OVT_PlayerData player = OVT_Global.GetPlayers().GetPlayer(m_sPlayerID);
		if(player && player.isOfficer)
			return true;
		
		// Players can remove their own items
		if(placeableComp.GetOwnerPersistentId() == m_sPlayerID)
			return true;
		
		return false;
	}
	
	//! Remove the item the player is looking at
	void DoRemove()
	{
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		if(!player) return;
		
		// Raycast to find the item the player is looking at
		vector cameraPos, cameraDir;
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		BaseWorld world = GetGame().GetWorld();
		
		float screenW, screenH;
		workspace.GetScreenSize(screenW, screenH);
		cameraPos = workspace.ProjScreenToWorldNative(screenW / 2, screenH / 2, cameraDir, world, -1);
		
		// Trace to find what the player is looking at
		autoptr TraceParam trace = new TraceParam();
		trace.Start = cameraPos;
		trace.End = cameraPos + cameraDir * 50; // 50m range
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		
		float traceDis = world.TraceMove(trace, null);
		if(traceDis < 1)
		{
			IEntity hitEntity = trace.TraceEnt;
			if(hitEntity)
			{
				OVT_PlaceableComponent placeableComp = OVT_PlaceableComponent.Cast(hitEntity.FindComponent(OVT_PlaceableComponent));
				if(placeableComp && CanRemoveItem(placeableComp))
				{
					// Send removal request to server
					OVT_Global.GetServer().RemovePlacedItem(hitEntity.GetID(), m_iPlayerID);
					ShowHint("#OVT-ItemRemoved");
					SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.CLICK);
				}
				else
				{
					ShowHint("#OVT-CannotRemoveItem");
					SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.ERROR);
				}
			}
		}
	}
	
	//! Clear highlighting from all items
	void ClearHighlights()
	{
		if(m_eHighlightedEntity)
		{
			// Reset material to empty string to restore original
			SCR_Global.SetMaterial(m_eHighlightedEntity, "");
			m_eHighlightedEntity = null;
		}
	}
}
