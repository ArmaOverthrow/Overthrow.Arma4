class OVT_GetShopLocationJobStage : OVT_JobStage
{
	override bool OnStart(OVT_Job job)
	{
		OVT_TownData town = job.GetTown();
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		int townID = OVT_Global.GetTowns().GetTownID(town);
		
		RplId shopId = economy.m_mTownShops[townID][0];
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(shopId));
		IEntity ent = rpl.GetEntity();
		job.location = ent.GetOrigin();
		
		return false;
	}
}