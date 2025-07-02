class OVT_BaseServerProgressComponentClass : OVT_ComponentClass {};

//------------------------------------------------------------------------------------------------
//! Base component for server operations that require progress tracking.
//! Provides infrastructure for reporting operation progress from server to owning client.
//! Extend this class for any long-running server operation that needs client feedback.
class OVT_BaseServerProgressComponent : OVT_Component
{
	// Progress tracking
	protected float m_fProgress = 0.0;
	protected int m_iItemsProcessed = 0;
	protected int m_iTotalItems = 0;
	protected string m_sCurrentOperation = "";
	protected bool m_bIsRunning = false;
	
	// Client-side progress callbacks
	ref ScriptInvoker m_OnProgressUpdate = new ScriptInvoker();
	ref ScriptInvoker m_OnOperationComplete = new ScriptInvoker();
	ref ScriptInvoker m_OnOperationError = new ScriptInvoker();
	
	//------------------------------------------------------------------------------------------------
	//! Updates progress information and notifies client.
	//! Called from server-side operations to report progress.
	//! \param[in] progress Progress value from 0.0 to 1.0
	//! \param[in] current Current item being processed
	//! \param[in] total Total items to process
	//! \param[in] operation Description of current operation
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	void RpcDo_UpdateProgress(float progress, int current, int total, string operation)
	{
		m_fProgress = progress;
		m_iItemsProcessed = current;
		m_iTotalItems = total;
		m_sCurrentOperation = operation;
		m_OnProgressUpdate.Invoke(progress, current, total, operation);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Notifies client that operation completed successfully.
	//! \param[in] itemsTransferred Number of items successfully processed
	//! \param[in] itemsSkipped Number of items skipped
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	void RpcDo_OperationComplete(int itemsTransferred, int itemsSkipped)
	{
		m_bIsRunning = false;
		m_fProgress = 1.0;
		m_OnOperationComplete.Invoke(itemsTransferred, itemsSkipped);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Notifies client that operation failed with error.
	//! \param[in] errorMessage Description of the error
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	void RpcDo_OperationError(string errorMessage)
	{
		m_bIsRunning = false;
		m_fProgress = 0.0;
		m_OnOperationError.Invoke(errorMessage);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Called from server when starting an operation
	protected void StartOperation(string operationName)
	{
		m_bIsRunning = true;
		m_fProgress = 0.0;
		m_iItemsProcessed = 0;
		m_iTotalItems = 0;
		m_sCurrentOperation = operationName;
		
		// Notify client of start
		Rpc(RpcDo_UpdateProgress, 0.0, 0, 0, operationName);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get current progress (0.0 to 1.0)
	float GetProgress() 
	{ 
		return m_fProgress; 
	}
	
	//------------------------------------------------------------------------------------------------
	//! Check if an operation is currently running
	bool IsRunning() 
	{ 
		return m_bIsRunning; 
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get description of current operation
	string GetCurrentOperation() 
	{ 
		return m_sCurrentOperation; 
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get number of items processed so far
	int GetItemsProcessed() 
	{ 
		return m_iItemsProcessed; 
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get total number of items to process
	int GetTotalItems() 
	{ 
		return m_iTotalItems; 
	}
	
	//------------------------------------------------------------------------------------------------
	//! Helper to get entity from RplId
	protected IEntity GetEntityFromRplId(RplId rplId)
	{
		RplComponent rplComp = RplComponent.Cast(Replication.FindItem(rplId));
		if (!rplComp) return null;
		return rplComp.GetEntity();
	}
}