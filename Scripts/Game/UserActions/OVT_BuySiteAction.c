//------------------------------------------------------------------------------------------------
//! "Buy sawmill ($16,000)" - takes ownership of an unowned production site with PERSONAL money.
//!
//! HIDDEN ON AN OWNED SITE, because there is nothing to buy: a site is never resold, so the action is
//! genuinely irrelevant rather than merely blocked. Everything else - no money, no site record - is
//! VISIBLE WITH A REASON, following the shipped gate policy of OVT_OpenResourceStoreAction.
//!
//! THE PRICE SHOWN IS THE PRICE CHARGED. GetCost() is the client half of one shared evaluation:
//! OVT_ResourceProductionRules.BuyCost(the site's authored base cost, the difficulty's real-estate
//! multiplier). The server re-derives it from the same two replicated inputs inside RpcAsk_BuySite,
//! so there is no second formula to drift. Everything here is advisory; the authority re-checks the
//! site, the ownership, the distance and the balance.
//!
//! The resistance-funded variant subclasses this one - the only differences are which account is
//! charged, the officer gate and the two localization keys.
//------------------------------------------------------------------------------------------------
class OVT_BuySiteAction : ScriptedUserAction
{
	//! How long the label is reused, in milliseconds of world time. The label is asked for every frame
	//! a player looks at the site (OVT_OpenResourceStoreAction's shape).
	protected const float LABEL_TTL_MS = 1000;

	protected float m_fLabelExpiresAt;
	protected bool m_bHasLabel;
	protected string m_sCachedName;

	//------------------------------------------------------------------------------------------------
	//! Ask the server to buy this site.
	//! \param[in] pOwnerEntity The site entity.
	//! \param[in] pUserEntity The acting character.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		OVT_ProductionSiteData rec = GetSiteRecord();
		if (!rec)
			return;

		OVT_ResourceProductionRequestComponent requests = OVT_ControllerComponent<OVT_ResourceProductionRequestComponent>.Get();
		if (!requests)
			return;

		requests.BuySite(rec.location, UsesResistanceFunds());
	}

	//------------------------------------------------------------------------------------------------
	//! Shown only while the site is unowned. MayBuySite is the single predicate - the ownership
	//! comparison is never re-implemented here.
	//! \param[in] user The character looking at the action.
	//! \return True when the site is for sale.
	override bool CanBeShownScript(IEntity user)
	{
		OVT_ProductionSiteData rec = GetSiteRecord();
		if (!rec)
			return false;

		return OVT_ResourceProductionRules.MayBuySite(rec.owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Blocked, with a reason, when the charged account cannot cover the price.
	//! \param[in] user The acting character.
	//! \return True when the request is worth sending.
	override bool CanBePerformedScript(IEntity user)
	{
		if (!user)
			return false;

		int cost = GetCost();
		if (cost <= 0)
		{
			SetCannotPerformReason("#OVT-ProdSite_NoSite");
			return false;
		}

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!economy)
		{
			SetCannotPerformReason("#OVT-ProdSite_Failed");
			return false;
		}

		if (!CanAfford(economy, user, cost))
		{
			SetCannotPerformReason(NoFundsKey());
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[out] outName The label to draw.
	//! \return Always true; this action always names itself.
	override bool GetActionNameScript(out string outName)
	{
		RefreshLabel();

		outName = m_sCachedName;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The purchase goes over the request component from the clicking machine.
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//-----------------------------------------------------------------------
	// SUBCLASS SEAMS
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! \return False - this variant charges the acting player.
	protected bool UsesResistanceFunds()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The localization key of the priced label. One "%1" placeholder, the formatted price.
	protected string LabelKey()
	{
		return "#OVT-ProdSite_Buy_Price";
	}

	//------------------------------------------------------------------------------------------------
	//! \return The localization key shown when the charged account is short.
	protected string NoFundsKey()
	{
		return "#OVT-ProdSite_NoMoney";
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the charged account covers the price. Advisory - the server tests the same balance.
	//! \param[in] economy The economy manager.
	//! \param[in] user The acting character.
	//! \param[in] cost The price.
	//! \return True when the account can pay.
	protected bool CanAfford(notnull OVT_EconomyManagerComponent economy, IEntity user, int cost)
	{
		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players)
			return false;

		return economy.PlayerHasMoney(players.GetPersistentIDFromControlledEntity(user), cost);
	}

	//-----------------------------------------------------------------------
	// HELPERS
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Rebuilds the label when it has gone stale.
	protected void RefreshLabel()
	{
		float now = GetWorldTimeMs();
		if (m_bHasLabel && now < m_fLabelExpiresAt)
			return;

		m_fLabelExpiresAt = now + LABEL_TTL_MS;
		m_bHasLabel = true;

		m_sCachedName = WidgetManager.Translate(LabelKey(), OVT_MoneyFormat.FormatMoney(GetCost()));
	}

	//------------------------------------------------------------------------------------------------
	//! THE CLIENT HALF OF THE ONE PRICE. Same static, same two inputs as RpcAsk_BuySite.
	//! \return The purchase price, or 0 when the site or the config cannot be read.
	protected int GetCost()
	{
		OVT_ResourceProductionComponent production = OVT_ComponentFinder<OVT_ResourceProductionComponent>.Find(GetOwner());
		if (!production)
			return 0;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return 0;

		return OVT_ResourceProductionRules.BuyCost(production.GetBaseCost(), config.GetRealEstateCostMultiplier());
	}

	//------------------------------------------------------------------------------------------------
	//! \return This site's discovered record, or null when the manager is not up.
	protected OVT_ProductionSiteData GetSiteRecord()
	{
		OVT_ResourceProductionManagerComponent manager = OVT_Global.GetProduction();
		if (!manager)
			return null;

		return manager.GetSiteForEntity(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	//! \return World time in milliseconds, or 0 outside a world.
	protected float GetWorldTimeMs()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return 0;

		return world.GetWorldTime();
	}
}
