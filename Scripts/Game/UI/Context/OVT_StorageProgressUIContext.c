//------------------------------------------------------------------------------------------------
//! Storage progress UI context that extends OVT_UIContext for proper integration with Overthrow's UI system
//! Shows progress during storage transfer operations like FOB undeployment
class OVT_StorageProgressUIContext : OVT_UIContext
{
	protected ProgressBarWidget m_wProgressBar;
	protected TextWidget m_wProgressText;
	protected TextWidget m_wTitleText;
	protected TextWidget m_wOperationText;
	protected ButtonWidget m_wCancelButton;
	
	protected float m_fProgress = 0.0;
	protected int m_iCurrentContainer = 0;
	protected int m_iTotalContainers = 0;
	protected int m_iCurrentItem = 0;
	protected int m_iTotalItems = 0;
	protected string m_sCurrentOperation = "";
	protected string m_sOperationId = "";
	protected bool m_bCanCancel = false;
	protected bool m_bCompleted = false;
	
	//------------------------------------------------------------------------------------------------
	override void PostInit()
	{
		super.PostInit();
		
		// Set context properties for proper UI behavior
		m_sContextName = "StorageProgressContext";
		m_bHideHUDOnShow = false; // Keep HUD visible during storage operations
		m_bOpenActionCloses = false; // Don't close on open action
		m_Layout = "{F701351008A1B6AF}UI/Layouts/Menu/StorageProgressMenu.layout"; // Layout resource path
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnShow()
	{
		super.OnShow();
		
		if (!m_wRoot) return;
		
		// Find UI elements
		m_wProgressBar = ProgressBarWidget.Cast(m_wRoot.FindAnyWidget("ProgressBar"));
		m_wProgressText = TextWidget.Cast(m_wRoot.FindAnyWidget("ProgressText"));
		m_wTitleText = TextWidget.Cast(m_wRoot.FindAnyWidget("TitleText"));
		m_wOperationText = TextWidget.Cast(m_wRoot.FindAnyWidget("OperationText"));
		m_wCancelButton = ButtonWidget.Cast(m_wRoot.FindAnyWidget("CancelButton"));
		
		// Set up cancel button if available
		if (m_wCancelButton)
		{
			m_wCancelButton.SetVisible(m_bCanCancel);
			if (m_bCanCancel)
			{
				SCR_InputButtonComponent cancelAction = SCR_InputButtonComponent.Cast(m_wCancelButton.FindHandler(SCR_InputButtonComponent));
				if (cancelAction)
					cancelAction.m_OnActivated.Insert(OnCancelClicked);
			}
		}
		
		// Initialize display
		UpdateProgress();
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnClose()
	{
		super.OnClose();
		
		// Clean up references
		m_wProgressBar = null;
		m_wProgressText = null;
		m_wTitleText = null;
		m_wOperationText = null;
		m_wCancelButton = null;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Sets up the progress dialog for a new operation
	//! \param title Dialog title
	//! \param operationId Operation ID for cancellation support
	//! \param canCancel Whether user can cancel the operation
	void SetupOperation(string title, string operationId = "", bool canCancel = false)
	{
		if (m_wTitleText)
			m_wTitleText.SetText(title);
			
		m_sOperationId = operationId;
		m_bCanCancel = canCancel && !operationId.IsEmpty();
		m_bCompleted = false;
		
		// Update cancel button visibility
		if (m_wCancelButton)
			m_wCancelButton.SetVisible(m_bCanCancel);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Updates the progress display
	//! \param progress Progress value (0.0 to 1.0)
	//! \param currentContainer Current container being processed
	//! \param totalContainers Total number of containers
	//! \param currentItem Current item being processed
	//! \param totalItems Total number of items
	//! \param operation Current operation description
	void UpdateProgress(float progress = -1, int currentContainer = -1, int totalContainers = -1, 
						int currentItem = -1, int totalItems = -1, string operation = "")
	{
		// Update stored values if provided
		if (progress >= 0) m_fProgress = progress;
		if (currentContainer >= 0) m_iCurrentContainer = currentContainer;
		if (totalContainers >= 0) m_iTotalContainers = totalContainers;
		if (currentItem >= 0) m_iCurrentItem = currentItem;
		if (totalItems >= 0) m_iTotalItems = totalItems;
		if (!operation.IsEmpty()) m_sCurrentOperation = operation;
		
		// Update progress bar
		if (m_wProgressBar)
			m_wProgressBar.SetCurrent(m_fProgress);
		
		// Update progress text
		if (m_wProgressText)
		{
			int percentage = Math.Round(m_fProgress * 100);
			m_wProgressText.SetTextFormat("#OVT-ProgressPercent", percentage);
		}
		
		// Update operation text
		if (m_wOperationText)
		{
			string operationText = m_sCurrentOperation;
			
			if (m_iTotalContainers > 0)
			{
				m_wOperationText.SetTextFormat("#OVT-ContainerProgress", m_iCurrentContainer, m_iTotalContainers);
			}
			
			if (m_iTotalItems > 0)
			{
				m_wOperationText.SetTextFormat("#OVT-ItemProgress", m_iCurrentItem, m_iTotalItems);
			}
			
			
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Marks the operation as completed and shows results
	//! \param itemsTransferred Number of items successfully transferred
	//! \param itemsSkipped Number of items that were skipped
	void OnOperationComplete(int itemsTransferred, int itemsSkipped = 0)
	{
		if (m_bCompleted) return;
		m_bCompleted = true;
		
		// Update to 100% progress
		m_fProgress = 1.0;
		
		// Create completion message using localized strings
		string message;
		if (itemsSkipped > 0)
		{
			m_wOperationText.SetTextFormat("#OVT-StorageTransferCompleteWithSkipped", itemsTransferred, itemsSkipped);
		}
		else
		{
			m_wOperationText.SetTextFormat("#OVT-StorageTransferComplete", itemsTransferred);
		}
		
		// Update UI			
		if (m_wProgressBar)
			m_wProgressBar.SetCurrent(1.0);
			
		if (m_wProgressText)
			m_wProgressText.SetText("#OVT-ProgressComplete");
		
		// Hide cancel button
		if (m_wCancelButton)
			m_wCancelButton.SetVisible(false);
		
		// Auto-close after delay
		GetGame().GetCallqueue().CallLater(CloseLayout, 2000, false);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Shows an error message and closes the dialog
	//! \param errorMessage Error description
	void OnOperationError(string errorMessage)
	{
		if (m_bCompleted) return;
		m_bCompleted = true;
		
		// Update operation text with error
		if (m_wOperationText)
			m_wOperationText.SetTextFormat("#OVT-StorageTransferError", errorMessage);
		
		// Show error notification
		OVT_Global.GetNotify().SendTextNotification("#OVT-StorageTransferFailed", -1, 
			errorMessage);
		
		// Hide cancel button
		if (m_wCancelButton)
			m_wCancelButton.SetVisible(false);
		
		// Close after shorter delay for errors
		GetGame().GetCallqueue().CallLater(CloseLayout, 1500, false);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Handles cancel button click
	void OnCancelClicked()
	{
		if (m_bCompleted || !m_bCanCancel) return;
		
		// Send cancel request to inventory manager
		OVT_InventoryManagerComponent inventoryMgr = OVT_Global.GetInventory();
		if (inventoryMgr && !m_sOperationId.IsEmpty())
		{
			inventoryMgr.CancelOperation(m_sOperationId);
		}
		
		m_bCompleted = true;
		CloseLayout();
		
		// Show cancellation notification
		OVT_Global.GetNotify().SendTextNotification("#OVT-StorageTransferCancelled", -1);
	}
	
	//------------------------------------------------------------------------------------------------
	override bool CanShowLayout()
	{
		// Only allow showing if not already completed
		return !m_bCompleted;
	}
}