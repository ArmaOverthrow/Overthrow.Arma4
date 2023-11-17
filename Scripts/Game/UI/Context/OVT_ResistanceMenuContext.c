class OVT_ResistancePlayerData : Managed
{
	int playerId;
	
	void OVT_ResistancePlayerData(int id)
	{
		playerId = id;
	}
}

class OVT_ResistanceMenuContext : OVT_UIContext
{		
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Leaderboard player layout", params: "layout")]
	ResourceName m_PlayerLayout;
	
	protected OVT_TownManagerComponent m_Towns;
	
	protected SCR_SpinBoxComponent m_PlayerSpin;
	protected SCR_SliderComponent m_AmountSlider;
	
	override void PostInit()
	{
		if(SCR_Global.IsEditMode()) return;
		m_Towns = OVT_Global.GetTowns();
		m_Economy.m_OnPlayerMoneyChanged.Insert(OnPlayerMoneyChanged);
	}
	
	protected void OnPlayerMoneyChanged(string playerId, int amount)
	{
		if(playerId == m_sPlayerID && m_bIsActive)
		{
			TextWidget money = TextWidget.Cast(m_wRoot.FindAnyWidget("PlayerMoney"));		
			money.SetText("$" + amount);
		}
	}
	
	override void OnShow()
	{	
		bool isOfficer = OVT_Global.GetResistanceFaction().IsLocalPlayerOfficer();
		m_Economy.m_OnResistanceMoneyChanged.Insert(RefreshFunds);
		
		Widget closeButton = m_wRoot.FindAnyWidget("CloseButton");
		SCR_InputButtonComponent btn = SCR_InputButtonComponent.Cast(closeButton.FindHandler(SCR_InputButtonComponent));		
		btn.m_OnClicked.Insert(CloseLayout);
		
		Widget ww = m_wRoot.FindAnyWidget("MakeOfficer");
		if(!isOfficer) ww.SetVisible(false);
		ButtonActionComponent action = ButtonActionComponent.Cast(ww.FindHandler(ButtonActionComponent));		
		action.GetOnAction().Insert(MakeOfficer);
		
		ww = m_wRoot.FindAnyWidget("SendFunds");
		if(!isOfficer) ww.SetVisible(false);
		action = ButtonActionComponent.Cast(ww.FindHandler(ButtonActionComponent));		
		action.GetOnAction().Insert(SendFunds);
		
		ww = m_wRoot.FindAnyWidget("SendMoney");
		action = ButtonActionComponent.Cast(ww.FindHandler(ButtonActionComponent));		
		action.GetOnAction().Insert(SendMoney);
		
		ww = m_wRoot.FindAnyWidget("DonateFunds");
		action = ButtonActionComponent.Cast(ww.FindHandler(ButtonActionComponent));		
		action.GetOnAction().Insert(DonateFunds);
		
		Refresh();		
	}
	
	override void OnClose()
	{
		m_Economy.m_OnResistanceMoneyChanged.Remove(RefreshFunds);
	}
	
	protected void Refresh()
	{
		bool isOfficer = OVT_Global.GetResistanceFaction().IsLocalPlayerOfficer();
		
		TextWidget money = TextWidget.Cast(m_wRoot.FindAnyWidget("PlayerMoney"));		
		money.SetText("$" + m_Economy.GetPlayerMoney(m_sPlayerID));
		
		TextWidget w = TextWidget.Cast(m_wRoot.FindAnyWidget("TotalFunds"));
		w.SetText("$" + m_Economy.GetResistanceMoney().ToString());
		
		int donations = m_Economy.GetDonationIncome();
		int taxes = m_Economy.GetTaxIncome();
		int totalincome = donations + taxes;
		
		w = TextWidget.Cast(m_wRoot.FindAnyWidget("TotalIncome"));
		w.SetText("$" + totalincome.ToString());
		
		int population, supporters;
		m_Towns.GetTotalPopulationStats(population, supporters);
				
		w = TextWidget.Cast(m_wRoot.FindAnyWidget("TotalSupporters"));
		w.SetText(supporters.ToString() + "/" + population.ToString());
		
		w = TextWidget.Cast(m_wRoot.FindAnyWidget("SupporterIncome"));
		w.SetText("$" + donations.ToString());
		
		w = TextWidget.Cast(m_wRoot.FindAnyWidget("TaxIncome"));
		w.SetText("$" + taxes.ToString());
		
		ChimeraWorld world = GetGame().GetWorld();
		TimeAndWeatherManagerEntity t = world.GetTimeAndWeatherManager();
		int nextIncome = 0;
		TimeContainer time = t.GetTime();	
		
		if(time.m_iHours < 18) nextIncome = 18;
		if(time.m_iHours < 12) nextIncome = 12;
		if(time.m_iHours < 6) nextIncome = 6;	
		
		string next = nextIncome.ToString();
		if(next.Length() < 2) next = "0" + next;
		
		w = TextWidget.Cast(m_wRoot.FindAnyWidget("NextIncomeTime"));
		w.SetText(next + ":00");
		
		Widget ww = m_wRoot.FindAnyWidget("PlayerSpin");
		SCR_SpinBoxComponent spin = SCR_SpinBoxComponent.Cast(ww.FindHandler(SCR_SpinBoxComponent));
		m_PlayerSpin = spin;
		PlayerManager mgr = GetGame().GetPlayerManager();
		array<int> players = {};
		mgr.GetPlayers(players);
		OVT_PlayerManagerComponent playerMgr = OVT_Global.GetPlayers();
		foreach(int id : players)
		{
			spin.AddItem(mgr.GetPlayerName(id), new OVT_ResistancePlayerData(id));
		}
		
		ww = m_wRoot.FindAnyWidget("TaxSlider");
		SCR_SliderComponent slider = SCR_SliderComponent.Cast(ww.FindHandler(SCR_SliderComponent));
		slider.SetValue(m_Economy.m_fResistanceTax);
		slider.GetOnChangedFinal().Insert(SetTax);
		if(!isOfficer) slider.SetEnabled(false);
		
		ww = m_wRoot.FindAnyWidget("AmountSlider");
		slider = SCR_SliderComponent.Cast(ww.FindHandler(SCR_SliderComponent));
		m_AmountSlider = slider;
		slider.SetMax(100);
		if(m_Economy.GetResistanceMoney() > slider.GetMax())
		{
			slider.SetMax(m_Economy.GetResistanceMoney());
		}
		if(m_Economy.GetLocalPlayerMoney() > slider.GetMax())
		{
			slider.SetMax(m_Economy.GetLocalPlayerMoney());
		}
		if(slider.GetMax() < 1000)
		{
			slider.SetStep(10);
			slider.SetMin(10);
			slider.SetValue(10);
		}
		
		SCR_SortedArray<OVT_PlayerData> leaderboard();
		OVT_PlayerManagerComponent pm = OVT_Global.GetPlayers();
		for(int i=0; i < pm.m_mPlayers.Count(); i++)
		{			
			OVT_PlayerData player = pm.m_mPlayers.GetElement(i);
			if(player.name == "") continue;
			leaderboard.Insert(player.kills, player);
		}
		
		Widget container = m_wRoot.FindAnyWidget("LeaderboardContainer");
		while(container.GetChildren())
			container.RemoveChild(container.GetChildren());
		
		array<OVT_PlayerData> playersSorted();
		leaderboard.ToArray(playersSorted);
		WorkspaceWidget workspace = GetGame().GetWorkspace(); 
		for(int i = playersSorted.Count()-1; i>=0; i--)
		{
			OVT_PlayerData player = playersSorted[i];
			Widget pw = workspace.CreateWidgets(m_PlayerLayout, container);
			TextWidget txt = TextWidget.Cast(pw.FindAnyWidget("TextLevel"));
			txt.SetText(player.GetLevel().ToString());
			
			txt = TextWidget.Cast(pw.FindAnyWidget("NameText"));
			txt.SetText(player.name);
			
			txt = TextWidget.Cast(pw.FindAnyWidget("KillsText"));
			txt.SetText(player.kills.ToString());
		}
	}
	
	protected void RefreshFunds()
	{
		TextWidget w = TextWidget.Cast(m_wRoot.FindAnyWidget("TotalFunds"));
		w.SetText("$" + m_Economy.GetResistanceMoney().ToString());
	}
	
	protected void SetTax(SCR_SliderComponent slider)
	{
		if(!OVT_Global.GetResistanceFaction().IsLocalPlayerOfficer()) return;
		
		m_Economy.SetResistanceTax(slider.GetValue());
	}
	
	protected void MakeOfficer(SCR_ButtonTextComponent btn)
	{
		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if(!resistance.IsLocalPlayerOfficer()) return;
		
		OVT_ResistancePlayerData data = OVT_ResistancePlayerData.Cast(m_PlayerSpin.GetCurrentItemData());
		
		if(resistance.IsOfficer(data.playerId)) return;
		
		resistance.AddOfficer(data.playerId);
	}
	
	protected void DonateFunds(SCR_ButtonTextComponent btn)
	{	
		int localId = SCR_PlayerController.GetLocalPlayerId();		
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(localId);	
				
		int amount = m_AmountSlider.GetValue();
		int money = m_Economy.GetPlayerMoney(persId);
		if(amount > money){
			amount = money;
		}
		if(amount <= 0) return;		
				
		m_Economy.TakePlayerMoney(localId, amount);
		m_Economy.AddResistanceMoney(amount);
		OVT_Global.GetServer().SendNotification("PlayerDonated",-1,OVT_Global.GetPlayers().GetPlayerName(m_iPlayerID),amount.ToString());
	}
	
	protected void SendFunds(SCR_ButtonTextComponent btn)
	{
		if(!OVT_Global.GetResistanceFaction().IsLocalPlayerOfficer()) return;
		
		int amount = m_AmountSlider.GetValue();
		if(amount > m_Economy.GetResistanceMoney()){
			amount = m_Economy.GetResistanceMoney();
		}
		if(amount <= 0) return;
		
		OVT_ResistancePlayerData data = OVT_ResistancePlayerData.Cast(m_PlayerSpin.GetCurrentItemData());
		m_Economy.AddPlayerMoney(data.playerId, amount);
		m_Economy.TakeResistanceMoney(amount);
		OVT_Global.GetServer().SendNotification("PlayerSentFunds",data.playerId,amount.ToString());
	}
	
	protected void SendMoney(SCR_ButtonTextComponent btn)
	{	
		int localId = SCR_PlayerController.GetLocalPlayerId();
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(localId);	
				
		int amount = m_AmountSlider.GetValue();
		int money = m_Economy.GetPlayerMoney(persId);
		if(amount > money){
			amount = money;
		}
		if(amount <= 0) return;
		
		OVT_ResistancePlayerData data = OVT_ResistancePlayerData.Cast(m_PlayerSpin.GetCurrentItemData());
		
		if(data.playerId == SCR_PlayerController.GetLocalPlayerId()) return;
				
		m_Economy.AddPlayerMoney(data.playerId, amount);
		m_Economy.TakePlayerMoney(localId, amount);
		OVT_Global.GetServer().SendNotification("PlayerSentMoney",data.playerId,OVT_Global.GetPlayers().GetPlayerName(m_iPlayerID),amount.ToString());
	}
}