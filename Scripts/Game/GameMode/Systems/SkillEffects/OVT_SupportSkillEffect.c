class OVT_SupportSkillEffect : OVT_SkillEffect
{
	[Attribute("0", desc:"Chance at converting a supporter")]
	float m_fSupportChance;
	
	override void OnPlayerData(OVT_PlayerData player)
	{
		player.diplomacy = m_fSupportChance;
	}
	
	override void SetDescriptionTo(TextWidget widget)
	{
		widget.SetTextFormat("#OVT-SkillEffect_Support",m_fSupportChance * 100);
	}
}