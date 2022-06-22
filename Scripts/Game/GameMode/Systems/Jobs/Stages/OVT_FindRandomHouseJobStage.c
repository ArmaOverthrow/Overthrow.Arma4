class OVT_FindRandomHouseJobStage : OVT_JobStage
{
	override bool OnStart(OVT_Job job)
	{
		OVT_TownManagerComponent townMgr = OVT_Global.GetTowns();
		
		IEntity house = townMgr.GetRandomHouseInTown(townMgr.m_Towns[job.townId]);
		
		job.location = house.GetOrigin();
		
		return false;
	}
}