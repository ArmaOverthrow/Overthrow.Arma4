class OVT_StaminaSkillEffect : OVT_SkillEffect
{
	[Attribute("0", desc:"Stamina drain will be reduced by this")]
	float m_fStaminaIncrease;
	
	override void SetupSkillLocal(ChimeraCharacter character)
	{
		
	}
	
	override void SetDescriptionTo(TextWidget widget)
	{
		widget.SetTextFormat("#OVT-SkillEffect_Stamina",m_fStaminaIncrease * 100);
	}
}