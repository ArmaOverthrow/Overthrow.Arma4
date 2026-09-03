//------------------------------------------------------------------------------------------------
//! Overthrow has no arsenal. Every SCR_ArsenalComponent reports disabled and lists no stock, so a
//! vanilla arsenal box or gadget that reaches a world (composition, admin spawn) cannot hand out
//! copies of its items or refund items dropped into it. GitHub issue #172.
//! The server request path trusts the item catalog and never asks the arsenal, so the client list
//! is the gate: an empty list means no request.
//------------------------------------------------------------------------------------------------
modded class SCR_ArsenalComponent
{
	//------------------------------------------------------------------------------------------------
	//! \return Always false.
	override bool IsArsenalEnabled()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Always false.
	override bool IsRefundEnabled()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[out] availablePrefabs Always emptied.
	//! \return Always false.
	override bool GetAvailablePrefabs(out notnull array<ResourceName> availablePrefabs)
	{
		availablePrefabs.Clear();
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[out] filteredArsenalItems Always emptied.
	//! \param[in] requiresDisplayType Unused.
	//! \return Always false.
	override bool GetFilteredArsenalItems(out notnull array<SCR_ArsenalItem> filteredArsenalItems, EArsenalItemDisplayType requiresDisplayType = -1)
	{
		filteredArsenalItems.Clear();
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[out] filteredArsenalItems Always emptied.
	//! \param[in] requiresDisplayType Unused.
	//! \return Always false.
	override bool GetFilteredOverwriteArsenalItems(out notnull array<SCR_ArsenalItem> filteredArsenalItems, EArsenalItemDisplayType requiresDisplayType = -1)
	{
		filteredArsenalItems.Clear();
		return false;
	}
}
