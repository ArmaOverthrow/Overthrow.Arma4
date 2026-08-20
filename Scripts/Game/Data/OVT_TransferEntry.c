//------------------------------------------------------------------------------------------------
//! How a transfer row draws its image: a prefab preview or a flat texture.
//------------------------------------------------------------------------------------------------
enum EOVT_TransferImageKind
{
	PREFAB,		//! m_sImage is a prefab ResourceName, drawn by an ItemPreviewWidget
	TEXTURE		//! m_sImage is a texture/imageset ResourceName, drawn by an ImageWidget
}

//------------------------------------------------------------------------------------------------
//! What the single value column on a transfer row means.
//------------------------------------------------------------------------------------------------
enum EOVT_TransferValueKind
{
	PRICE,		//! Unit price - the value column is money and the cart totals cost
	QUANTITY	//! Stock held - the value column is a count and the cart totals items
}

//------------------------------------------------------------------------------------------------
//! One browsable row in a transfer screen - an importable item, a warehouse stack, later a resource.
//!
//! Deliberately dumb: what a row draws and what a cart line needs, and nothing else. No entity
//! handles, no widgets, no manager lookups, so the models above it stay Logic-tier testable.
//------------------------------------------------------------------------------------------------
class OVT_TransferEntry : Managed
{
	string m_sId;								//! Stable identity; the cart keys on this
	string m_sDisplayName;						//! Already-resolved name, what the sort orders by
	EOVT_TransferImageKind m_eImageKind;		//! Which of the two image widgets to show
	ResourceName m_sImage;						//! The prefab, or the texture/imageset
	int m_iValue;								//! Unit price (PRICE) or stock held (QUANTITY)
	EOVT_TransferValueKind m_eValueKind;		//! Drives formatting and the default summary
	int m_iMaxQuantity;							//! Cap the cart clamps to
	int m_iCategoryId;							//! Consumer-defined. Never OVT_TransferListModel.CATEGORY_ALL
	bool m_bEnabled;							//! False draws the row dimmed
	string m_sDisabledReasonKey;				//! #OVT- key naming why, empty when enabled

	//------------------------------------------------------------------------------------------------
	//! Builds an empty row. Every field is set explicitly - `new` applies no attribute defaults.
	void OVT_TransferEntry()
	{
		m_sId = "";
		m_sDisplayName = "";
		m_eImageKind = EOVT_TransferImageKind.PREFAB;
		m_sImage = ResourceName.Empty;
		m_iValue = 0;
		m_eValueKind = EOVT_TransferValueKind.QUANTITY;
		m_iMaxQuantity = 0;
		m_iCategoryId = 0;
		m_bEnabled = true;
		m_sDisabledReasonKey = "";
	}
}
