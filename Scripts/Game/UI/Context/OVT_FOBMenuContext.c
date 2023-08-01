class OVT_FOBMenuContext : OVT_UIContext
{	
	OVT_CampData m_FOB;
	ref SCR_SpinBoxComponent m_GroupSpin;
	ref SCR_ButtonTextComponent m_GarrisonButton;
	
	override void OnShow()
	{		
		Widget w = m_wRoot.FindAnyWidget("GarrisonSpin");
		m_GroupSpin = SCR_SpinBoxComponent.Cast(w.FindHandler(SCR_SpinBoxComponent));
				
		m_GroupSpin.GetOnLeftArrowClick().Insert(UpdateInfo);
		m_GroupSpin.GetOnRightArrowClick().Insert(UpdateInfo);
		
		m_GarrisonButton = SCR_ButtonTextComponent.Cast(m_wRoot.FindAnyWidget("AddToGarrison").FindHandler(SCR_ButtonTextComponent));
		m_GarrisonButton.m_OnClicked.Insert(AddToGarrison);
		
		Widget closeButton = m_wRoot.FindAnyWidget("CloseButton");
		SCR_NavigationButtonComponent btn = SCR_NavigationButtonComponent.Cast(closeButton.FindHandler(SCR_NavigationButtonComponent));		
		btn.m_OnClicked.Insert(CloseLayout);
		
		Refresh();		
	}
	
	override void OnClose()
	{
		m_GroupSpin.GetOnLeftArrowClick().Remove(UpdateInfo);
		m_GroupSpin.GetOnRightArrowClick().Remove(UpdateInfo);
		m_GarrisonButton.m_OnClicked.Remove(AddToGarrison);
	}
	
	protected void Refresh()
	{
		
		
		
		OVT_Faction faction = OVT_Global.GetConfig().GetPlayerFaction();
		
		foreach(int i, ResourceName res : faction.m_aGroupPrefabSlots)
		{
			IEntity spawn = OVT_Global.SpawnEntityPrefab(res, "0 0 0", "0 0 0", false);
			
			SCR_AIGroup aigroup = SCR_AIGroup.Cast(spawn);
			int soldierCost = m_Config.m_Difficulty.baseRecruitCost * aigroup.m_aUnitPrefabSlots.Count();
			
			//To-do: properly calculate equipment cost (factoring in warehouse)
			int equipmentCost = 300 * aigroup.m_aUnitPrefabSlots.Count();
			
			SCR_EditableGroupComponent group = SCR_EditableGroupComponent.Cast(spawn.FindComponent(SCR_EditableGroupComponent));
			if(group)
			{
				m_GroupSpin.AddItem(group.GetDisplayName(), new OVT_GroupUIInfo(res, soldierCost, equipmentCost, aigroup.m_aUnitPrefabSlots.Count()));
			}
			
			
			
			SCR_EntityHelper.DeleteEntityAndChildren(spawn);			
		}
		
		
		UpdateInfo();
	}
	
	protected void AddToGarrison()
	{
		OVT_GroupUIInfo uiinfo = OVT_GroupUIInfo.Cast(m_GroupSpin.GetCurrentItemData());
		
		int cost = uiinfo.soldierCost + uiinfo.equipmentCost;
		if(!m_Economy.LocalPlayerHasMoney(cost))
		{	
			CloseLayout();	
			ShowHint("#OVT-CannotAfford");			
			return;
		}
		
		OVT_TownData town = OVT_Global.GetTowns().GetNearestTown(m_Owner.GetOrigin());
		if(town.support < uiinfo.numSoldiers || town.population < uiinfo.numSoldiers)
		{			
			CloseLayout();	
			SCR_HintManagerComponent.ShowCustomHint("#OVT-NoSupportersToRecruit");
			return;
		}
		
		m_Economy.TakeLocalPlayerMoney(cost);
		OVT_Global.GetServer().AddGarrisonFOB(m_FOB, uiinfo.resource);
	}
	
	protected void UpdateInfo()
	{
		OVT_GroupUIInfo uiinfo = OVT_GroupUIInfo.Cast(m_GroupSpin.GetCurrentItemData());
		
		TextWidget w = TextWidget.Cast(m_wRoot.FindAnyWidget("SoldierCost"));
		w.SetText(uiinfo.soldierCost.ToString());
		
		w = TextWidget.Cast(m_wRoot.FindAnyWidget("EquipmentCost"));
		w.SetText(uiinfo.equipmentCost.ToString());
		
		w = TextWidget.Cast(m_wRoot.FindAnyWidget("TotalCost"));
		w.SetText((uiinfo.soldierCost + uiinfo.equipmentCost).ToString());
	}
}