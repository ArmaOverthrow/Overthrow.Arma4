class OVT_TimeoutOrDeathJobStage : OVT_JobStage
{
	[Attribute("360", UIWidgets.EditBox, "Timeout in minutes")] 
	int m_iTimeoutMinutes;

	protected int m_iStartMinute = -1;
	protected int m_iLastNotifiedMinute = -1;

	override bool OnStart(OVT_Job job)
	{
		ChimeraWorld world = GetGame().GetWorld();
		TimeAndWeatherManagerEntity t = world.GetTimeAndWeatherManager();
		TimeContainer time = t.GetTime();
		m_iStartMinute = time.m_iHours * 60 + time.m_iMinutes;
		m_iLastNotifiedMinute = m_iStartMinute;
		return true;
	}

	override bool OnTick(OVT_Job job)
	{
		// Проверка смерти цели
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(job.entity));
		if (!rpl)
		{
			Print("[TimeoutOrDeathJobStage] RplComponent is null for job.entity=" + job.entity);
			return false; // entity уже не существует, стадия завершается
		}

		IEntity entity = rpl.GetEntity();
		if (!entity)
		{
			Print("[TimeoutOrDeathJobStage] Entity is null for job.entity=" + job.entity);
			return false; // entity уже не существует, стадия завершается
		}

		DamageManagerComponent damageManager = null;
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (character)
			damageManager = character.GetDamageManager();
		else
			damageManager = DamageManagerComponent.Cast(entity.FindComponent(DamageManagerComponent));

		if (!damageManager)
		{
			Print("[TimeoutOrDeathJobStage] DamageManagerComponent is null for entity=" + entity);
			return false;
		}

		if (damageManager.IsDestroyed())
		{
			Print("[TimeoutOrDeathJobStage] Target destroyed for job.entity=" + job.entity);
			return false; // цель убита
		}

		// Проверка таймаута по минутам
		if(m_iStartMinute < 0) return true; // Not initialized
		ChimeraWorld world = GetGame().GetWorld();
		TimeAndWeatherManagerEntity t = world.GetTimeAndWeatherManager();
		TimeContainer time = t.GetTime();
		int currentMinute = time.m_iHours * 60 + time.m_iMinutes;
		int minutesPassed = currentMinute - m_iStartMinute;
		if(minutesPassed < 0) minutesPassed += 24 * 60; // handle midnight wrap
		int minutesLeft = m_iTimeoutMinutes - minutesPassed;
		if(currentMinute - m_iLastNotifiedMinute >= 10 && minutesLeft > 0 && minutesLeft <= m_iTimeoutMinutes)
		{
			m_iLastNotifiedMinute = currentMinute;
			int hoursLeft = minutesLeft / 60;
			int minsLeft = minutesLeft % 60;
			string msg = string.Format("You have about %1 hour(s) %2 minute(s) left to complete the mission!", hoursLeft, minsLeft);
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-Job_OfficerTimeoutWarning", msg);
		}
		if(minutesPassed >= m_iTimeoutMinutes)
		{
			Print("[TimeoutOrDeathJobStage] Timeout reached for job.entity=" + job.entity);
			// Timeout reached, fail the job stage
			OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
			if(occupying) occupying.m_iResources += 2000;
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-Job_Failed", "#OVT-Job_OfficerTimeout");
			job.failed = true;
			return false;
		}
		return true;
	}
} 