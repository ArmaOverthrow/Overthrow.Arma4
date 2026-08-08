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
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	void RpcDo_UpdateProgress(float progress, int current, int total)
	{
		m_fProgress = progress;
		m_iItemsProcessed = current;
		m_iTotalItems = total;
		m_OnProgressUpdate.Invoke(progress, current, total, m_sCurrentOperation);
		
		// Also notify the controller
		OVT_OverthrowController controller = OVT_OverthrowController.Cast(GetOwner());
		if (controller && controller.GetProgressEvents())
			controller.GetProgressEvents().InvokeProgressUpdate(progress, current, total);
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
		
		// Also notify the controller
		OVT_OverthrowController controller = OVT_OverthrowController.Cast(GetOwner());
		if (controller && controller.GetProgressEvents())
			controller.GetProgressEvents().InvokeProgressComplete(itemsTransferred, itemsSkipped);
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
		
		// Also notify the controller
		OVT_OverthrowController controller = OVT_OverthrowController.Cast(GetOwner());
		if (controller && controller.GetProgressEvents())
			controller.GetProgressEvents().InvokeProgressError(errorMessage);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Notifies client that a new operation has started.
	//! \param[in] operationName Name of the operation
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	void RpcDo_OperationStart(string operationName)
	{
		m_bIsRunning = true;
		m_sCurrentOperation = operationName;
		
		// Notify the controller about the operation start
		OVT_OverthrowController controller = OVT_OverthrowController.Cast(GetOwner());
		if (controller && controller.GetProgressEvents())
			controller.GetProgressEvents().InvokeProgressStart(operationName);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Called from server when starting an operation
	//! \param[in] operationNameKey Localized string key (e.g., "#OVT-Progress-TransferringItems")
	protected void StartOperation(string operationNameKey)
	{
		m_bIsRunning = true;
		m_fProgress = 0.0;
		m_iItemsProcessed = 0;
		m_iTotalItems = 0;
		m_sCurrentOperation = operationNameKey;

		// Send operation start RPC with the operation name
		SendOperationStart(operationNameKey);

		// Send initial progress update (without redundant operation text)
		SendProgressUpdate(0.0, 0, 0, operationNameKey);
	}

	//------------------------------------------------------------------------------------------------
	//! Tells the owning client an operation has begun. ALWAYS use this instead of a bare
	//! Rpc(RpcDo_OperationStart, ...) - see IsLocalPlayerOwner for why (BUG-090).
	//! \param[in] operationName Localized string key for the operation.
	protected void SendOperationStart(string operationName)
	{
		if (IsLocalPlayerOwner())
		{
			RpcDo_OperationStart(operationName);
			return;
		}

		Rpc(RpcDo_OperationStart, operationName);
	}

	//------------------------------------------------------------------------------------------------
	//! Tells the owning client how far along the operation is. ALWAYS use this instead of a bare
	//! Rpc(RpcDo_UpdateProgress, ...) - see IsLocalPlayerOwner for why (BUG-090).
	//!
	//! \a operation is deliberately NOT put on the wire: the client already learned the operation
	//! name from RpcDo_OperationStart and caches it in m_sCurrentOperation, which is what gets
	//! handed to m_OnProgressUpdate here. It stays in the signature because that is the shape the
	//! storage callbacks (OVT_StorageProgressCallback.OnProgressUpdate) report in.
	//! \param[in] progress Progress value from 0.0 to 1.0.
	//! \param[in] current Current item being processed.
	//! \param[in] total Total items to process.
	//! \param[in] operation Operation name reported by the caller; not replicated.
	void SendProgressUpdate(float progress, int current, int total, string operation)
	{
		if (IsLocalPlayerOwner())
		{
			RpcDo_UpdateProgress(progress, current, total);
			return;
		}

		Rpc(RpcDo_UpdateProgress, progress, current, total);
	}

	//------------------------------------------------------------------------------------------------
	//! Reports a finished operation.
	//!
	//! The local call is not only about the host: server-side listeners (the resistance manager's
	//! FOB deploy/collect continuations subscribe to m_OnOperationComplete on the SERVER's copy of
	//! the component) need the handler to run on the server every time, dedicated or not. On a
	//! listen server that same call is also the host's only delivery, so the Rpc is skipped there.
	//! \param[in] itemsTransferred Number of items successfully processed.
	//! \param[in] itemsSkipped Number of items skipped.
	void SendOperationComplete(int itemsTransferred, int itemsSkipped)
	{
		RpcDo_OperationComplete(itemsTransferred, itemsSkipped);

		if (IsLocalPlayerOwner()) return;

		Rpc(RpcDo_OperationComplete, itemsTransferred, itemsSkipped);
	}

	//------------------------------------------------------------------------------------------------
	//! Reports a failed operation. Same server-local-then-replicate rule as SendOperationComplete -
	//! the resistance manager clears its pending FOB state from m_OnOperationError.
	//! \param[in] errorMessage Description of the error.
	void SendOperationError(string errorMessage)
	{
		RpcDo_OperationError(errorMessage);

		if (IsLocalPlayerOwner()) return;

		Rpc(RpcDo_OperationError, errorMessage);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the controller this component sits on belongs to THIS process's local player.
	//!
	//! The engine never loops an Rpc back to the sender, so on a listen server an
	//! RplRcver.Owner RPC sent from the server to the host's own controller is silently dropped and
	//! the host sees no progress at all (BUG-090). Callers use this to run the handler directly
	//! instead. On a dedicated server GetLocalPlayerId() is 0, no controller is registered for it,
	//! and every send goes over the wire exactly as before.
	//! \return True when this component's owner is the local player's controller.
	protected bool IsLocalPlayerOwner()
	{
		int localPlayerId = SCR_PlayerController.GetLocalPlayerId();
		if (localPlayerId <= 0) return false;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players) return false;

		IEntity localController = players.GetController(localPlayerId);
		if (!localController) return false;

		return localController == GetOwner();
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