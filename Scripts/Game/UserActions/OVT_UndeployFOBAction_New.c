//------------------------------------------------------------------------------------------------
//! Example migration to new controller architecture
//! This shows how to update from OVT_PlayerCommsComponent to OVT_ContainerTransferComponent
class OVT_UndeployFOBAction_New : ScriptedUserAction
{	
	
	//---------------------------------------------------------
 	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity) 
 	{
		// OLD WAY:
		// OVT_Global.GetServer().UndeployFOB(pOwnerEntity.GetParent());
		
		// NEW WAY:
		OVT_ContainerTransferComponent transfer = OVT_Global.GetContainerTransfer();
		if (!transfer)
		{
			Print("[Overthrow] Container transfer component not available", LogLevel.WARNING);
			return;
		}
		
		// Check if component is busy
		if (!transfer.IsAvailable())
		{
			// Could show a message to user that another operation is in progress
			return;
		}
		
		// Get the deployed FOB entity (parent of the action entity)
		IEntity deployedFOB = pOwnerEntity.GetParent();
		
		// Find the mobile FOB vehicle nearby (you'd need to implement this search)
		// For now, using a placeholder - in real implementation you'd search for the vehicle
		IEntity mobileFOB = FindNearbyMobileFOB(deployedFOB);
		if (!mobileFOB)
		{
			Print("[Overthrow] No mobile FOB vehicle found nearby", LogLevel.WARNING);
			return;
		}
		
		// Subscribe to progress updates if you want to show UI
		transfer.m_OnProgressUpdate.Insert(OnTransferProgress);
		transfer.m_OnOperationComplete.Insert(OnTransferComplete);
		transfer.m_OnOperationError.Insert(OnTransferError);
		
		// Start the undeployment with container collection
		transfer.UndeployFOBWithCollection(deployedFOB, mobileFOB);
 	}
	
	//------------------------------------------------------------------------------------------------
	protected void OnTransferProgress(float progress, int current, int total, string operation)
	{
		// Update UI with progress
		Print(string.Format("[Overthrow] FOB Undeploy Progress: %.1f%% (%d/%d) - %s", 
			progress * 100, current, total, operation), LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	protected void OnTransferComplete(int itemsTransferred, int itemsSkipped)
	{
		// Clean up event handlers
		OVT_ContainerTransferComponent transfer = OVT_Global.GetContainerTransfer();
		if (transfer)
		{
			transfer.m_OnProgressUpdate.Remove(OnTransferProgress);
			transfer.m_OnOperationComplete.Remove(OnTransferComplete);
			transfer.m_OnOperationError.Remove(OnTransferError);
		}
		
		Print(string.Format("[Overthrow] FOB Undeploy Complete: %d items transferred, %d skipped", 
			itemsTransferred, itemsSkipped), LogLevel.NORMAL);
		
		// Now trigger the actual FOB removal
		// This would call the resistance faction manager to delete the FOB
		// OVT_Global.GetResistanceFaction().CompleteFOBUndeploy(...);
	}
	
	//------------------------------------------------------------------------------------------------
	protected void OnTransferError(string errorMessage)
	{
		// Clean up event handlers
		OVT_ContainerTransferComponent transfer = OVT_Global.GetContainerTransfer();
		if (transfer)
		{
			transfer.m_OnProgressUpdate.Remove(OnTransferProgress);
			transfer.m_OnOperationComplete.Remove(OnTransferComplete);
			transfer.m_OnOperationError.Remove(OnTransferError);
		}
		
		Print("[Overthrow] FOB Undeploy Error: " + errorMessage, LogLevel.ERROR);
	}
	
	//------------------------------------------------------------------------------------------------
	protected IEntity FindNearbyMobileFOB(IEntity deployedFOB)
	{
		// This is a placeholder - in real implementation you would:
		// 1. Search for vehicles within a certain radius
		// 2. Check if they have the mobile FOB component
		// 3. Return the closest one
		
		// For now, just return null
		return null;
	}
		
	override bool GetActionNameScript(out string outName)
	{
		return false;
	}	
			
	override bool CanBeShownScript(IEntity user) 
	{
		// Check server config to see if FOB undeployment is restricted to officers only
		if(OVT_Global.GetConfig().m_ConfigFile.mobileFOBOfficersOnly)
		{
			if(!OVT_Global.GetPlayers().LocalPlayerIsOfficer()) return false;
		}
		
		// Also check if container transfer component is available
		OVT_ContainerTransferComponent transfer = OVT_Global.GetContainerTransfer();
		if (!transfer || !transfer.IsAvailable()) return false;
		
		return true;
	}
	
	override bool HasLocalEffectOnlyScript() { return true; }
}