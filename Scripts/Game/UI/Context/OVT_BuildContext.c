class OVT_BuildContext : OVT_UIContext
{
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Layout to show when building", params: "layout")]
	ResourceName m_BuildLayout;	
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Build Camera Prefab", params: "et")]
	ResourceName m_BuildCameraPrefab;
	
	SCR_CameraBase m_Camera;
	CameraBase m_PlayerCamera;
	
	Widget m_BuildWidget;
		
	protected IEntity m_eBuildingEntity;
	protected ResourceName m_pBuildingPrefab;
	protected OVT_Buildable m_Buildable;
	
	protected vector[] m_vCurrentTransform[4];
		
	protected OVT_RealEstateManagerComponent m_RealEstate;
	protected OVT_OccupyingFactionManager m_OccupyingFaction;
	protected OVT_ResistanceFactionManager m_Resistance;
	protected OVT_TownManagerComponent m_Towns;
	protected CameraManager m_CameraManager;
	
	const int MAX_FOB_BUILD_DIS = 100;
	
	bool m_bBuilding = false;
	int m_iPrefabIndex = 0;
		
	override void PostInit()
	{
		m_RealEstate = OVT_Global.GetRealEstate();
		m_OccupyingFaction = OVT_Global.GetOccupyingFaction();
		m_Resistance = OVT_Global.GetResistanceFaction();
		m_Towns = OVT_Global.GetTowns();		
		m_CameraManager = GetGame().GetCameraManager();
	}
	
	override void OnFrame(float timeSlice)
	{		
		if (m_bBuilding && m_Camera)	
		{
			if (m_CameraManager && m_CameraManager.CurrentCamera() != m_Camera)
			{
				m_CameraManager.SetCamera(m_Camera);
			}
			
			m_InputManager.ActivateContext("OverthrowBuildContext");
			
			if(m_eBuildingEntity)
			{
				vector normal = vector.Zero;				
				m_eBuildingEntity.SetOrigin(GetBuildPosition(normal));
				m_eBuildingEntity.GetTransform(m_vCurrentTransform);				
				m_eBuildingEntity.Update();			
			}
		}
	}
	
	override void OnShow()
	{			
				
		int done = 0;
		array<OVT_Buildable> valid = new array<OVT_Buildable>;
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
				
		string reason;		
		foreach(int i, OVT_Buildable buildable : m_Resistance.m_BuildablesConfig.m_aBuildables)
		{
			if(CanBuild(buildable, player.GetOrigin(), reason))
			{
				valid.Insert(buildable);
			} 
		}
		
		if(valid.Count() == 0)
		{			
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.ERROR);
			CloseLayout();
			ShowHint("#OVT-CannotBuildAnythingHere");
			return;
		}
		
		Widget root = m_wRoot.FindAnyWidget("m_BrowserGrid");
		
		foreach(int i, OVT_Buildable buildable : valid)
		{
			Widget w = root.FindWidget("BuildMenu_Card" + i);
			OVT_BuildMenuCardComponent card = OVT_BuildMenuCardComponent.Cast(w.FindHandler(OVT_BuildMenuCardComponent));
			
			card.Init(buildable, this);
			
			done++;
		}
		
		for(int i=done; i < 15; i++)
		{
			Widget w = root.FindWidget("BuildMenu_Card" + i);
			w.SetOpacity(0);
		}
	}
	
	override void RegisterInputs()
	{
		super.RegisterInputs();
		if(!m_InputManager) return;
		
		m_InputManager.AddActionListener("CharacterFire", EActionTrigger.DOWN, DoBuild);
		m_InputManager.AddActionListener("OverthrowRotateLeft", EActionTrigger.PRESSED, RotateLeft);
		m_InputManager.AddActionListener("OverthrowRotateRight", EActionTrigger.PRESSED, RotateRight);
		m_InputManager.AddActionListener("OverthrowNextItem", EActionTrigger.DOWN, NextItem);
		m_InputManager.AddActionListener("OverthrowPrevItem", EActionTrigger.DOWN, PrevItem);
		m_InputManager.AddActionListener("MenuBack", EActionTrigger.DOWN, Cancel);
		
		m_InputManager.AddActionListener("CharacterRight", EActionTrigger.VALUE, MoveRight);
		m_InputManager.AddActionListener("CharacterForward", EActionTrigger.VALUE, MoveForward);
		m_InputManager.AddActionListener("Inventory_InspectZoom", EActionTrigger.VALUE, Zoom);
	}
	
	override void UnregisterInputs()
	{
		super.UnregisterInputs();
		if(!m_InputManager) return;
		
		m_InputManager.RemoveActionListener("CharacterFire", EActionTrigger.DOWN, DoBuild);
		m_InputManager.RemoveActionListener("OverthrowRotateLeft", EActionTrigger.PRESSED, RotateLeft);
		m_InputManager.RemoveActionListener("OverthrowRotateRight", EActionTrigger.PRESSED, RotateRight);
		m_InputManager.RemoveActionListener("OverthrowNextItem", EActionTrigger.DOWN, NextItem);
		m_InputManager.RemoveActionListener("OverthrowPrevItem", EActionTrigger.DOWN, PrevItem);
		m_InputManager.RemoveActionListener("MenuBack", EActionTrigger.DOWN, Cancel);
		
		m_InputManager.RemoveActionListener("CharacterRight", EActionTrigger.VALUE, MoveRight);
		m_InputManager.RemoveActionListener("CharacterForward", EActionTrigger.VALUE, MoveForward);
		m_InputManager.RemoveActionListener("Inventory_InspectZoom", EActionTrigger.VALUE, Zoom);
	}
	
	void Cancel(float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		if(!m_bBuilding) return;
		m_bBuilding = false;
		RemoveGhost();
		RevertCamera();
		if(m_BuildWidget)
			m_BuildWidget.RemoveFromHierarchy();
	}
	
	bool CanBuild(OVT_Buildable buildable, vector pos, out string reason)
	{
		reason = "#OVT-CannotBuildHere";
		
		float dist;
		
		if(buildable.m_bBuildAtBase)
		{			
			OVT_BaseData base = m_OccupyingFaction.GetNearestBase(pos);			
			dist = vector.Distance(base.location,pos);
			if(dist < m_Config.m_Difficulty.baseRange && !base.IsOccupyingFaction())
			{
				return true;
			}
		}
		
		if(buildable.m_bBuildInTown)
		{	
			OVT_TownData town = m_Towns.GetNearestTown(pos);
			if(town.size == 1) {
				reason = "#OVT-CannotBuildVillage";
				return false;
			}
			dist = vector.Distance(town.location,pos);
			int range = m_Towns.m_iCityRange;
			if(town.size < 3) range = m_Towns.m_iTownRange;
			if(dist < range)
			{
				return true;
			}
		}
		
		if(buildable.m_bBuildInVillage)
		{	
			OVT_TownData town = m_Towns.GetNearestTown(pos);
			if(town.size > 1) {
				reason = "#OVT-CannotBuildTown";
				return false;
			}
			dist = vector.Distance(town.location,pos);
			int range = m_Towns.m_iVillageRange;			
			if(dist < range)
			{
				return true;
			}
		}
		
		if(buildable.m_bBuildAtFOB)
		{	
			vector fob = m_Resistance.GetNearestFOB(pos);		
			dist = vector.Distance(fob, pos);
			if(dist < MAX_FOB_BUILD_DIS) return true;	
		}
							
		return false;
	}
	
	void StartBuild(OVT_Buildable buildable)
	{
		if(m_bIsActive) CloseLayout();
				
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		
		m_Buildable = buildable;
		
		string reason;
		if(!CanBuild(m_Buildable, player.GetOrigin(), reason))
		{
			ShowHint(reason);
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.ERROR);
			return;
		}
		
		if(!m_Economy.PlayerHasMoney(m_sPlayerID, m_Config.GetBuildableCost(buildable)))
		{
			ShowHint("#OVT-CannotAfford");
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.ERROR);
			return;
		}
		
		WorkspaceWidget workspace = GetGame().GetWorkspace(); 
		m_BuildWidget = workspace.CreateWidgets(m_BuildLayout);
		
		m_bBuilding = true;		
		m_iPrefabIndex = 0;
		
		SpawnGhost();
		CreateCamera();
		MoveCamera();
	}
	
	protected void CreateCamera()
	{
		if(m_Camera) return;
		
		CameraManager cameraMgr = GetGame().GetCameraManager();
		BaseWorld world = GetGame().GetWorld();
		
		m_PlayerCamera = cameraMgr.CurrentCamera();
		
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		
		EntitySpawnParams params = EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;		
		params.Transform[3] = player.GetOrigin() + "0 15 0";
						
		IEntity cam = GetGame().SpawnEntityPrefabLocal(Resource.Load(m_BuildCameraPrefab),null,params);
		if(cam)
		{
			m_Camera = SCR_CameraBase.Cast(cam);	
		}
	}
	
	protected void MoveCamera()
	{
		if(!m_Camera) return;
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		m_Camera.SetAngles("-45 0 0");
		m_Camera.SetOrigin(player.GetOrigin() + "0 15 0");
	}
	
	protected void RevertCamera()
	{
		CameraManager cameraMgr = GetGame().GetCameraManager();		
		cameraMgr.SetCamera(m_PlayerCamera);
		
		SCR_EntityHelper.DeleteEntityAndChildren(m_Camera);
		m_Camera = null;
	}
	
	void MoveRight(float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		if(!m_bBuilding) return;
		
		vector move = "0 0 0";
		move[0] = value * 0.07;
		vector pos = m_Camera.GetOrigin() + move;
		
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		vector groundPos = Vector(pos[0],GetGame().GetWorld().GetSurfaceY(pos[0],pos[2]),pos[2]);
		float dist = vector.Distance(player.GetOrigin(), groundPos);
		if(dist > 50) return;
		
		m_Camera.SetOrigin(pos);
	}	
	
	void MoveForward(float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		if(!m_bBuilding) return;
		vector move = "0 0 0";
		move[2] = value * 0.07;
		vector pos = m_Camera.GetOrigin() + move;
		
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		vector groundPos = Vector(pos[0],GetGame().GetWorld().GetSurfaceY(pos[0],pos[2]),pos[2]);
		float dist = vector.Distance(player.GetOrigin(), groundPos);
		if(dist > 50) return;		
		
		m_Camera.SetOrigin(pos);
	}	
	
	void Zoom(float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		if(!m_bBuilding) return;
		vector move = "0 0 0";
		move[1] = value * -0.005;
		
		vector lookpos = m_vCurrentTransform[3];
		
		vector pos = m_Camera.GetOrigin();
		pos = pos + move;
		
		float ground = GetGame().GetWorld().GetSurfaceY(pos[0],pos[2]);
		if(pos[1] < ground + 10)
		{
			pos[1] = ground + 10;
		}
		
		if(pos[1] > ground + 25)
		{
			pos[1] = ground + 25;
		}
		
		m_Camera.SetOrigin(pos);		
	}	
	
	protected void SpawnGhost()
	{		
		vector normal = vector.Zero;
		vector pos = GetBuildPosition(normal);
				
		EntitySpawnParams params = EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = pos;
		m_pBuildingPrefab = m_Buildable.m_aPrefabs[m_iPrefabIndex];
		m_eBuildingEntity = GetGame().SpawnEntityPrefabLocal(Resource.Load(m_pBuildingPrefab), null, params);
		
		if(m_vCurrentTransform)
		{
			m_eBuildingEntity.SetTransform(m_vCurrentTransform);
		}
		//SCR_Global.SetMaterial(m_eBuildingEntity, "{E0FECF0FE7457A54}Assets/Editor/PlacingPreview/Preview_03.emat", true);
		
		Physics phys = m_eBuildingEntity.GetPhysics();
		if(phys)
		{
			phys.SetActive(0);
		}
		
		OVT_MainMenuContextOverrideComponent over = EPF_Component<OVT_MainMenuContextOverrideComponent>.Find(m_eBuildingEntity);
		if(over)
		{
			//Disable map icon showing for ghost
			over.m_UiInfo = null;
		}
		
		m_eBuildingEntity.SetFlags(EntityFlags.VISIBLE, true);
	}
	
	protected void RemoveGhost()
	{
		SCR_EntityHelper.DeleteEntityAndChildren(m_eBuildingEntity);
		m_eBuildingEntity = null;
	}
	
	void DoBuild(float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{	
		if(!m_bBuilding) return;
			
		int cost = m_Config.GetBuildableCost(m_Buildable);
		vector mat[4];
		
		if(m_eBuildingEntity)
		{			
			m_eBuildingEntity.GetTransform(mat);
			RemoveGhost();
			string error;
			if(!CanBuild(m_Buildable, mat[3], error))
			{
				ShowHint(error);
				SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.ERROR);
				return;
			}
			
			if(!m_Economy.PlayerHasMoney(m_sPlayerID, cost))
			{
				ShowHint("#OVT-CannotAfford");
				SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.ERROR);
				return;
			}
			
			vector angles = Math3D.MatrixToAngles(mat);
			int buildableIndex = m_Resistance.m_BuildablesConfig.m_aBuildables.Find(m_Buildable);
			int prefabIndex = m_Buildable.m_aPrefabs.Find(m_pBuildingPrefab);
			OVT_Global.GetServer().BuildItem(buildableIndex, prefabIndex, mat[3], angles, m_iPlayerID);
						
			m_Economy.TakePlayerMoney(m_iPlayerID, m_Config.GetBuildableCost(m_Buildable));
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.CLICK);
		}
		
		Cancel();
	}
	
	void NextItem(float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		if(!m_bBuilding) return;
		int newIndex = m_iPrefabIndex + 1;
		if(newIndex > m_Buildable.m_aPrefabs.Count() - 1)
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
		if(!m_bBuilding) return;
		int newIndex = m_iPrefabIndex - 1;
		if(newIndex < 0)
		{
			newIndex = m_Buildable.m_aPrefabs.Count() - 1;
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
		if(m_eBuildingEntity)
		{
			vector angles = m_eBuildingEntity.GetYawPitchRoll();			
			angles[0] = angles[0] - 1;				
			m_eBuildingEntity.SetYawPitchRoll(angles);
		}
	}
	
	void RotateRight(float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		if(m_eBuildingEntity)
		{
			vector angles = m_eBuildingEntity.GetYawPitchRoll();			
			angles[0] = angles[0] + 1;			
			m_eBuildingEntity.SetYawPitchRoll(angles);
		}
	}
	
	vector GetBuildPosition(out vector normal)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		BaseWorld world = GetGame().GetWorld();
		
		float screenW, screenH;
		workspace.GetScreenSize(screenW, screenH);
		vector cameraDir;
		vector cameraPos = workspace.ProjScreenToWorldNative(screenW / 2, screenH / 2, cameraDir, world, -1);
		
		//--- Find object/ground intersection, or use maximum distance when none is found
		float traceDis = GetTraceDis(cameraPos, cameraDir * 500, normal);
		if (traceDis == 1)
			traceDis = 500;
		else
			traceDis *= 500;
		
		vector endPos = cameraPos + cameraDir * traceDis;
		
		return endPos;
	}
	
	protected float GetTraceDis(vector pos, vector dir, out vector hitNormal)
	{
		BaseWorld world = GetGame().GetWorld();
		autoptr TraceParam trace = new TraceParam();
		trace.Start = pos;
		trace.End = trace.Start + dir;
		trace.Flags = TraceFlags.WORLD;
		trace.Exclude = m_eBuildingEntity;
		
		float dis = world.TraceMove(trace, SCR_Global.FilterCallback_IgnoreCharacters);
		hitNormal = trace.TraceNorm;
		
		return dis;
	}
}