class OVT_HasRecruitJobStage : OVT_JobStage
{
	override bool OnTick(OVT_Job job)
	{
		if (job.owner.IsEmpty())
			return true;
		
		// Get recruit manager
		OVT_RecruitManagerComponent recruitManager = OVT_RecruitManagerComponent.GetInstance();
		if (!recruitManager)
			return true;
		
		// Get player data
		OVT_PlayerData playerData = OVT_Global.GetPlayers().GetPlayer(job.owner);
		if (!playerData)
			return true;
		
		// Check if player has at least one recruit
		int recruitCount = recruitManager.GetRecruitCount(job.owner);
		if (recruitCount > 0)
		{
			// Player has recruits, job stage complete
			return false;
		}
		
		// Continue waiting for player to recruit someone
		return true;
	}
}