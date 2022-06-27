class OVT_WaitTillSupportJobStage : OVT_JobStage
{
	[Attribute("25")]
	int m_iMinimumSupport;
	
	override bool OnTick(OVT_Job job)
	{
		OVT_TownData town = job.GetTown();
		playerId = OVT_Global.NearestPlayer(town.location);
		if(town.SupportPercentage() >= m_iMinimumSupport) return false;
		
		return true;
	}
}