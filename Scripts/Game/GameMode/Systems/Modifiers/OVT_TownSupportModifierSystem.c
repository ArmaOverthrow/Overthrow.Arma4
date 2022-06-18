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
	
	override int Recalculate(array<ref int> modifiers, int baseValue = 100, int min = 0, int max = 100)
	{
		int newsupport = baseValue;
		float supportmods = 0;
		foreach(int index : modifiers)
		{
			OVT_ModifierConfig mod = m_Config.m_aModifiers[index];
			supportmods += mod.baseEffect;
		}
		if(supportmods > 100) supportmods = 100;
		if(supportmods < -100) supportmods = -100;
		
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
			if(s_AIRandomGenerator.RandFloatXY(0, 100) < supportmods)
			{
				newsupport++;
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