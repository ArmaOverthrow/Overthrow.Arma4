class OVT_PlaceContext : OVT_UIContext
{
	ref OVT_PlaceMenuWidgets m_Widgets;
	
	protected IEntity m_ePlacingEntity;
	protected ResourceName m_pPlacingPrefab;
	protected OVT_Placeable m_Placeable;
	
	protected const float TRACE_DIS = 50;
	protected const float MAX_PREVIEW_DIS = 50;
	protected const float MAX_HOUSE_PLACE_DIS = 30;
	
	protected OVT_RealEstateManagerComponent m_RealEstate;
	
	bool m_bPlacing = false;
	int m_iPrefabIndex = 0;
		
	override void PostInit()
	{
		m_Widgets = new OVT_PlaceMenuWidgets();
		m_RealEstate = OVT_Global.GetRealEstate();
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
	}
	
	override void OnShow()
	{		
		m_Widgets.Init(m_wRoot);
				
		int done = 0;
		foreach(int i, OVT_Placeable placeable : m_Config.m_aPlaceables)
		{
			Widget w = m_Widgets.m_BrowserGrid.FindWidget("PlaceMenu_Card" + i);
			OVT_PlaceMenuCardComponent card = OVT_PlaceMenuCardComponent.Cast(w.FindHandler(OVT_PlaceMenuCardComponent));
			
			card.Init(placeable, this);
			
			done++;
		}
		
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
		
		m_InputManager.AddActionListener("OverthrowPlace", EActionTrigger.DOWN, DoPlace);
		m_InputManager.AddActionListener("OverthrowRotateLeft", EActionTrigger.PRESSED, RotateLeft);
		m_InputManager.AddActionListener("OverthrowRotateRight", EActionTrigger.PRESSED, RotateRight);
		m_InputManager.AddActionListener("OverthrowNextItem", EActionTrigger.DOWN, NextItem);
		m_InputManager.AddActionListener("OverthrowPrevItem", EActionTrigger.DOWN, NextItem);
		m_InputManager.AddActionListener("OverthrowPlaceCancel", EActionTrigger.DOWN, Cancel);
	}
	
	override void UnregisterInputs()
	{
		super.UnregisterInputs();
		if(!m_InputManager) return;
		
		m_InputManager.RemoveActionListener("OverthrowPlace", EActionTrigger.DOWN, DoPlace);
		m_InputManager.RemoveActionListener("OverthrowRotateLeft", EActionTrigger.PRESSED, RotateLeft);
		m_InputManager.RemoveActionListener("OverthrowRotateRight", EActionTrigger.PRESSED, RotateRight);
		m_InputManager.RemoveActionListener("OverthrowNextItem", EActionTrigger.DOWN, NextItem);
		m_InputManager.RemoveActionListener("OverthrowPrevItem", EActionTrigger.DOWN, NextItem);
		m_InputManager.RemoveActionListener("OverthrowPlaceCancel", EActionTrigger.DOWN, Cancel);
	}
	
	void Cancel(float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		m_bPlacing = false;
		RemoveGhost();
	}
	
	bool CanPlace(vector pos)
	{
		if(m_Placeable.m_bIgnoreLocation) return true;
		
		IEntity house = m_RealEstate.GetNearestOwned(m_sPlayerID, pos);
		float dist = vector.Distance(house.GetOrigin(), pos);
		
		if(dist < MAX_HOUSE_PLACE_DIS) return true;
		
		return false;
	}
	
	void StartPlace(OVT_Placeable placeable)
	{
		if(m_bIsActive) CloseLayout();
				
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		
		m_Placeable = placeable;
		
		if(!CanPlace(player.GetOrigin()))
		{
			ShowHint("#OVT-CannotPlaceHere");
			SCR_UISoundEntity.SoundEvent(UISounds.ERROR);
			return;
		}
		
		if(!m_Economy.PlayerHasMoney(m_sPlayerID, m_Config.GetPlaceableCost(placeable)))
		{
			ShowHint("#OVT-CannotAfford");
			SCR_UISoundEntity.SoundEvent(UISounds.ERROR);
			return;
		}
		
		
		
		m_bPlacing = true;
		
		
		m_iPrefabIndex = 0;
		
		SpawnGhost();
	}
	
	protected void SpawnGhost()
	{
		vector normal = vector.Zero;
		vector pos = GetPlacePosition(normal);
				
		EntitySpawnParams params = EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = pos;
		m_pPlacingPrefab = m_Placeable.m_aPrefabs[m_iPrefabIndex];
		m_ePlacingEntity = GetGame().SpawnEntityPrefabLocal(Resource.Load(m_pPlacingPrefab), null, params);
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
		SCR_Global.DeleteEntityAndChildren(m_ePlacingEntity);
		m_ePlacingEntity = null;
	}
	
	void DoPlace(float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		m_bPlacing = false;
		
		if(m_ePlacingEntity)
		{
			vector mat[4];
			m_ePlacingEntity.GetTransform(mat);
			RemoveGhost();
			
			if(!CanPlace(mat[3]))
			{
				ShowHint("#OVT-CannotPlaceHere");
				SCR_UISoundEntity.SoundEvent(UISounds.ERROR);
				return;
			}
			
			if(!m_Economy.PlayerHasMoney(m_sPlayerID, m_Config.GetPlaceableCost(m_Placeable)))
			{
				ShowHint("#OVT-CannotAfford");
				SCR_UISoundEntity.SoundEvent(UISounds.ERROR);
				return;
			}
			
			EntitySpawnParams params = EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			params.Transform = mat;
			IEntity entity = GetGame().SpawnEntityPrefab(Resource.Load(m_pPlacingPrefab), null, params);
			m_Economy.TakePlayerMoney(m_iPlayerID, m_Config.GetPlaceableCost(m_Placeable));
			SCR_UISoundEntity.SoundEvent(UISounds.CLICK);
			
			if(m_Placeable.handler)
			{
				m_Placeable.handler.OnPlace(entity, m_iPlayerID);
			}
		}
	}
	
	void NextItem(float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
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
}