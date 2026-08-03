class OVT_FindRandomHouseJobStage : OVT_JobStage
{
	override bool OnStart(OVT_Job job)
	{
		OVT_TownManagerComponent townMgr = OVT_Global.GetTowns();
		if(job.townId < 0 || job.townId >= townMgr.m_Towns.Count()) return false;

		IEntity house = townMgr.GetRandomUnownedHouseInTown(townMgr.m_Towns[job.townId]);
		if(!house)
		{
			Print("[Overthrow] OVT_FindRandomHouseJobStage: no unowned house found in town " + job.townId.ToString(), LogLevel.WARNING);
			return false;
		}

		job.location = house.GetOrigin();

		return false;
	}
}