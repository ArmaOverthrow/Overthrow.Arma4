//------------------------------------------------------------------------------------------------
//! Callback handler for FOB undeployment operations
//! Handles cleanup and notifications when FOB undeployment with container collection completes
class OVT_FOBUndeploymentCallback : OVT_StorageProgressCallback
{
	protected IEntity m_pDeployedFOB;
	protected IEntity m_pMobileFOB;
	
	//------------------------------------------------------------------------------------------------
	void OVT_FOBUndeploymentCallback(IEntity deployedFOB, IEntity mobileFOB)
	{
		m_pDeployedFOB = deployedFOB;
		m_pMobileFOB = mobileFOB;
		
		// Deactivate physics on the mobile FOB to prevent it from flying away during transfer
		if (m_pMobileFOB)
		{
			Physics physics = m_pMobileFOB.GetPhysics();
			if (physics)
			{
				physics.SetActive(ActiveState.INACTIVE);
				Print("Deactivated physics on mobile FOB during transfer");
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnProgressUpdate(float progress, int currentItem, int totalItems, string operation)
	{
		// Send progress update to clients via RPC if needed
		// For now, just log progress server-side
		Print(string.Format("FOB Undeployment Progress: %1% - %2", 
			Math.Round(progress * 100), operation), LogLevel.VERBOSE);
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnComplete(int itemsTransferred, int itemsSkipped)
	{
		Print(string.Format("OVT_FOBUndeploymentCallback.OnComplete() START: %1 items transferred, %2 items skipped", 
			itemsTransferred, itemsSkipped));
		Print("FOB undeployment callback - starting cleanup and deletion sequence");
		
		// Clean up all placed/built items in FOB area AFTER container collection
		if (m_pDeployedFOB)
		{
			Print("Deployed FOB entity exists, getting position for cleanup");
			vector fobPosition = m_pDeployedFOB.GetOrigin();
			Print(string.Format("FOB position: %1", fobPosition.ToString()));
			
			OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
			if (resistance)
			{
				Print("Starting comprehensive FOB area cleanup after container collection...");
				resistance.CleanupFOBArea(fobPosition, 75.0);
				Print("FOB area cleanup completed");
			}
			else
			{
				Print("ERROR: Resistance faction manager is null!");
			}
		}
		else
		{
			Print("WARNING: Deployed FOB entity is null!");
		}
		
		// Delete the deployed FOB entity after cleanup
		if (m_pDeployedFOB)
		{
			Print("Deleting deployed FOB entity...");
			SCR_EntityHelper.DeleteEntityAndChildren(m_pDeployedFOB);
			Print("Deployed FOB entity deleted successfully");
			m_pDeployedFOB = null;
		}
		else
		{
			Print("No deployed FOB to delete (already null)");
		}
		
		// Now reactivate physics on the mobile FOB after old one is gone
		if (m_pMobileFOB)
		{
			Print("Reactivating physics on mobile FOB...");
			Physics physics = m_pMobileFOB.GetPhysics();
			if (physics)
			{
				physics.SetActive(ActiveState.ACTIVE);
				Print("Reactivated physics on mobile FOB after old FOB deletion");
			}
			else
			{
				Print("WARNING: Mobile FOB has no physics component!");
			}
		}
		else
		{
			Print("WARNING: Mobile FOB is null!");
		}
		
		// Send completion notification to players
		if (m_pMobileFOB)
		{
			string ownerPersistentId = "";
			OVT_VehicleManagerComponent vm = OVT_Global.GetVehicles();
			if (vm)
				ownerPersistentId = vm.GetOwnerID(m_pMobileFOB);
			
			if (!ownerPersistentId.IsEmpty())
			{
				int playerId = OVT_Global.GetPlayers().GetPlayerIDFromPersistentID(ownerPersistentId);
				if (playerId > 0)
				{
					// Send notification to the FOB owner
					OVT_Global.GetNotify().SendTextNotification("#OVT-FOBUndeployed", playerId, 
						itemsTransferred.ToString(), GetContainerCount().ToString());
				}
			}
		}
		
		// Clean up references
		Print("Cleaning up FOB callback references...");
		m_pDeployedFOB = null;
		m_pMobileFOB = null;
		Print("OVT_FOBUndeploymentCallback.OnComplete() FINISHED");
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnError(string errorMessage)
	{
		Print(string.Format("FOB undeployment failed: %1", errorMessage), LogLevel.ERROR);
		
		// Send error notification to players
		if (m_pMobileFOB)
		{
			string ownerPersistentId = "";
			OVT_VehicleManagerComponent vm = OVT_Global.GetVehicles();
			if (vm)
				ownerPersistentId = vm.GetOwnerID(m_pMobileFOB);
			
			if (!ownerPersistentId.IsEmpty())
			{
				int playerId = OVT_Global.GetPlayers().GetPlayerIDFromPersistentID(ownerPersistentId);
				if (playerId > 0)
				{
					OVT_Global.GetNotify().SendTextNotification("#OVT-FOBUndeployFailed", playerId, 
						errorMessage);
				}
			}
		}
		
		// Fallback: still delete deployed FOB to prevent it being stuck
		if (m_pDeployedFOB)
		{
			Print("Performing fallback deletion of deployed FOB due to error");
			SCR_EntityHelper.DeleteEntityAndChildren(m_pDeployedFOB);
		}
		
		// Clean up references
		m_pDeployedFOB = null;
		m_pMobileFOB = null;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gets estimated container count for notification (simplified)
	protected int GetContainerCount()
	{
		// This is a simplified implementation - could be enhanced to track actual container count
		return 3; // Placeholder value
	}
}