class OVT_MapInfoUI : SCR_MapUIBaseComponent
{	
	[Attribute()]
	protected ResourceName m_Layout;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Modifier Layout", params: "layout")]
	ResourceName m_ModLayout;
	
	[Attribute(uiwidget: UIWidgets.ColorPicker)]
	ref Color m_NegativeModifierColor;
	
	[Attribute(uiwidget: UIWidgets.ColorPicker)]
	ref Color m_PositiveModifierColor;
	
	protected Widget m_wInfoRoot;
	protected OVT_TownData m_SelectedTown;
	
	protected void ShowTownInfo()
	{
		if(!m_wInfoRoot) return;
		if(!m_SelectedTown) return;
		
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		
		OVT_TownManagerComponent townManager = OVT_Global.GetTowns();				
		int townID = townManager.GetTownID(m_SelectedTown);
		
		ImageWidget img = ImageWidget.Cast(m_wInfoRoot.FindAnyWidget("ControllingFaction"));
		img.LoadImageTexture(0, m_SelectedTown.ControllingFactionData().GetUIInfo().GetIconPath());
				
		TextWidget widget = TextWidget.Cast(m_wInfoRoot.FindAnyWidget("TownName"));
		widget.SetText(townManager.GetTownName(townID));
		
		widget = TextWidget.Cast(m_wInfoRoot.FindAnyWidget("Population"));
		widget.SetText(m_SelectedTown.population.ToString());
		
		widget = TextWidget.Cast(m_wInfoRoot.FindAnyWidget("Distance"));
		float distance = vector.Distance(m_SelectedTown.location, player.GetOrigin());
		string dis, units;
		SCR_Global.GetDistForHUD(distance, false, dis, units);
		widget.SetText(dis + " " + units);
		
		widget = TextWidget.Cast(m_wInfoRoot.FindAnyWidget("Stability"));
		widget.SetText(m_SelectedTown.stability.ToString() + "%");
		
		widget = TextWidget.Cast(m_wInfoRoot.FindAnyWidget("Support"));		
		widget.SetText(m_SelectedTown.support.ToString() + " (" + m_SelectedTown.SupportPercentage().ToString() + "%)");
		
		Widget container = m_wInfoRoot.FindAnyWidget("StabilityModContainer");
		Widget child = container.GetChildren();
		while(child)
		{
			container.RemoveChild(child);
			child = container.GetChildren();
		}
		autoptr array<int> done = new array<int>;
		OVT_TownModifierSystem system = townManager.GetModifierSystem(OVT_TownStabilityModifierSystem);
		WorkspaceWidget workspace = GetGame().GetWorkspace(); 
		foreach(OVT_TownModifierData data : m_SelectedTown.stabilityModifiers)
		{
			if(done.Contains(data.id)) continue;
			
			OVT_ModifierConfig mod = system.m_Config.m_aModifiers[data.id];
			
			Widget w = workspace.CreateWidgets(m_ModLayout, container);
			TextWidget tw = TextWidget.Cast(w.FindAnyWidget("Text"));
			
			int effect = mod.baseEffect;
			if(mod.flags & OVT_ModifierFlags.STACKABLE)
			{
				effect = 0;
				//count all present
				foreach(OVT_TownModifierData check : m_SelectedTown.stabilityModifiers)
				{
					if(check.id == data.id) effect += mod.baseEffect;
				}
			}
			
			tw.SetText(effect.ToString() + "% " + mod.title);
			
			PanelWidget panel = PanelWidget.Cast(w.FindAnyWidget("Background"));
			if(mod.baseEffect < 0)
			{
				panel.SetColor(m_NegativeModifierColor);
			}else{
				panel.SetColor(m_PositiveModifierColor);
			}
			done.Insert(data.id);
		}
		
		container = m_wInfoRoot.FindAnyWidget("SupportModContainer");
		child = container.GetChildren();
		while(child)
		{
			container.RemoveChild(child);
			child = container.GetChildren();
		}
		done.Clear();
		
		system = townManager.GetModifierSystem(OVT_TownSupportModifierSystem);
		foreach(OVT_TownModifierData data : m_SelectedTown.supportModifiers)
		{
			if(done.Contains(data.id)) continue;
			OVT_ModifierConfig mod = system.m_Config.m_aModifiers[data.id];
			Widget w = workspace.CreateWidgets(m_ModLayout, container);
			TextWidget tw = TextWidget.Cast(w.FindAnyWidget("Text"));
			int effect = mod.baseEffect;
			if(mod.flags & OVT_ModifierFlags.STACKABLE)
			{
				effect = 0;
				//count all present
				foreach(OVT_TownModifierData check : m_SelectedTown.supportModifiers)
				{
					if(check.id == data.id) effect += mod.baseEffect;
				}
			}
			
			tw.SetText(effect.ToString() + "% " + mod.title);
			
			PanelWidget panel = PanelWidget.Cast(w.FindAnyWidget("Background"));
			if(mod.baseEffect < 0)
			{
				panel.SetColor(m_NegativeModifierColor);
			}else{
				panel.SetColor(m_PositiveModifierColor);
			}
			done.Insert(data.id);
		}
	}
	
	override void OnMapOpen(MapConfiguration config)
	{		
		super.OnMapOpen(config);
		
		Widget w = GetGame().GetWorkspace().CreateWidgets(m_Layout, m_RootWidget);
		
		m_wInfoRoot = w;
		
		w.SetVisible(false);		
	}
	
	void SelectTown(OVT_TownData town)
	{
		m_wInfoRoot.SetVisible(true);
		m_SelectedTown = town;
		ShowTownInfo();
	}
}