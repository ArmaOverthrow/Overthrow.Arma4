// credit to wierdjet
// https://www.twitch.tv/weirdjet
// https://reforger.armaplatform.com/workshop/59689E02BA18B635-UnflipMe

class OVT_FlipVehicleAction: ScriptedUserAction
{
	ActionsManagerComponent m_cActionsManagerComponent = null;
	Vehicle m_eVehicle = null;
	
	override event void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_cActionsManagerComponent = ActionsManagerComponent.Cast(pManagerComponent);
		m_eVehicle = Vehicle.Cast(pOwnerEntity);
	}
	
	// Gets called when the action is complete (after the 5sec wait)
	override event void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		BaseWorld world = GetGame().GetWorld();

		if (!world || !m_eVehicle)
			return;
		
		if (m_eVehicle.IsOccupied()) return;
		
		// Get the current transform (position) of the vehicle
		vector transform[4];
		pOwnerEntity.GetTransform(transform);
		
		//--- Reset pitch and roll, but preserve yaw
		vector angles = Math3D.MatrixToAngles(transform);
		Math3D.AnglesToMatrix(Vector(angles[0], 0, 0), transform);
		
		// Teleport the vehicle 0.5 units in the air
		transform[3][1] = transform[3][1]+0.5;
		
		// Apply new transform
		pOwnerEntity.SetTransform(transform);

		// Apply slight down force to make it kick in physics wise
		pOwnerEntity.GetPhysics().ApplyImpulse("0 -1 0");
	}
	
	override event bool CanBeShownScript(IEntity user)
	{
		if (!m_eVehicle) {
			return false;
		}
		
		OVT_OverthrowGameMode ot = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if(!ot) return false;
		
		OVT_PlayerOwnerComponent playerowner = EPF_Component<OVT_PlayerOwnerComponent>.Find(GetOwner());
		if(!playerowner) return false;
		if(!playerowner.IsLocked()) return !m_eVehicle.IsOccupied();
				
		string ownerUid = playerowner.GetPlayerOwnerUid();		
		string playerUid = OVT_Global.GetPlayers().GetPersistentIDFromControlledEntity(user);
		if(ownerUid != playerUid) return false;
		
		return !m_eVehicle.IsOccupied();
	}
}