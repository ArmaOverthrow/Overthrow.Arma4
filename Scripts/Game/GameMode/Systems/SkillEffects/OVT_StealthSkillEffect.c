class OVT_StealthSkillEffect : OVT_SkillEffect
{
	[Attribute("0", desc:"Distance reduction multiplier")]
	float m_fDistanceMul;
	
	[Attribute("0", desc:"Detection time bonus multiplier")]
	float m_fDetectionTimeMul;
	
	[Attribute("0", desc:"Disguise effectiveness bonus")]
	float m_fDisguiseBonus;
	
	override void OnPlayerData(OVT_PlayerData player)
	{
		// Enhanced stealth effects - much more impactful
		player.stealthMultiplier = 1 - m_fDistanceMul;
	}
	
	//! Gets the detection time bonus for this skill level
	float GetDetectionTimeBonus()
	{
		return m_fDetectionTimeMul;
	}
	
	//! Gets the disguise effectiveness bonus for this skill level  
	float GetDisguiseBonus()
	{
		return m_fDisguiseBonus;
	}
	
	override void SetDescriptionTo(TextWidget widget)
	{
		string bonusText = "";
		if(m_fDistanceMul > 0)
			bonusText += string.Format("Detection distance -%1%% ", m_fDistanceMul * 100);
		if(m_fDetectionTimeMul > 0)
			bonusText += string.Format("Detection time +%1%% ", m_fDetectionTimeMul * 100);
		if(m_fDisguiseBonus > 0)
			bonusText += string.Format("Disguise effectiveness +%1%% ", m_fDisguiseBonus * 100);
			
		widget.SetText(bonusText);
	}
}