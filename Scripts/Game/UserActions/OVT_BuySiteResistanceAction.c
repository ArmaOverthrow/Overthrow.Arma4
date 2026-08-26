//------------------------------------------------------------------------------------------------
//! "Buy sawmill for the resistance ($16,000)" - the officer-only, treasury-funded variant of
//! OVT_BuySiteAction. The site ends up owned by "resistance": readable by the whole faction, and
//! flippable only by officers.
//!
//! The officer test here is UX. The authority is IsOfficer(playerId) inside RpcAsk_BuySite.
//------------------------------------------------------------------------------------------------
class OVT_BuySiteResistanceAction : OVT_BuySiteAction
{
	//------------------------------------------------------------------------------------------------
	//! Shown only to an officer, and only while the site is unowned.
	//! \param[in] user The character looking at the action.
	//! \return True when this player may spend resistance funds on this site.
	override bool CanBeShownScript(IEntity user)
	{
		if (!super.CanBeShownScript(user))
			return false;

		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (!resistance)
			return false;

		return resistance.IsLocalPlayerOfficer();
	}

	//------------------------------------------------------------------------------------------------
	//! \return True - this variant charges the resistance treasury.
	override protected bool UsesResistanceFunds()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The localization key of the priced label. One "%1" placeholder, the formatted price.
	override protected string LabelKey()
	{
		return "#OVT-ProdSite_BuyResistance_Price";
	}

	//------------------------------------------------------------------------------------------------
	//! \return The localization key shown when the treasury is short.
	override protected string NoFundsKey()
	{
		return "#OVT-ProdSite_NoFunds";
	}

	//------------------------------------------------------------------------------------------------
	//! The treasury balance is streamed to every client, so this is an honest advisory answer.
	//! \param[in] economy The economy manager.
	//! \param[in] user Unused - the treasury is not the caller's account.
	//! \param[in] cost The price.
	//! \return True when the treasury can pay.
	override protected bool CanAfford(notnull OVT_EconomyManagerComponent economy, IEntity user, int cost)
	{
		return economy.ResistanceHasMoney(cost);
	}
}
