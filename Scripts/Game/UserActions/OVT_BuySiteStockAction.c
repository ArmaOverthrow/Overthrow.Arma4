//------------------------------------------------------------------------------------------------
//! "Buy stock (42)" - opens OVT_ProductionSiteBuyContext on an unowned production site.
//!
//! SHOWN ONLY WHILE UNOWNED (D13/§3.9 Sort 3) - OVT_ResourceProductionRules.MayBuyStock is the
//! single predicate, never re-implemented here. The work goes over the request component from the
//! clicking machine (HasLocalEffectOnlyScript), the OVT_OpenResourceStoreAction shape.
//------------------------------------------------------------------------------------------------
class OVT_BuySiteStockAction : ScriptedUserAction
{
	//! How long the label is reused, in milliseconds of world time. OVT_OpenResourceStoreAction's shape.
	protected const float LABEL_TTL_MS = 1000;

	protected float m_fLabelExpiresAt;
	protected bool m_bHasLabel;
	protected string m_sCachedName;

	//------------------------------------------------------------------------------------------------
	//! Opens the buy screen on this site, on the clicking machine.
	//! \param[in] pOwnerEntity The site entity.
	//! \param[in] pUserEntity The acting character.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pUserEntity)
			return;

		OVT_UIManagerComponent uimanager = OVT_UIManagerComponent.Cast(pUserEntity.FindComponent(OVT_UIManagerComponent));
		if (!uimanager)
			return;

		OVT_ProductionSiteBuyContext context = OVT_ProductionSiteBuyContext.Cast(uimanager.GetContext(OVT_ProductionSiteBuyContext));
		if (!context)
			return;

		context.SetSite(pOwnerEntity);

		uimanager.ShowContext(OVT_ProductionSiteBuyContext);
	}

	//------------------------------------------------------------------------------------------------
	//! Shown only while the site is unowned. MayBuyStock is the single predicate - the ownership
	//! comparison is never re-implemented here.
	//! \param[in] user The character looking at the action.
	//! \return True when the site has stock for sale.
	override bool CanBeShownScript(IEntity user)
	{
		OVT_ProductionSiteData rec = GetSiteRecord();
		if (!rec)
			return false;

		return OVT_ResourceProductionRules.MayBuyStock(rec.owner);
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
	//! The screen and the purchase both run over the request component from the clicking machine.
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Rebuilds the label when it has gone stale.
	protected void RefreshLabel()
	{
		float now = GetWorldTimeMs();
		if (m_bHasLabel && now < m_fLabelExpiresAt)
			return;

		m_fLabelExpiresAt = now + LABEL_TTL_MS;
		m_bHasLabel = true;

		m_sCachedName = WidgetManager.Translate("#OVT-ProdSite_BuyStock", GetStockCount().ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! \return The site's current stock of its own resource, 0 when unresolvable.
	protected int GetStockCount()
	{
		OVT_ResourceProductionComponent production = OVT_ComponentFinder<OVT_ResourceProductionComponent>.Find(GetOwner());
		if (!production)
			return 0;

		OVT_ResourceStoreComponent store = production.GetStore();
		if (!store)
			return 0;

		OVT_ResourceLedger ledger = store.GetLedger();
		if (!ledger)
			return 0;

		return ledger.Count(production.GetResourceId());
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
