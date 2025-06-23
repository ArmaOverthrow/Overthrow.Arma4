class OVT_CampMenuContext : OVT_UIContext
{	
	OVT_CampData m_Camp;
	ref SCR_InputButtonComponent m_PrivacyButton;
	ref SCR_InputButtonComponent m_DeleteButton;
	
	override void OnShow()
	{		
		m_PrivacyButton = SCR_InputButtonComponent.Cast(m_wRoot.FindAnyWidget("TogglePrivacy").FindHandler(SCR_InputButtonComponent));
		m_PrivacyButton.m_OnClicked.Insert(TogglePrivacy);
		
		m_DeleteButton = SCR_InputButtonComponent.Cast(m_wRoot.FindAnyWidget("DeleteCamp").FindHandler(SCR_InputButtonComponent));
		m_DeleteButton.m_OnClicked.Insert(DeleteCamp);
		
		Widget closeButton = m_wRoot.FindAnyWidget("CloseButton");
		SCR_InputButtonComponent btn = SCR_InputButtonComponent.Cast(closeButton.FindHandler(SCR_InputButtonComponent));		
		btn.m_OnClicked.Insert(CloseLayout);
		
		Refresh();		
	}
	
	override void OnClose()
	{
		m_PrivacyButton.m_OnClicked.Remove(TogglePrivacy);
		m_DeleteButton.m_OnClicked.Remove(DeleteCamp);
	}
	
	protected void Refresh()
	{
		// Update camp name
		TextWidget nameText = TextWidget.Cast(m_wRoot.FindAnyWidget("CampName"));
		if(nameText)
			nameText.SetText(m_Camp.name);
		
		// Update privacy button text and status
		UpdatePrivacyDisplay();
	}
	
	protected void UpdatePrivacyDisplay()
	{
		if(m_Camp.isPrivate)
		{
			m_PrivacyButton.SetLabel("#OVT-MakeCampPublic");
		}
		else
		{
			m_PrivacyButton.SetLabel("#OVT-MakeCampPrivate");
		}
		
		// Update privacy status text
		TextWidget statusText = TextWidget.Cast(m_wRoot.FindAnyWidget("PrivacyStatus"));
		if(statusText)
		{
			if(m_Camp.isPrivate)
			{
				statusText.SetText("#OVT-CampPrivate");
				statusText.SetColor(Color.FromSRGBA(255, 100, 100, 255));
			}
			else
			{
				statusText.SetText("#OVT-CampPublic");
				statusText.SetColor(Color.FromSRGBA(100, 255, 100, 255));
			}
		}
	}
	
	protected void TogglePrivacy()
	{
		OVT_Global.GetServer().SetCampPrivacy(m_Camp, !m_Camp.isPrivate);
		UpdatePrivacyDisplay();
	}
	
	protected void DeleteCamp()
	{
		// Show confirmation dialog
		SCR_ConfigurableDialogUi dialog = SCR_ConfigurableDialogUi.CreateFromPreset("{5D86A311C893DBF0}Configs/UI/Dialogs/DialogPresets_Campaign.conf", "DELETE_CAMP");
		if(dialog)
		{
			dialog.m_OnConfirm.Insert(OnConfirmDelete);
		}
	}
	
	protected void OnConfirmDelete()
	{
		OVT_Global.GetServer().DeleteCamp(m_Camp);
		CloseLayout();
	}
}