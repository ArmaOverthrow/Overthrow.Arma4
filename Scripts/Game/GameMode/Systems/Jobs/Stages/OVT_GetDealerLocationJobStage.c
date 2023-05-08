class OVT_GetDealerLocationJobStage : OVT_JobStage
{
	override bool OnStart(OVT_Job job)
	{
		OVT_TownData town = job.GetTown();
		if(town.gunDealerPosition && town.gunDealerPosition[0] != 0) job.location = town.gunDealerPosition;
		
		return false;
	}
}