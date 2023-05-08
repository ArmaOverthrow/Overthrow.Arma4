
//Simply waits till the jobs attached entity is dead
class OVT_WaitTillDeadJobStage : OVT_JobStage
{
	override bool OnTick(OVT_Job job)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(job.entity));
				
		if(rpl)
		{
			IEntity entity = rpl.GetEntity();
			DamageManagerComponent damageManager;
			ChimeraCharacter character = ChimeraCharacter.Cast(entity);
			if (character)
				damageManager = character.GetDamageManager();
			else
				damageManager = DamageManagerComponent.Cast(entity.FindComponent(DamageManagerComponent));
			
			if (!damageManager)
				return false;
			
			if(damageManager.GetState() == EDamageState.DESTROYED){				
				return false;
			}
		}else{
			return false;
		}
		
		return true;
	}
}