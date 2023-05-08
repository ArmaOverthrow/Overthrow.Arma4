class OVT_WaitTillSupportJobStage : OVT_JobStage
{
	[Attribute("25")]
	int m_iMinimumSupport;
	
	override bool OnTick(OVT_Job job)
	{	
		if(job.GetTown().SupportPercentage() >= m_iMinimumSupport) return false;		
		return true;
	}
}