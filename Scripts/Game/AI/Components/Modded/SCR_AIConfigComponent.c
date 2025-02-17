
modded class SCR_AIConfigComponent : ScriptComponent
{
	override bool PerformDangerReaction(SCR_AIUtilityComponent utility, AIDangerEvent dangerEvent, int dangerEventCount)
	{
		EAIDangerEventType type = dangerEvent.GetDangerType();
		
		if(type == EAIDangerEventType.Danger_WeaponFire)
		{
			IEntity instigator = dangerEvent.GetObject();
			if(instigator)
			{
				OVT_PlayerWantedComponent wanted = OVT_PlayerWantedComponent.Cast(instigator.FindComponent(OVT_PlayerWantedComponent));
				
				if(wanted)
				{
					wanted.SetBaseWantedLevel(2);
				}
			}
		}
		
		return super.PerformDangerReaction(utility, dangerEvent, dangerEventCount);
	}
}