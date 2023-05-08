class OVT_WaitTillPlayerInRangeJobStage : OVT_JobStage
{
	[Attribute("220")]
	int m_iRange;
	
	override bool OnTick(OVT_Job job)
	{
		if(job.owner == "") return true;
		
		OVT_PlayerData playerData = OVT_Global.GetPlayers().GetPlayer(job.owner);
		if(playerData.IsOffline()) return true;
		
		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerData.id);		
		if(!player) return true;
		
		if(vector.Distance(player.GetOrigin(), job.location) <= m_iRange) return false;
		
		return true;
	}
}