class OVT_StealthSkillEffect : OVT_SkillEffect
{
	[Attribute("0", desc:"Distance reduction multiplier at night")]
	float m_fDistanceMul;
	
	override void OnPlayerData(OVT_PlayerData player)
	{
		player.stealthMultiplier = 1 - m_fDistanceMul;
	}
	
	override void SetDescriptionTo(TextWidget widget)
	{
		widget.SetTextFormat("#OVT-SkillEffect_Stealth",m_fDistanceMul * 100);
	}
}