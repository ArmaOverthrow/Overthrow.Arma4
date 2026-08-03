//------------------------------------------------------------------------------------------------
//! One card in the shop browser grid - a buyable shop stock line, a sellable inventory stack, or a
//! vehicle upgrade (OVT_ManageVehicleContext reuses this card).
//!
//! The 15 card widgets in ShopMenu.layout are REUSED between refreshes, so Init() must set every
//! visual it can change, in both directions. A card that only ever dimmed itself would stay dim
//! after the next page turn.
//------------------------------------------------------------------------------------------------
class OVT_ShopMenuCardComponent : SCR_ScriptedWidgetComponent
{
	//! Dimmed text on a card the shop will not buy (implementation plan R12: tinting the root does
	//! not visibly affect the 3D item preview, so the text is dimmed explicitly).
	protected const int COLOR_TEXT_DISABLED = 0xFF808080;

	//! Red tint applied to the card root, following OVT_PlaceMenuCardComponent's can't-do-this look.
	protected const int COLOR_ROOT_DISABLED = 0xFFFF4444;

	protected const float OPACITY_ENABLED = 1.0;
	protected const float OPACITY_DISABLED = 0.4;

	protected ResourceName m_Resource;
	protected OVT_UIContext m_Context;
	protected bool m_bEnabled;
	protected string m_sDisabledReasonKey;

	//------------------------------------------------------------------------------------------------
	//! Draws one row onto this card.
	//! \param[in] res Prefab shown by the card (preview image and name are resolved from it).
	//! \param[in] cost Price to show, or -1 to hide the price.
	//! \param[in] qty Quantity to show in the stock corner, or -1 to hide it.
	//! \param[in] context Menu to notify when the card is clicked.
	//! \param[in] enabled False dims the card; it stays clickable so the details pane can explain why
	//! (Definition of Done F9 - greyed out, still selectable).
	//! \param[in] reasonKey Localization key naming why the card is disabled. Empty when enabled.
	void Init(ResourceName res, int cost, int qty, OVT_UIContext context, bool enabled = true, string reasonKey = "")
	{
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();

		m_Resource = res;
		m_Context = context;
		m_bEnabled = enabled;
		m_sDisabledReasonKey = reasonKey;

		if(!m_wRoot) return;

		TextWidget text = TextWidget.Cast(m_wRoot.FindAnyWidget("EntityName"));

		TextWidget costWidget = TextWidget.Cast(m_wRoot.FindAnyWidget("Cost"));
		if(costWidget)
		{
			if(cost == -1)
			{
				costWidget.SetVisible(false);
			}else{
				costWidget.SetVisible(true);
				costWidget.SetText(OVT_MoneyFormat.FormatMoney(cost));
			}
		}

		// The Stock widget is authored "Is Visible" 0 and the legacy code only ever called
		// SetVisible(false) on it, so a shop card has never rendered its stock count. Sell mode needs
		// the carried quantity, and the buy grid gets the stock it always meant to show.
		TextWidget qtyWidget = TextWidget.Cast(m_wRoot.FindAnyWidget("Stock"));
		if(qtyWidget)
		{
			if(qty == -1)
			{
				qtyWidget.SetVisible(false);
			}else{
				qtyWidget.SetVisible(true);
				qtyWidget.SetText(qty.ToString());
			}
		}

		ApplyEnabledVisual(text, costWidget, qtyWidget);

		// Name and image are resolved BEFORE the preview widgets are touched, so a missing preview
		// widget or preview manager can never leave the card unlabelled.
		bool isVehicle = false;
		if(economy && economy.IsVehicle(res)) isVehicle = true;

		string displayName = "";
		ResourceName vehicleImage;

		if(isVehicle)
		{
			SCR_EditableVehicleUIInfo info = OVT_Global.GetVehicleUIInfo(res);
			if(info)
			{
				displayName = info.GetName();
				vehicleImage = info.GetImage();
			}else{
				SCR_EditableEntityUIInfo uiinfo = OVT_Global.GetEditableUIInfo(res);
				if(uiinfo)
				{
					displayName = uiinfo.GetName();
					vehicleImage = uiinfo.GetImage();
				}
			}
		}else{
			UIInfo info = OVT_Global.GetItemUIInfo(res);
			if(info) displayName = info.GetName();
		}

		if(text) text.SetText(displayName);

		ItemPreviewWidget img = ItemPreviewWidget.Cast(m_wRoot.FindAnyWidget("Image"));
		ImageWidget tex = ImageWidget.Cast(m_wRoot.FindAnyWidget("Texture"));

		ChimeraWorld world = GetGame().GetWorld();
		ItemPreviewManagerEntity manager = world.GetItemPreviewManager();
		if (!manager)
			return;

		if(!img || !tex)
			return;

		// Set rendering and preview properties

		img.SetResolutionScale(1, 1);

		if(isVehicle)
		{
			tex.LoadImageTexture(0, vehicleImage);
			img.SetVisible(false);
			tex.SetVisible(true);
		}else{
			manager.SetPreviewItemFromPrefab(img, res);
			img.SetVisible(true);
			tex.SetVisible(false);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return The prefab this card is currently showing.
	ResourceName GetResource()
	{
		return m_Resource;
	}

	//------------------------------------------------------------------------------------------------
	//! \return False when the card is drawn dimmed (the shop will not buy this row).
	bool IsEnabled()
	{
		return m_bEnabled;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The localization key naming why this card is disabled, or an empty string.
	string GetDisabledReasonKey()
	{
		return m_sDisabledReasonKey;
	}

	//------------------------------------------------------------------------------------------------
	//! Applies (or clears) the disabled look. Both directions are written every Init because the card
	//! widgets are pooled.
	protected void ApplyEnabledVisual(TextWidget name, TextWidget cost, TextWidget stock)
	{
		if(!m_wRoot) return;

		if(m_bEnabled)
		{
			m_wRoot.SetOpacity(OPACITY_ENABLED);
			m_wRoot.SetColor(Color.White);

			if(name) name.SetColor(Color.White);
			if(cost) cost.SetColor(Color.White);
			if(stock) stock.SetColor(Color.White);
			return;
		}

		m_wRoot.SetOpacity(OPACITY_DISABLED);
		m_wRoot.SetColor(Color.FromInt(COLOR_ROOT_DISABLED));

		if(name) name.SetColor(Color.FromInt(COLOR_TEXT_DISABLED));
		if(cost) cost.SetColor(Color.FromInt(COLOR_TEXT_DISABLED));
		if(stock) stock.SetColor(Color.FromInt(COLOR_TEXT_DISABLED));
	}

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if(!m_Context) return false;

		super.OnClick(w, x, y, button);
		if (button != 0)
			return false;

		// Disabled cards stay selectable on purpose - the details pane is where the player is told
		// why the shop will not buy this (F9).
		m_Context.SelectItem(m_Resource);

		return true;
	}
}
