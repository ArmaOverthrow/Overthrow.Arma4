class OVT_GetShopLocationJobStage : OVT_JobStage
{
	override bool OnStart(OVT_Job job)
	{
		OVT_TownData town = job.GetTown();
		if(!town) return false;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		int townID = OVT_Global.GetTowns().GetTownID(town);

		//The shop registry can have changed since the offer-time condition validated it
		if(!economy.m_mTownShops.Contains(townID) || economy.m_mTownShops[townID].Count() == 0)
		{
			Print("[Overthrow] OVT_GetShopLocationJobStage: no shops registered in town " + townID.ToString(), LogLevel.WARNING);
			return false;
		}

		RplId shopId = economy.m_mTownShops[townID][0];
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(shopId));
		if(!rpl)
		{
			Print("[Overthrow] OVT_GetShopLocationJobStage: shop RplId could not be resolved in town " + townID.ToString(), LogLevel.WARNING);
			return false;
		}

		IEntity ent = rpl.GetEntity();
		if(!ent) return false;

		job.location = ent.GetOrigin();

		return false;
	}
}