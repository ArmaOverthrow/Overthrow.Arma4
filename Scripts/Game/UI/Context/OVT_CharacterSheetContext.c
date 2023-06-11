class OVT_CharacterSheetContext : OVT_UIContext
{		
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Skill Layout", params: "layout")]
	ResourceName m_SkillLayout;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Skill Level Layout", params: "layout")]
	ResourceName m_SkillLevelLayout;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Skill Effect Layout", params: "layout")]
	ResourceName m_SkillEffectLayout;
	
	protected ItemPreviewWidget m_wPlayerRender;
	protected ItemPreviewManagerEntity m_pPreviewManager;
	protected PreviewRenderAttributes m_PlayerRenderAttributes;
	protected SCR_CharacterInventoryStorageComponent m_StorageManager;
	protected OVT_SkillManagerComponent m_SkillManager;
	
	protected Widget m_SkillContainer;
	
	override void OnShow()
	{		
		m_StorageManager = SCR_CharacterInventoryStorageComponent.Cast(m_Player.FindComponent(SCR_CharacterInventoryStorageComponent));
		m_pPreviewManager = GetGame().GetItemPreviewManager();
		m_wPlayerRender = ItemPreviewWidget.Cast(m_wRoot.FindAnyWidget("playerRender"));
		auto collection = m_StorageManager.GetAttributes();
		if (collection)
			m_PlayerRenderAttributes = PreviewRenderAttributes.Cast(collection.FindAttribute(SCR_CharacterInventoryPreviewAttributes));
		
		m_SkillContainer = m_wRoot.FindAnyWidget("SkillContainer");
		m_SkillManager = OVT_Global.GetSkills();
		
		m_SkillManager.m_OnPlayerSkill.Insert(Refresh);
		
		Widget closeButton = m_wRoot.FindAnyWidget("CloseButton");
		SCR_NavigationButtonComponent btn = SCR_NavigationButtonComponent.Cast(closeButton.FindHandler(SCR_NavigationButtonComponent));		
		btn.m_OnClicked.Insert(CloseLayout);
				
		RefreshPlayerWidget();
		
		Refresh();		
	}
	
	protected void Refresh()
	{
		OVT_PlayerData player = OVT_PlayerData.Get(m_sPlayerID);
		
		TextWidget txt = TextWidget.Cast(m_wRoot.FindAnyWidget("TextPlayerName"));
		txt.SetText(player.name);
		
		int level = player.GetLevel();
		int levelToXP = player.GetNextLevelXP();
		int toSpend = (level - 1) - player.CountSkills();
		
		txt = TextWidget.Cast(m_wRoot.FindAnyWidget("TextLevel"));
		txt.SetTextFormat(level.ToString());
		
		txt = TextWidget.Cast(m_wRoot.FindAnyWidget("TextLevelFrom"));
		txt.SetText(level.ToString());
		
		txt = TextWidget.Cast(m_wRoot.FindAnyWidget("TextLevelTo"));
		txt.SetText((level+1).ToString());
		
		txt = TextWidget.Cast(m_wRoot.FindAnyWidget("TextXP"));
		txt.SetTextFormat("%1/%2 XP",player.xp.ToString(),levelToXP.ToString());
		
		ProgressBarWidget progress = ProgressBarWidget.Cast(m_wRoot.FindAnyWidget("LevelProgress"));
		progress.SetCurrent(player.GetLevelProgress() * 100);
		
		txt = TextWidget.Cast(m_wRoot.FindAnyWidget("TextToSpend"));
		if(toSpend == 1)
		{
			txt.SetTextFormat("#OVT-SkillPointToSpend",toSpend);	
		}else{
			txt.SetTextFormat("#OVT-SkillPointsToSpend",toSpend);	
		}
		
		Widget child = m_SkillContainer.GetChildren();
		while(child)
		{
			m_SkillContainer.RemoveChild(child);
			child = m_SkillContainer.GetChildren();
		}
		
		foreach(OVT_SkillConfig config : m_SkillManager.m_Skills.m_aSkills)
		{
			int playerLevel = 0;
			if(player.skills.Contains(config.m_sKey)) playerLevel = player.skills[config.m_sKey];
			RenderSkill(config,toSpend,playerLevel);
		}
	}
	
	protected void RenderSkill(OVT_SkillConfig config, int toSpend, int playerLevel)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace(); 
		Widget w = workspace.CreateWidgets(m_SkillLayout, m_SkillContainer);
		
		TextWidget txt = TextWidget.Cast(w.FindAnyWidget("SkillName"));
		txt.SetText(config.m_UIInfo.GetName());
		
		txt = TextWidget.Cast(w.FindAnyWidget("SkillDescription"));
		txt.SetText(config.m_UIInfo.GetDescription());
		
		ImageWidget icon = ImageWidget.Cast(w.FindAnyWidget("SkillIcon"));
		config.m_UIInfo.SetIconTo(icon);
		
		Widget levelContainer = w.FindAnyWidget("LevelContainer");
		Widget child = levelContainer.GetChildren();
		while(child)
		{
			levelContainer.RemoveChild(child);
			child = levelContainer.GetChildren();
		}
		
		ButtonWidget add = ButtonWidget.Cast(w.FindAnyWidget("AddButton"));
		add.SetName(config.m_sKey);
		if(toSpend > 0 && playerLevel < config.m_aLevels.Count())
		{
			add.SetVisible(true);
		}else{
			add.SetVisible(false);
		}
		SCR_ButtonComponent btn = SCR_ButtonComponent.Cast(add.FindHandler(SCR_ButtonComponent));
		if(btn)
		{
			btn.m_OnClicked.Insert(BuySkill);
		}
		
		int level = 1;
		foreach(OVT_SkillLevelConfig levelCfg : config.m_aLevels)
		{
			Widget ww = workspace.CreateWidgets(m_SkillLevelLayout, levelContainer);
			Widget effectContainer = ww.FindAnyWidget("EffectContainer");
			txt = TextWidget.Cast(ww.FindAnyWidget("TextSkillLevel"));
			txt.SetText(level.ToString());
			
			if(playerLevel >= level)
			{
				ImageWidget bg = ImageWidget.Cast(ww.FindAnyWidget("Background"));
				bg.SetColor(new Color(0.0, 0.32, 0.12, 1.0));
			}
			
			
			Widget effectChild = effectContainer.GetChildren();
			while(effectChild)
			{
				effectContainer.RemoveChild(effectChild);
				effectChild = effectContainer.GetChildren();
			}
			
			foreach(OVT_SkillEffect fx : levelCfg.m_aEffects)
			{
				TextWidget effect = TextWidget.Cast(workspace.CreateWidgets(m_SkillEffectLayout, effectContainer));
				
				fx.SetDescriptionTo(effect);
			}
			
			level++;
		}
	}
	
	protected void BuySkill(SCR_ButtonBaseComponent btn)
	{		
		string key = btn.GetRootWidget().GetName();
		OVT_PlayerData player = OVT_PlayerData.Get(m_sPlayerID);
		
		int level = player.GetLevel();
		int toSpend = (level - 1) - player.CountSkills();
		if(toSpend <= 0) return;
		
		OVT_Global.GetServer().BuySkill(m_iPlayerID,key);
	}
	
	protected override void OnClose()
	{
		m_SkillManager.m_OnPlayerSkill.Remove(Refresh);
	}
	
	
	protected void RefreshPlayerWidget()
	{
		if (!m_pPreviewManager)
			return;
		if (m_wPlayerRender)
			m_pPreviewManager.SetPreviewItem(m_wPlayerRender, m_Player, m_PlayerRenderAttributes);
		
	}
}