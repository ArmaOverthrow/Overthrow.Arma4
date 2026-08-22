//------------------------------------------------------------------------------------------------
//! The player-facing half of paid refuelling: the rate in the action label, and a real reason when
//! the pump refuses over money.
//!
//! Two small additions to the vanilla refuel action, both purely presentational - nothing here
//! decides anything. The authority for both lives in the modded SCR_FuelSupportStationComponent,
//! which is what set the refusal reason this class turns into a sentence, and which will re-derive
//! affordability on the server half a second later regardless of what was drawn here.
//!
//! WHY THE PRICE IS APPENDED AFTER super AND NOT INJECTED THROUGH GetActionStringParam. Vanilla's
//! GetActionNameScript picks ONE extra field to show: it asks GetActionStringParam first and only
//! falls back to the fill percentage when that comes back empty. Overriding the param to carry the
//! price would therefore silently delete the tank-percentage readout players already rely on.
//! Appending to the string super produced keeps both: "Refuel (37.5) ($1/L)".
//!
//! Everything about this class degrades to vanilla in one step: no config, no resolved station, a
//! free source, or a price of 0, and no suffix is drawn at all (implementation.md D9/R9).
//------------------------------------------------------------------------------------------------
modded class SCR_RefuelAtSupportStationAction : SCR_BaseUseSupportStationAction
{
	//! "%1 ($%2/L)" - %1 is the whole action name vanilla just built (fill percentage included),
	//! %2 the trimmed price. A key inside a larger rendered string is fine; the format itself has to
	//! be a key so the parentheses and the "/L" can be translated.
	protected const LocalizedString OVT_PRICE_FORMAT = "#OVT-Refuel_PriceFormat";

	//! Shown on the greyed action when the modded fuel station refused over money.
	protected const LocalizedString OVT_CANNOT_AFFORD_REASON = "#OVT-Refuel_CannotAfford";

	//------------------------------------------------------------------------------------------------
	//! Vanilla's action name, with the rate appended when this pump charges for its fuel.
	//!
	//! Only while the action is the selected one (the same condition under which vanilla shows the
	//! fill percentage) and only once a station has actually been resolved - which is also why a
	//! player who has been refused for being broke sees no price: GetClosestValidSupportStation
	//! returned null for them, so there is no station whose rate could honestly be quoted.
	//! \param[in,out] outName The action label, built by vanilla and extended here.
	//! \return True when there is a name to show, unchanged from vanilla's answer.
	override bool GetActionNameScript(out string outName)
	{
		if(!super.GetActionNameScript(outName))
			return false;

		if(!m_bActionActive) return true;
		if(!m_SupportStationComponent) return true;

		if(!OVT_Global.GetConfig()) return true;

		if(OVT_FuelUtils.IsFreeFuelSource(m_SupportStationComponent)) return true;

		float price = OVT_FuelUtils.GetFuelPricePerLitre();
		if(price <= 0) return true;

		outName = WidgetManager.Translate(OVT_PRICE_FORMAT, outName, OVT_FuelPricing.FormatPricePerLitre(price));

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Maps the one refusal reason Overthrow adds; every other reason stays vanilla's to explain.
	//! \param[in] reasonInvalid The reason the station refused.
	//! \return The localized sentence to show under the greyed action.
	protected override LocalizedString GetInvalidPerformReasonString(ESupportStationReasonInvalid reasonInvalid)
	{
		if(reasonInvalid == ESupportStationReasonInvalid.OVT_CANNOT_AFFORD_FUEL)
			return OVT_CANNOT_AFFORD_REASON;

		return super.GetInvalidPerformReasonString(reasonInvalid);
	}
}
