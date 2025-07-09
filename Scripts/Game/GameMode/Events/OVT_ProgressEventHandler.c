// Type definitions for progress system ScriptInvokers
void ScriptInvokerProgressStartMethod(string operationName);
typedef func ScriptInvokerProgressStartMethod;
typedef ScriptInvokerBase<ScriptInvokerProgressStartMethod> ScriptInvokerProgressStart;

void ScriptInvokerProgressUpdateMethod(float progress, int current, int total);
typedef func ScriptInvokerProgressUpdateMethod;
typedef ScriptInvokerBase<ScriptInvokerProgressUpdateMethod> ScriptInvokerProgressUpdate;

void ScriptInvokerProgressCompleteMethod(int itemsTransferred, int itemsSkipped);
typedef func ScriptInvokerProgressCompleteMethod;
typedef ScriptInvokerBase<ScriptInvokerProgressCompleteMethod> ScriptInvokerProgressComplete;

void ScriptInvokerProgressErrorMethod(string errorMessage);
typedef func ScriptInvokerProgressErrorMethod;
typedef ScriptInvokerBase<ScriptInvokerProgressErrorMethod> ScriptInvokerProgressError;

//------------------------------------------------------------------------------------------------
//! Encapsulates all progress-related events and functionality
class OVT_ProgressEventHandler : Managed
{
	// Progress system ScriptInvokers
	protected ref ScriptInvokerProgressStart m_OnProgressStart = new ScriptInvokerProgressStart();
	protected ref ScriptInvokerProgressUpdate m_OnProgressUpdate = new ScriptInvokerProgressUpdate();
	protected ref ScriptInvokerProgressComplete m_OnProgressComplete = new ScriptInvokerProgressComplete();
	protected ref ScriptInvokerProgressError m_OnProgressError = new ScriptInvokerProgressError();
	
	//------------------------------------------------------------------------------------------------
	//! Get the progress start event invoker
	ScriptInvokerProgressStart GetOnProgressStart()
	{
		return m_OnProgressStart;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get the progress update event invoker
	ScriptInvokerProgressUpdate GetOnProgressUpdate()
	{
		return m_OnProgressUpdate;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get the progress complete event invoker
	ScriptInvokerProgressComplete GetOnProgressComplete()
	{
		return m_OnProgressComplete;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get the progress error event invoker
	ScriptInvokerProgressError GetOnProgressError()
	{
		return m_OnProgressError;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Invoke progress start event
	void InvokeProgressStart(string operationName)
	{
		m_OnProgressStart.Invoke(operationName);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Invoke progress update event
	void InvokeProgressUpdate(float progress, int current, int total)
	{
		m_OnProgressUpdate.Invoke(progress, current, total);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Invoke progress complete event
	void InvokeProgressComplete(int itemsTransferred, int itemsSkipped)
	{
		m_OnProgressComplete.Invoke(itemsTransferred, itemsSkipped);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Invoke progress error event
	void InvokeProgressError(string errorMessage)
	{
		m_OnProgressError.Invoke(errorMessage);
	}
}
