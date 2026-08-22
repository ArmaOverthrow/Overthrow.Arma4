//------------------------------------------------------------------------------------------------
//! "Build Guard Tower" / "Need 28 more Timber" - finishes a construction site out of the crate piles
//! around it.
//!
//! IT READS THE REPLICATED PILE CONTENTS, so the label and the enable state are right on every
//! machine with no round trip. The server re-derives every one of these numbers in
//! OVT_ResourceRequestComponent.RpcAsk_BuildFromSite() before it takes anything; this only stops a
//! request that is certain to be refused.
//!
//! Sort Priority 2, under the requirements readout.
//!
//! The label is rebuilt on a one second TTL - it is asked for every frame a player looks at the site
//! (OVT_OpenResourceStoreAction's shape).
//------------------------------------------------------------------------------------------------
class OVT_BuildFromSiteAction : ScriptedUserAction
{
	//! How long the label is reused, in milliseconds of world time.
	protected const float LABEL_TTL_MS = 1000;

	//! World time (ms) at which the cached label goes stale, and whether there is one.
	protected float m_fLabelExpiresAt;
	protected bool m_bHasLabel;

	//! The label built at the last rebuild, and whether that rebuild found the site satisfied.
	protected string m_sCachedName;
	protected bool m_bSatisfied;

	//------------------------------------------------------------------------------------------------
	//! Asks the server to consume the piles and finish the site.
	//! \param[in] pOwnerEntity The construction site.
	//! \param[in] pUserEntity The acting character.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		RplId siteId = OVT_ResourceUtils.GetHolderId(pOwnerEntity);
		if (!siteId.IsValid())
			return;

		OVT_ResourceRequestComponent requests = OVT_ControllerComponent<OVT_ResourceRequestComponent>.Get();
		if (!requests)
			return;

		requests.RequestBuildFromSite(siteId);
	}

	//------------------------------------------------------------------------------------------------
	//! Hidden on a ruin and on anything that is not a site; a site that is merely short is VISIBLE
	//! with a reason, because a missing button is indistinguishable from a bug.
	//! \param[in] user The character looking at the action.
	//! \return True when the action may be drawn.
	override bool CanBeShownScript(IEntity user)
	{
		if (!OVT_StructureDamage.IsUsable(GetOwner()))
			return false;

		return OVT_SiteRequirementsReader.GetSite(GetOwner()) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] user The character looking at the action.
	//! \return True only when the nearby piles cover every requirement.
	override bool CanBePerformedScript(IEntity user)
	{
		RefreshLabel();

		if (!m_bSatisfied)
		{
			SetCannotPerformReason(m_sCachedName);
			return false;
		}

		if (!OVT_ResourceUtils.GetHolderId(GetOwner()).IsValid())
		{
			SetCannotPerformReason("#OVT-Resource_NoSite");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! "Build X" when the piles cover it, otherwise the first shortfall by name.
	//! \param[out] outName The label to draw.
	//! \return Always true; this action always names itself.
	override bool GetActionNameScript(out string outName)
	{
		RefreshLabel();

		outName = m_sCachedName;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The work goes over the request component from the clicking machine.
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Rebuilds the label when it has gone stale. Reading the replicated ledgers is cheap; sweeping
	//! the piles and building a string every frame for every site a player can see is not.
	protected void RefreshLabel()
	{
		float now = GetWorldTimeMs();
		if (m_bHasLabel && now < m_fLabelExpiresAt)
			return;

		m_fLabelExpiresAt = now + LABEL_TTL_MS;
		m_bHasLabel = true;
		m_bSatisfied = false;

		array<ref OVT_ResourceAmount> need = new array<ref OVT_ResourceAmount>();
		array<ref OVT_ResourceAmount> have = new array<ref OVT_ResourceAmount>();
		if (!OVT_SiteRequirementsReader.Read(GetOwner(), need, have))
		{
			m_sCachedName = "#OVT-Resource_NoSite";
			return;
		}

		string shortId;
		if (OVT_ResourceRules.IsSatisfied(need, have, shortId))
		{
			m_bSatisfied = true;
			m_sCachedName = WidgetManager.Translate("#OVT-Resource_BuildNow", ResolveBuildingName());
			return;
		}

		int missing = OVT_ResourceRules.AmountOf(need, shortId) - OVT_ResourceRules.AmountOf(have, shortId);
		m_sCachedName = WidgetManager.Translate("#OVT-Resource_ShortOf", missing.ToString(), OVT_ResourceUtils.ResolveResourceTitle(shortId));
	}

	//------------------------------------------------------------------------------------------------
	//! \return The building's replicated name, translated, or a generic one.
	protected string ResolveBuildingName()
	{
		OVT_ConstructionSiteComponent site = OVT_SiteRequirementsReader.GetSite(GetOwner());
		if (!site)
			return WidgetManager.Translate("#OVT-Resource_ConstructionSite");

		string name = site.GetBuildableName();
		if (name == "")
			return WidgetManager.Translate("#OVT-Resource_ConstructionSite");

		return WidgetManager.Translate(name);
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
