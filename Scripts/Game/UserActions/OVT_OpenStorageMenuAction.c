//------------------------------------------------------------------------------------------------
//! "Storage (N)" - opens the Open Storage screen on this holder.
//!
//! The number is the holder's REPLICATED count, so it is correct on every client within one
//! replication tick and updates without anybody opening a menu.
//------------------------------------------------------------------------------------------------
class OVT_OpenStorageMenuAction : OVT_StorageActionBase
{
	//! Cached label and the count it was built for; a fresh string per frame is the only thing this
	//! avoids (OVT_FillFuelAction's pattern).
	protected int m_iCachedCount = -1;
	protected string m_sCachedName;

	//------------------------------------------------------------------------------------------------
	//! \param[in] pOwnerEntity The holder.
	//! \param[in] pUserEntity The acting character.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!CanBePerformedScript(pUserEntity))
			return;

		OVT_UIManagerComponent uimanager = OVT_UIManagerComponent.Cast(pUserEntity.FindComponent(OVT_UIManagerComponent));
		if (!uimanager)
			return;

		OVT_StorageContext context = OVT_StorageContext.Cast(uimanager.GetContext(OVT_StorageContext));
		if (!context)
			return;

		context.SetHolder(OVT_StorageUtils.ResolveStorageHolder(pOwnerEntity));

		uimanager.ShowContext(OVT_StorageContext);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[out] outName The label to draw.
	//! \return Always true; this action always names itself.
	override bool GetActionNameScript(out string outName)
	{
		OVT_StorageComponent storage = GetStorage();
		if (!storage)
		{
			outName = "#OVT-Storage_Open";
			return true;
		}

		int count = storage.GetTotalCount();
		if (count != m_iCachedCount || m_sCachedName == "")
		{
			m_iCachedCount = count;
			m_sCachedName = "#OVT-Storage_Open (" + count.ToString() + ")";
		}

		outName = m_sCachedName;
		return true;
	}
}
