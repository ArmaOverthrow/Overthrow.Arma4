class OVT_PlayerNotInRangeJobCondition : OVT_JobCondition
{
	[Attribute("1500")]
	float m_iRange;
			
	override bool ShouldStart(OVT_TownData town, OVT_BaseData base, int playerId)
	{
		if(playerId > -1)
		{			
			IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);		
			if(!player) return false;
			
			if(base)
			{
				return vector.Distance(player.GetOrigin(), base.location) > m_iRange;
			}
			if(town)
			{
				return vector.Distance(player.GetOrigin(), town.location) > m_iRange;
			}
			return false;			
		}else{
			if(base)
			{
				return !OVT_Global.PlayerInRange(base.location, m_iRange);
			}
			if(town)
			{
				return !OVT_Global.PlayerInRange(town.location, m_iRange);
			}
			return false;
		}
	}
}