//------------------------------------------------------------------------------------------------
//! A context that can host a row of OVT_ShopMenuTabComponent tabs.
//!
//! EnforceScript has no interfaces, so the tab component's "host" contract is an intermediate class
//! on the one ancestor every context already shares. Only tab hosts inherit it.
//------------------------------------------------------------------------------------------------
class OVT_TabHostContext : OVT_UIContext
{
	//------------------------------------------------------------------------------------------------
	//! A tab was picked.
	//! \param[in] tabId The host's own id for that tab.
	void SelectTabId(int tabId)
	{
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] tabId A tab id.
	//! \return True when tabId is the host's active tab.
	bool IsTabIdActive(int tabId)
	{
		return false;
	}
}
