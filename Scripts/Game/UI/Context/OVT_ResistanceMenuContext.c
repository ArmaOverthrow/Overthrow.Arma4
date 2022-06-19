class OVT_ResistanceMenuContext : OVT_UIContext
{		
	protected OVT_TownManagerComponent m_Towns;
	override void PostInit()
	{
		if(SCR_Global.IsEditMode()) return;
		m_Towns = OVT_Global.GetTowns();
	}
	
	override void OnShow()
	{	
				
		Refresh();		
	}
	
	protected void Refresh()
	{
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
		
		TimeAndWeatherManagerEntity t = GetGame().GetTimeAndWeatherManager();
		int nextIncome = 0;
		TimeContainer time = t.GetTime();	
		
		if(time.m_iHours < 18) nextIncome = 18;
		if(time.m_iHours < 12) nextIncome = 12;
		if(time.m_iHours < 6) nextIncome = 6;	
		
		string next = nextIncome.ToString();
		if(next.Length() < 2) next = "0" + next;
		
		w = TextWidget.Cast(m_wRoot.FindAnyWidget("NextIncomeTime"));
		w.SetText(next + ":00");
	}
	
}