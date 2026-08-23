//! Gun dealer location type for the Overthrow map system
//!
//! INFO PANEL (implementation plan section 4.6b): DIFFERENT CONTENT FROM THE SHOP PANEL, SAME LAYOUT.
//! A gun dealer's panel answers "is it worth walking to this one?", and top-N price carets cannot
//! answer that. What this dealer STOCKS can.
//!
//! Of everything a dealer carries, only a handful of ids are per-dealer picks - one RIFLE, one
//! SNIPER_RIFLE, one MACHINE_GUN and one ROCKET_LAUNCHER, plus one WEAPON_VARIANTS roll each for
//! rifle/sniper/MG, each rolled by m_bSingleRandomItem (OVT_TownController.c:288-310). Every pistol (including occupying-faction ones), all ammunition,
//! every attachment, throwable and explosive is IDENTICAL AT EVERY DEALER, so listing them would pad
//! the panel with rows that read the same everywhere and bury the four that differ.
//!
//! HONESTY CONSTRAINT (N14). Those four RE-ROLL ON EVERY CAMPAIGN LOAD: the generator is the global
//! unseeded s_AIRandomGenerator and nothing persists the result - the persistence config has no shop
//! entry and the dealer entity is explicitly UntrackTransient-ed as "respawned by this controller
//! every session". Only the position survives, in OVT_TownData.gunDealerPosition. Within a session
//! the set IS stable (restock tops up existing ids and never re-rolls), so the panel does not lie to
//! a player acting on it now - but the copy is present tense ("Available here") and must never imply
//! permanence. Do not write "always stocks" or "specialises in".
//!
//! NO PRICE, NO CURRENCY, NO PERCENTAGE AND NO STOCK COUNT is rendered.
[BaseContainerProps(), OVT_MapLocationTypeTitle()]
class OVT_MapLocationGunDealer : OVT_MapLocationShopBase
{
	//! Whether to draw a scarcity caret beside each signature weapon.
	//!
	//! DEFAULTS OFF, AND THE REASON IS IN THE ECONOMY, NOT IN THE UI. The scarcity term reads TOWN
	//! stock, which sums across m_mTownShops - and a gun dealer is deliberately NOT in that map
	//! (FilterShopEntities excludes dealers from m_aAllShops, and RegisterGunDealer inserts only into
	//! m_aGunDealers). Configs/System/ShopConfig.conf stocks no weapon at any town shop either, so
	//! town stock for every weapon is 0, the scarcity term is pinned at its +10% maximum, and every
	//! weapon at every dealer bands to three carets up. Four identical glyphs carry no information and
	//! read as "this dealer is a rip-off", which is not a per-dealer fact.
	//!
	//! The code path is live and shared with the shop panel: set this to 1 in Configs/Map/OverthrowMap.conf
	//! to see it on screen. It becomes informative the moment a town shop stocks weapons.
	[Attribute(defvalue: "0", desc: "Draw a price caret beside each signature weapon. Off by default: no town shop stocks weapons, so the town-wide scarcity term is pinned at its maximum and every weapon reads three carets up.", category: "Info")]
	protected bool m_bShowWeaponCarets;

	//------------------------------------------------------------------------------------------------
	//! Builds the four-weapon panel. Runs ONCE, when the panel opens (Q-7).
	//!
	//! A CATEGORY THE DEALER DID NOT ROLL IS ABSENT, never "None" - three rows is a correct answer.
	//! A dealer whose entity has not streamed in yet (N18 - RegisterGunDealer has no broadcast RPC, so
	//! a dealer registered after a client joined is missing from that client's list entirely) degrades
	//! to the header alone rather than to a script error.
	//! \param[in] locationInfoWidget The instantiated OVT_MapInfoShop.layout
	//! \param[in] location The dealer record being described
	override protected void OnSetupLocationInfo(Widget locationInfoWidget, OVT_MapLocationData location)
	{
		if (!locationInfoWidget || !location)
			return;

		if (!m_Economy)
			return;

		Widget rows = locationInfoWidget.FindAnyWidget(WIDGET_ROWS);

		// The distance-to-port term applies to any non-vehicle item, so the badge is as meaningful
		// here as at a town shop - and unlike the carets it genuinely varies between dealers.
		ShowRemotenessBadge(locationInfoWidget, m_PriceIndicator.GetRemotenessLevel(m_Economy, location.m_vPosition));

		OVT_ShopComponent dealer = GetShopForLocation(location);
		if (!dealer)
			return;

		array<ref OVT_MapShopPriceRow> weapons = new array<ref OVT_MapShopPriceRow>();
		m_PriceIndicator.CollectSignatureWeapons(m_Economy, m_TownManager, dealer, location.m_vPosition, weapons);

		if (weapons.IsEmpty())
		{
			AddInfoRow(rows, "", "#OVT-Map_GunDealer_None");
			return;
		}

		ShowSectionHeader(locationInfoWidget, "#OVT-Map_GunDealer_Available");

		Widget weaponRows = locationInfoWidget.FindAnyWidget(WIDGET_SCARCITY_ROWS);

		foreach (OVT_MapShopPriceRow weapon : weapons)
		{
			string kindKey = OVT_MapShopPriceIndicator.GetWeaponTypeKey(weapon.m_eWeaponType);

			if (m_bShowWeaponCarets)
			{
				AddCaretRow(weaponRows, weapon.m_eLevel, kindKey, weapon.m_sDisplayName);
				continue;
			}

			AddInfoRow(weaponRows, kindKey, weapon.m_sDisplayName);
		}
	}

	override void PopulateLocations(array<ref OVT_MapLocationData> locations)
	{
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!economy)
			return;

		// Get all gun dealers from economy manager
		array<RplId> gunDealers = economy.GetGunDealers();
		if (!gunDealers)
			return;

		foreach (RplId dealerId : gunDealers)
		{
			// Get the gun dealer entity from replication
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(dealerId));
			if (!rpl)
				continue;

			IEntity dealerEntity = rpl.GetEntity();
			if (!dealerEntity || !dealerEntity.GetWorld())
				continue;

			// Create location data for this gun dealer
			OVT_MapLocationData locationData = new OVT_MapLocationData(dealerEntity.GetOrigin(), "#OVT-GunDealer", ClassName());
			locationData.m_EntityID = dealerEntity.GetID();
			locationData.m_RplID = dealerId;
			locationData.m_pEntity = dealerEntity;

			locations.Insert(locationData);
		}
	}
}
