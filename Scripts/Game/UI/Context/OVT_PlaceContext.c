class OVT_PlaceContext : OVT_UIContext
{
	ref OVT_PlaceMenuWidgets m_Widgets;
	
	protected IEntity m_ePlacingEntity;
	protected ResourceName m_pPlacingPrefab;
	protected OVT_Placeable m_Placeable;
	
	bool m_bPlacing = false;
	int m_iPrefabIndex = 0;
		
	override void PostInit()
	{
		m_Widgets = new OVT_PlaceMenuWidgets();
	}
	
	override void OnFrame(float timeSlice)
	{		
		if (m_bPlacing)	
		{
			m_InputManager.ActivateContext("OverthrowPlaceContext");
			
			if(m_ePlacingEntity)
			{
				m_ePlacingEntity.SetOrigin(GetPlacePosition());
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
		
		m_InputManager.AddActionListener("OverthrowPlace", EActionTrigger.DOWN, DoPlace);
		m_InputManager.AddActionListener("OverthrowRotateLeft", EActionTrigger.DOWN, RotateLeft);
		m_InputManager.AddActionListener("OverthrowRotateRight", EActionTrigger.DOWN, RotateRight);
		m_InputManager.AddActionListener("OverthrowNextItem", EActionTrigger.DOWN, NextItem);
		m_InputManager.AddActionListener("OverthrowPrevItem", EActionTrigger.DOWN, NextItem);
		m_InputManager.AddActionListener("OverthrowPlaceCancel", EActionTrigger.DOWN, Cancel);
	}
	
	override void UnregisterInputs()
	{
		super.UnregisterInputs();
		
		m_InputManager.RemoveActionListener("OverthrowPlace", EActionTrigger.DOWN, DoPlace);
		m_InputManager.RemoveActionListener("OverthrowRotateLeft", EActionTrigger.DOWN, RotateLeft);
		m_InputManager.RemoveActionListener("OverthrowRotateRight", EActionTrigger.DOWN, RotateRight);
		m_InputManager.RemoveActionListener("OverthrowNextItem", EActionTrigger.DOWN, NextItem);
		m_InputManager.RemoveActionListener("OverthrowPrevItem", EActionTrigger.DOWN, NextItem);
		m_InputManager.RemoveActionListener("OverthrowPlaceCancel", EActionTrigger.DOWN, Cancel);
	}
	
	void Cancel(float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		m_bPlacing = false;
		RemoveGhost();
	}
	
	void StartPlace(OVT_Placeable placeable)
	{
		if(!m_Economy.PlayerHasMoney(m_iPlayerID, m_Config.GetPlaceableCost(placeable)))
		{
			ShowHint("#OVT-CannotAfford");
			SCR_UISoundEntity.SoundEvent(UISounds.ERROR);
			return;
		}
		
		if(m_bIsActive) CloseLayout();
		
		m_bPlacing = true;
		m_Placeable = placeable;
		
		m_iPrefabIndex = 0;
		
		SpawnGhost();
	}
	
	protected void SpawnGhost()
	{
		vector pos = GetPlacePosition();
		
		EntitySpawnParams params = EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = pos;
		m_pPlacingPrefab = m_Placeable.m_aPrefabs[m_iPrefabIndex];
		m_ePlacingEntity = GetGame().SpawnEntityPrefabLocal(Resource.Load(m_pPlacingPrefab), null, params);
		SCR_Global.SetMaterial(m_ePlacingEntity, "{E0FECF0FE7457A54}Assets/Editor/PlacingPreview/Preview_03.emat", true);
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
			
			if(!m_Economy.PlayerHasMoney(m_iPlayerID, m_Config.GetPlaceableCost(m_Placeable)))
			{
				ShowHint("#OVT-CannotAfford");
				SCR_UISoundEntity.SoundEvent(UISounds.ERROR);
				return;
			}
			
			EntitySpawnParams params = EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			params.Transform = mat;
			GetGame().SpawnEntityPrefab(Resource.Load(m_pPlacingPrefab), null, params);
			m_Economy.TakePlayerMoney(m_iPlayerID, m_Config.GetPlaceableCost(m_Placeable));
			SCR_UISoundEntity.SoundEvent(UISounds.CLICK);
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
			vector angles = m_ePlacingEntity.GetAngles();
			angles[1] = angles[1] - 1;
			m_ePlacingEntity.SetYawPitchRoll(angles);
		}
	}
	
	void RotateRight(float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		if(m_ePlacingEntity)
		{
			vector angles = m_ePlacingEntity.GetAngles();
			angles[1] = angles[1] + 1;
			m_ePlacingEntity.SetYawPitchRoll(angles);
		}
	}
	
	vector GetPlacePosition()
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		BaseWorld world = GetGame().GetWorld();
		
		float screenW, screenH;
		workspace.GetScreenSize(screenW, screenH);
		vector outDir;
		vector startPos = workspace.ProjScreenToWorldNative(screenW / 2, screenH / 2, outDir, world, -1);
		outDir *= 50;

		autoptr TraceParam trace = new TraceParam();
		trace.Start = startPos;
		trace.End = startPos + outDir;
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.LayerMask = TRACE_LAYER_CAMERA;
		trace.Exclude = m_Owner;
		
		if (startPos[1] > world.GetOceanBaseHeight())
			trace.Flags = trace.Flags | TraceFlags.OCEAN;
		
		float traceDis = world.TraceMove(trace, null);
		
		vector endPos = startPos + outDir * traceDis;
		
		return endPos;
	}
}