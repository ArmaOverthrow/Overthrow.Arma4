class OVT_ProgressInfo : SCR_InfoDisplay
{
	// UI widgets
	protected ref Widget m_wProgressFrame;
	protected ref TextWidget m_wOperationText;
	protected ref TextWidget m_wProgressText;
	protected ref SCR_InventoryProgressBar m_ProgressBar;
	
	// Progress state
	protected bool m_bIsActive = false;
	protected float m_fCurrentProgress = 0.0;
	protected string m_sCurrentOperation = "";
	protected int m_iCurrentItems = 0;
	protected int m_iTotalItems = 0;
	
	// Controller reference
	protected OVT_OverthrowController m_Controller;
	
	//------------------------------------------------------------------------------------------------
	override void OnStartDraw(IEntity owner)
	{
		super.OnStartDraw(owner);
		
		if (!m_wRoot)
			return;
		
		// Cache widget references
		m_wProgressFrame = m_wRoot.FindAnyWidget("ProgressFrame");
		Widget w = m_wRoot.FindAnyWidget("OperationText");
		m_wOperationText = TextWidget.Cast(m_wRoot.FindAnyWidget("OperationText"));
		m_wProgressText = TextWidget.Cast(m_wRoot.FindAnyWidget("ProgressText"));
		
		// Get the progress bar component
		Widget progressBarWidget = m_wRoot.FindAnyWidget("ProgressBar");
		if (progressBarWidget)
			m_ProgressBar = SCR_InventoryProgressBar.Cast(progressBarWidget.FindHandler(SCR_InventoryProgressBar));
		
		// Subscribe to controller events
		SubscribeToController();
		
		// Initially hide the progress display
		SetProgressVisible(false);
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnStopDraw(IEntity owner)
	{
		UnsubscribeFromController();
		super.OnStopDraw(owner);
	}
	
	//------------------------------------------------------------------------------------------------
	protected void SubscribeToController()
	{
		m_Controller = OVT_Global.GetController();
		if (!m_Controller)
			return;
		
		OVT_ProgressEventHandler progressEvents = m_Controller.GetProgressEvents();
		if (!progressEvents)
			return;
		
		progressEvents.GetOnProgressStart().Insert(OnProgressStart);
		progressEvents.GetOnProgressUpdate().Insert(OnProgressUpdate);
		progressEvents.GetOnProgressComplete().Insert(OnProgressComplete);
		progressEvents.GetOnProgressError().Insert(OnProgressError);
	}
	
	//------------------------------------------------------------------------------------------------
	protected void UnsubscribeFromController()
	{
		if (!m_Controller)
			return;
		
		OVT_ProgressEventHandler progressEvents = m_Controller.GetProgressEvents();
		if (!progressEvents)
			return;
		
		progressEvents.GetOnProgressStart().Remove(OnProgressStart);
		progressEvents.GetOnProgressUpdate().Remove(OnProgressUpdate);
		progressEvents.GetOnProgressComplete().Remove(OnProgressComplete);
		progressEvents.GetOnProgressError().Remove(OnProgressError);
	}
	
	//------------------------------------------------------------------------------------------------
	protected void OnProgressStart(string operationName)
	{
		m_sCurrentOperation = operationName;
		m_fCurrentProgress = 0.0;
		m_iCurrentItems = 0;
		m_iTotalItems = 0;
		m_bIsActive = true;
		
		// Update operation text (already localized string key)
		if (m_wOperationText)
			m_wOperationText.SetText(operationName);
		
		// Initialize progress bar
		if (m_ProgressBar)
		{
			m_ProgressBar.SetProgressRange(0, 1);
			m_ProgressBar.SetCurrentProgress(0);
		}
		
		// Clear progress text
		if (m_wProgressText)
			m_wProgressText.SetText(" ");
		
		SetProgressVisible(true);
	}
	
	//------------------------------------------------------------------------------------------------
	protected void OnProgressUpdate(float progress, int current, int total, string operation)
	{
		m_fCurrentProgress = progress;
		m_sCurrentOperation = operation;
		m_iCurrentItems = current;
		m_iTotalItems = total;
				
		// Update progress bar
		if (m_ProgressBar)
			m_ProgressBar.SetCurrentProgress(progress);
		
		// Update progress text
		if (m_wProgressText)
		{
			if (total > 0)
			{
				// Show item count
				m_wProgressText.SetTextFormat("%1/%2", current, total);
			}
			else
			{
				// Show percentage
				int percentage = Math.Round(progress * 100);
				m_wProgressText.SetTextFormat("%1%%", percentage);
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected void OnProgressComplete(int itemsTransferred, int itemsSkipped)
	{
		// Show completion message with localized strings
		if (m_wProgressText)
		{
			if (itemsSkipped > 0)
				m_wProgressText.SetTextFormat("#OVT-Progress-CompletedWithSkipped", itemsTransferred, itemsSkipped);
			else
				m_wProgressText.SetTextFormat("#OVT-Progress-Completed", itemsTransferred);
		}
		
		// Set progress to full
		if (m_ProgressBar)
			m_ProgressBar.SetCurrentProgress(1.0);
		
		// Hide after a short delay
		GetGame().GetCallqueue().CallLater(HideProgress, 2000);
	}
	
	//------------------------------------------------------------------------------------------------
	protected void OnProgressError(string errorMessage)
	{
		// Show error message with localized string
		if (m_wProgressText)
		{
			m_wProgressText.SetTextFormat("#OVT-Progress-Error", errorMessage);
			m_wProgressText.SetColor(Color.Red);
		}
		
		// Hide after a short delay
		GetGame().GetCallqueue().CallLater(HideProgress, 3000);
	}
	
	//------------------------------------------------------------------------------------------------
	protected void HideProgress()
	{
		m_bIsActive = false;
		SetProgressVisible(false);
		
		// Reset text color
		if (m_wOperationText)
			m_wOperationText.SetColor(Color.White);
	}
	
	//------------------------------------------------------------------------------------------------
	protected void SetProgressVisible(bool visible)
	{
		if (m_wProgressFrame)
			m_wProgressFrame.SetVisible(visible);
	}
	
}