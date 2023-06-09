class OVT_TradeDiscountSkillEffect : OVT_SkillEffect
{
	[Attribute("0", desc:"Discount multiplier to drop prices by")]
	float m_fDiscount;
	
	override void OnPlayerData(OVT_PlayerData player)
	{
		player.priceMultiplier = 1 - m_fDiscount;
	}
	
	override void SetDescriptionTo(TextWidget widget)
	{
		widget.SetTextFormat("#OVT-SkillEffect_TradeDiscount",m_fDiscount * 100);
	}
}