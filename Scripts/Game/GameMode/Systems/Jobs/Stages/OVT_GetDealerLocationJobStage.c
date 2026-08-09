class OVT_GetDealerLocationJobStage : OVT_JobStage
{
	override bool OnStart(OVT_Job job)
	{
		OVT_TownData town = job.GetTown();
		if(!town) return false;
		if(town.gunDealerPosition != vector.Zero) job.location = town.gunDealerPosition;
		
		return false;
	}
}