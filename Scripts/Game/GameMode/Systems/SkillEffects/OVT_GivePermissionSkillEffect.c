class OVT_GivePermissionSkillEffect : OVT_SkillEffect
{
	[Attribute("", desc:"Permission to give player")]
	string m_sPermission;
	
	[Attribute("", desc:"Description to use in character sheet")]
	string m_sDescription;
	
	override void OnPlayerData(OVT_PlayerData player)
	{
		player.GivePermission(m_sPermission);
	}
	
	override void SetDescriptionTo(TextWidget widget)
	{
		widget.SetText(m_sDescription);
	}
}