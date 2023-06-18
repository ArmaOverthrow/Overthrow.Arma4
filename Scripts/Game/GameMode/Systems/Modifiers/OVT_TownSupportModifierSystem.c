class OVT_TownSupportModifierSystem : OVT_TownModifierSystem
{
	protected override void TryAddModifier(int townId, int index)
	{
		m_TownManager.TryAddSupportModifier(townId, index);
	}
	
	protected override void RemoveModifier(int townId, int index)
	{
		m_TownManager.RemoveSupportModifier(townId, index);
	}
	
	protected override void OnTimeout(int townId, int index)
	{
		m_TownManager.TimeoutSupportModifier(townId, index);
	}
	
	override int Recalculate(array<ref OVT_TownModifierData> modifiers, int baseValue = 100, int min = 0, int max = 100)
	{
		int newsupport = baseValue;
		float supportmods = 0;
		foreach(OVT_TownModifierData modifier : modifiers)
		{
			if(!modifier) continue;
			OVT_ModifierConfig mod = m_Config.m_aModifiers[modifier.id];
			supportmods += mod.baseEffect;
		}
		if(supportmods > 100) supportmods = 100;
		if(supportmods < -100) supportmods = -100;
		
		int supportPerc = Math.Round((newsupport / max) * 100);
		
		if(supportmods > 75)
		{
			//Add a supporter to the cause
			newsupport++;
		}else if(supportmods < -75)
		{
			//Remove a supporter from the cause
			newsupport--;
		}else if(supportmods > 0)
		{
			//Maybe add a supporter
			if(supportPerc < supportmods)
			{
				if(s_AIRandomGenerator.RandFloatXY(0, 100) < supportmods)
				{
					newsupport++;
				}
			}else{
				if(s_AIRandomGenerator.RandFloatXY(0, 100) < (supportmods * 0.25))
				{
					newsupport++;
				}
			}
			
		}else if(supportmods < 0)
		{
			//Maybe remove a supporter
			if(s_AIRandomGenerator.RandFloatXY(0, 100) < Math.AbsInt(supportmods))
			{
				newsupport--;
			}
		}
		
		if(newsupport < min) newsupport = min;
		if(newsupport > max) newsupport = max;
		
		return newsupport;
	}
}