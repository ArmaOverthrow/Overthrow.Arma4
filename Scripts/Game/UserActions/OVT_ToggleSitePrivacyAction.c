//------------------------------------------------------------------------------------------------
//! "Make public" / "Make private" on an owned production site.
//!
//! SHOWN ONLY TO SOMEONE WHO MAY USE IT. OVT_ResourceProductionRules.MayTogglePrivacy is the single
//! predicate on both sides: unowned sites have nothing to toggle, a resistance-owned site answers to
//! officers, and a player-owned one answers to its owner alone. The comparison is never re-implemented
//! here - a client gate that disagrees with the server gate is the bug this design exists to remove.
//!
//! The manager accessor read below is PRESENTATION ONLY: which of the two labels to draw. The SERVER
//! decides which way the flag flips, from its own record - a client one broadcast stale would
//! otherwise re-send the state the site already holds and see nothing happen.
//------------------------------------------------------------------------------------------------
class OVT_ToggleSitePrivacyAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	//! Ask the server to flip this site's privacy.
	//! \param[in] pOwnerEntity The site entity.
	//! \param[in] pUserEntity The acting character.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		OVT_ResourceProductionManagerComponent manager = OVT_Global.GetProduction();
		if (!manager)
			return;

		OVT_ProductionSiteData rec = manager.GetSiteForEntity(GetOwner());
		if (!rec)
			return;

		OVT_ResourceProductionRequestComponent requests = OVT_ControllerComponent<OVT_ResourceProductionRequestComponent>.Get();
		if (!requests)
			return;

		requests.SetSitePrivacy(rec.location, !manager.IsSitePrivate(GetOwner()));
	}

	//------------------------------------------------------------------------------------------------
	//! Shown to the owner of an owned site, and to officers on a resistance-owned one. Nobody else.
	//! \param[in] user The character looking at the action.
	//! \return True when this player may flip the flag.
	override bool CanBeShownScript(IEntity user)
	{
		OVT_ResourceProductionManagerComponent manager = OVT_Global.GetProduction();
		if (!manager)
			return false;

		OVT_ProductionSiteData rec = manager.GetSiteForEntity(GetOwner());
		if (!rec)
			return false;

		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (!resistance)
			return false;

		return OVT_ResourceProductionRules.MayTogglePrivacy(OVT_Global.GetLocalPersistentId(), rec.owner, resistance.IsLocalPlayerOfficer());
	}

	//------------------------------------------------------------------------------------------------
	//! "Make public" while the site is closed, "Make private" while it is open.
	//! \param[out] outName The label to draw.
	//! \return Always true; this action always names itself.
	override bool GetActionNameScript(out string outName)
	{
		OVT_ResourceProductionManagerComponent manager = OVT_Global.GetProduction();
		if (!manager)
		{
			outName = "#OVT-ProdSite_MakePublic";
			return true;
		}

		if (manager.IsSitePrivate(GetOwner()))
		{
			outName = "#OVT-ProdSite_MakePublic";
			return true;
		}

		outName = "#OVT-ProdSite_MakePrivate";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The flip goes over the request component from the clicking machine.
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}
}
