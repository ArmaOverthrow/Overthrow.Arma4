//------------------------------------------------------------------------------------------------
//! Cargo readout for a truck carrying resources.
//!
//! VEHICLE-SCOPED, NOT PLAYER-SCOPED. The display is authored on Wheeled_Truck_Base.et, so the owner
//! handed to OnStartDraw (and returned by GetOwnerEntity) is the truck itself, and every value drawn
//! comes from that truck's own OVT_ResourceStoreComponent. BUG-097's shape - one declaration per
//! occupiable prefab making per-instance state wrong - only bites data that belongs to the PLAYER,
//! which this display holds none of.
//!
//! READS ONLY REPLICATED STATE. OVT_ResourceStoreComponent carries its contents in one RplProp, so
//! GetLedger() is correct on every machine within a replication tick. There is no request and no
//! round trip; the only subscription is to the store's own change invoker.
//!
//! NON-INTERACTIVE. The layout contains text and one background image, nothing focusable, so it adds
//! no gamepad focus island.
//------------------------------------------------------------------------------------------------
class OVT_CargoInfo : SCR_InfoDisplay
{
	//! Resource lines the panel can show before it collapses the remainder into a "+N more" row.
	protected const int MAX_LINES = 6;

	//! The store is authored on the same prefab, so resolution normally succeeds on the first try.
	//! It can still miss on a streamed-in proxy whose components are not all up, hence the retry.
	protected const int RESOLVE_RETRY_INTERVAL_MS = 1000;

	protected Widget m_wCargoPanel;
	protected TextWidget m_wVolumeText;
	protected ref array<TextWidget> m_aLineWidgets;

	protected OVT_ResourceStoreComponent m_Store;
	protected bool m_bSubscribed;

	//! Resource id -> translated title. Titles cannot change for the life of a session.
	protected ref map<string, string> m_mNames;

	//------------------------------------------------------------------------------------------------
	//! Caches every widget the panel writes to and starts resolving the truck's store.
	//! \param[in] owner The truck this display is authored on
	override void OnStartDraw(IEntity owner)
	{
		super.OnStartDraw(owner);

		if (!m_wRoot)
			return;

		m_wCargoPanel = m_wRoot.FindAnyWidget("CargoPanel");
		m_wVolumeText = TextWidget.Cast(m_wRoot.FindAnyWidget("CargoVolumeText"));

		m_aLineWidgets = new array<TextWidget>();
		for (int i = 0; i < MAX_LINES; i++)
		{
			TextWidget line = TextWidget.Cast(m_wRoot.FindAnyWidget("CargoLine" + i.ToString()));
			if (line)
				m_aLineWidgets.Insert(line);
		}

		SetPanelVisible(false);
		ResolveStore();
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the subscription, any pending retry and every cached widget.
	//! \param[in] owner The truck this display is authored on
	override void OnStopDraw(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(ResolveStore);
		Unsubscribe();

		m_wCargoPanel = null;
		m_wVolumeText = null;
		m_aLineWidgets = null;
		m_mNames = null;

		super.OnStopDraw(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Finds the store on the owner truck and subscribes to its contents invoker.
	//! Retries on a fixed schedule while the display is drawing; OnStopDraw cancels the retry, so the
	//! loop can never outlive the panel.
	protected void ResolveStore()
	{
		if (m_bSubscribed)
			return;

		IEntity owner = GetOwnerEntity();
		if (owner)
			m_Store = OVT_ResourceStoreComponent.Cast(owner.FindComponent(OVT_ResourceStoreComponent));

		if (!m_Store)
		{
			GetGame().GetCallqueue().CallLater(ResolveStore, RESOLVE_RETRY_INTERVAL_MS);
			return;
		}

		m_Store.GetOnContentsChanged().Insert(OnContentsChanged);
		m_bSubscribed = true;

		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	protected void Unsubscribe()
	{
		if (m_bSubscribed && m_Store)
			m_Store.GetOnContentsChanged().Remove(OnContentsChanged);

		m_bSubscribed = false;
		m_Store = null;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] store The store that changed - always this truck's own
	protected void OnContentsChanged(OVT_ResourceStoreComponent store)
	{
		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	//! Redraws the volume line and the resource lines from the truck's replicated ledger.
	protected void Refresh()
	{
		if (!m_Store)
		{
			SetPanelVisible(false);
			return;
		}

		OVT_ResourceLedger ledger = m_Store.GetLedger();
		if (!ledger)
		{
			SetPanelVisible(false);
			return;
		}

		SetPanelVisible(true);
		DrawVolume();
		DrawLines(ledger);
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawVolume()
	{
		if (!m_wVolumeText)
			return;

		int capacity = m_Store.GetCapacityLitres();
		string used = FormatCubicMetres(m_Store.GetUsedLitres());

		if (capacity == OVT_ResourceStoreComponent.UNLIMITED_CAPACITY)
		{
			m_wVolumeText.SetTextFormat("#OVT-Cargo_VolumeUnlimited", used);
			return;
		}

		m_wVolumeText.SetTextFormat("#OVT-Cargo_Volume", used, FormatCubicMetres(capacity));
	}

	//------------------------------------------------------------------------------------------------
	//! One row per non-zero ledger line, oldest first, with any overflow collapsed into the last row.
	//! \param[in] ledger The truck's ledger
	protected void DrawLines(OVT_ResourceLedger ledger)
	{
		if (!m_aLineWidgets)
			return;

		array<string> ids = new array<string>();
		array<int> quantities = new array<int>();
		ledger.GetLines(ids, quantities);

		int drawn = 0;
		int slots = m_aLineWidgets.Count();

		for (int i = 0; i < ids.Count(); i++)
		{
			if (quantities[i] <= 0)
				continue;

			if (drawn >= slots)
				break;

			// The last usable slot becomes the overflow counter when more lines remain than fit
			bool lastSlot = (drawn == slots - 1);
			int remaining = CountRemaining(quantities, i);
			if (lastSlot && remaining > 1)
			{
				m_aLineWidgets[drawn].SetTextFormat("#OVT-Cargo_More", remaining);
				m_aLineWidgets[drawn].SetVisible(true);
				drawn++;
				break;
			}

			m_aLineWidgets[drawn].SetTextFormat("#OVT-Cargo_Line", quantities[i], ResolveName(ids[i]));
			m_aLineWidgets[drawn].SetVisible(true);
			drawn++;
		}

		for (int i = drawn; i < slots; i++)
		{
			m_aLineWidgets[i].SetVisible(false);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] quantities Ledger quantities in line order
	//! \param[in] from First index to count from, inclusive
	//! \return How many non-zero lines remain at or after from
	protected int CountRemaining(array<int> quantities, int from)
	{
		int count = 0;
		for (int i = from; i < quantities.Count(); i++)
		{
			if (quantities[i] > 0)
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] id A bare resource id
	//! \return The translated catalogue title, falling back to the id so a row is never blank
	protected string ResolveName(string id)
	{
		if (!m_mNames)
			m_mNames = new map<string, string>();

		string cached;
		if (m_mNames.Find(id, cached))
			return cached;

		string name = OVT_ResourceUtils.ResolveResourceTitle(id);

		m_mNames.Set(id, name);
		return name;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] litres Integer litres
	//! \return Cubic metres to one decimal, matching every other resource readout
	protected string FormatCubicMetres(int litres)
	{
		return OVT_ResourceUtils.FormatCubicMetres(litres);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] visible True to show the panel
	protected void SetPanelVisible(bool visible)
	{
		if (m_wCargoPanel)
			m_wCargoPanel.SetVisible(visible);
	}
}
