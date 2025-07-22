class OVT_TimeoutJobStage : OVT_JobStage
{
	[Attribute("6", UIWidgets.EditBox, "Timeout in hours")] 
	int m_iTimeoutHours;

	protected int m_iStartHour = -1;
	protected int m_iLastNotifiedHour = -1;

	override bool OnStart(OVT_Job job)
	{
		ChimeraWorld world = GetGame().GetWorld();
		TimeAndWeatherManagerEntity t = world.GetTimeAndWeatherManager();
		TimeContainer time = t.GetTime();
		m_iStartHour = time.m_iHours;
		m_iLastNotifiedHour = m_iStartHour;
		return true;
	}

	override bool OnTick(OVT_Job job)
	{
		if(m_iStartHour < 0) return true; // Not initialized
		ChimeraWorld world = GetGame().GetWorld();
		TimeAndWeatherManagerEntity t = world.GetTimeAndWeatherManager();
		TimeContainer time = t.GetTime();
		int currentHour = time.m_iHours;
		int hoursPassed = currentHour - m_iStartHour;
		if(hoursPassed < 0) hoursPassed += 24; // handle midnight wrap
		int hoursLeft = m_iTimeoutHours - hoursPassed;
		if(currentHour != m_iLastNotifiedHour && hoursLeft > 0 && hoursLeft <= m_iTimeoutHours)
		{
			m_iLastNotifiedHour = currentHour;
			string msg = string.Format("You have about %1 hour(s) left to complete the mission!", hoursLeft);
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-Job_OfficerTimeoutWarning", msg);
		}
		if(hoursPassed >= m_iTimeoutHours)
		{
			// Timeout reached, fail the job stage
			OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
			if(occupying) occupying.m_iResources += 2000;
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-Job_Failed", "#OVT-Job_OfficerTimeout");
			return false;
		}
		return true;
	}
} 