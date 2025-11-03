class OVT_RespawnSystemComponentClass: SCR_RespawnSystemComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Respawn system component for Overthrow game mode.
//! This is the actual component that gets attached to the game mode entity.
//! It references OVT_SpawnLogic which contains the actual spawn logic implementation.
[ComponentEditorProps(icon: HYBRID_COMPONENT_ICON)]
class OVT_RespawnSystemComponent : SCR_RespawnSystemComponent
{
	//------------------------------------------------------------------------------------------------
	//! Get the Overthrow spawn logic (cast from base m_SpawnLogic)
	OVT_SpawnLogic GetOverthrowSpawnLogic()
	{
		return OVT_SpawnLogic.Cast(m_SpawnLogic);
	}

	//------------------------------------------------------------------------------------------------
	//! Get the event invoker for when player groups are created
	ScriptInvoker GetOnPlayerGroupCreated()
	{
		OVT_SpawnLogic spawnLogic = GetOverthrowSpawnLogic();
		if (spawnLogic)
			return spawnLogic.m_OnPlayerGroupCreated;
		return null;
	}
}
