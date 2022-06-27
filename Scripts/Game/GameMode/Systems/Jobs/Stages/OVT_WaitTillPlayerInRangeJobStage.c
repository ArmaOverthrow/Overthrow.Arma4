class OVT_WaitTillPlayerInRangeJobStage : OVT_JobStage
{
	[Attribute("220")]
	int m_iRange;
	
	override bool OnTick(OVT_Job job)
	{
		if(job.baseId > -1)
		{
			OVT_BaseControllerComponent base = job.GetBase();
			vector pos = base.GetOwner().GetOrigin();
			playerId = OVT_Global.NearestPlayer(pos);
			if(OVT_Global.PlayerInRange(pos, m_iRange)) return false;
		}else{
			OVT_TownData town = job.GetTown();
			playerId = OVT_Global.NearestPlayer(town.location);
			if(OVT_Global.PlayerInRange(town.location, m_iRange)) return false;
		}
		
		return true;
	}
}