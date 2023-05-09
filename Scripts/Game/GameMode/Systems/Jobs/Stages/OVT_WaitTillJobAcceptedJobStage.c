class OVT_WaitTillJobAcceptedJobStage : OVT_JobStage
{
	override bool OnTick(OVT_Job job)
	{	
		return !job.accepted;
	}
}