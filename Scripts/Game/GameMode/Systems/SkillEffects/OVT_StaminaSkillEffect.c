class OVT_StaminaSkillEffect : OVT_SkillEffect
{
	[Attribute("0", desc:"Stamina drain will be reduced by this")]
	float m_fStaminaIncrease;
	
	override void OnPlayerSpawn(ChimeraCharacter character)
	{
		SCR_CharacterStaminaComponent stamina = EPF_Component<SCR_CharacterStaminaComponent>.Find(character);
		if(!stamina) return;
		
		//We actually can't do anything here until BI exposes some stamina params
	}
	
	override void SetDescriptionTo(TextWidget widget)
	{
		widget.SetTextFormat("#OVT-SkillEffect_Stamina",m_fStaminaIncrease * 100);
	}
}