//------------------------------------------------------------------------------------------------
//! "Cargo (12.5 / 20 m3)" / "Crate pile (4.4 m3)" - opens the resource transfer screen on this holder.
//!
//! ONE CLASS, FOUR HOSTS: the truck delta, the crate pile, the warehouse delta and the production
//! site. Nothing in it is host-specific; the label branches on whether the store has a cap, which is
//! the only thing that differs between a truck and an unlimited holder.
//!
//! The volume comes from the holder's REPLICATED ledger, so it is right on every client within one
//! replication tick and changes with nobody's menu open. It is rebuilt on a one second TTL - the
//! label is asked for every frame a player looks at the holder (OVT_FillFuelAction's shape).
//!
//! GATE POLICY (OVT_FillFuelAction :35-39): HIDDEN only when the action is irrelevant - a ruin, or a
//! holder with no store. Every other refusal is VISIBLE WITH A REASON, because a missing button is
//! indistinguishable from a bug.
//------------------------------------------------------------------------------------------------
class OVT_OpenResourceStoreAction : ScriptedUserAction
{
	//! How long the label is reused, in milliseconds of world time.
	protected const float LABEL_TTL_MS = 1000;

	//! World time (ms) at which the cached label goes stale, and whether there is one.
	protected float m_fLabelExpiresAt;
	protected bool m_bHasLabel;

	//! The label built at the last rebuild.
	protected string m_sCachedName;

	//------------------------------------------------------------------------------------------------
	//! Opens the transfer screen on this holder, on the clicking machine.
	//! \param[in] pOwnerEntity The holder.
	//! \param[in] pUserEntity The acting character.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pUserEntity)
			return;

		OVT_UIManagerComponent uimanager = OVT_UIManagerComponent.Cast(pUserEntity.FindComponent(OVT_UIManagerComponent));
		if (!uimanager)
			return;

		OVT_ResourceTransferContext context = OVT_ResourceTransferContext.Cast(uimanager.GetContext(OVT_ResourceTransferContext));
		if (!context)
			return;

		context.SetHolder(OVT_ResourceUtils.ResolveStoreHolder(pOwnerEntity));

		uimanager.ShowContext(OVT_ResourceTransferContext);
	}

	//------------------------------------------------------------------------------------------------
	//! Hidden on a ruin and on anything with no usable store; everything else is refused with a reason.
	//! \param[in] user The character looking at the action.
	//! \return True when the action may be drawn.
	override bool CanBeShownScript(IEntity user)
	{
		if (!OVT_StructureDamage.IsUsable(Holder()))
			return false;

		return OVT_ResourceUtils.IsUsableHolder(GetStore());
	}

	//------------------------------------------------------------------------------------------------
	//! A client mirror of the server gate's locked and warehouse steps. The server re-checks all of
	//! it, including the distance; this only stops a request that is certain to be refused.
	//! \param[in] user The character looking at the action.
	//! \return True when the request is worth sending.
	override bool CanBePerformedScript(IEntity user)
	{
		if (!user)
			return false;

		if (!GetHolderId().IsValid())
		{
			SetCannotPerformReason("#OVT-Resource_NoStore");
			return false;
		}

		OVT_PlayerOwnerComponent playerOwner = OVT_ComponentFinder<OVT_PlayerOwnerComponent>.Find(Holder());
		if (playerOwner && playerOwner.IsLocked())
		{
			string ownerUid = playerOwner.GetPlayerOwnerUid();
			string playerUid = OVT_Global.GetPlayers().GetPersistentIDFromControlledEntity(user);

			if (ownerUid != "" && ownerUid != playerUid)
			{
				SetCannotPerformReason("#OVT-Resource_Locked");
				return false;
			}
		}

		if (!WarehouseIsOpenTo(user))
		{
			SetCannotPerformReason("#OVT-Resource_NoAccess");
			return false;
		}

		if (!SiteIsOpenTo(user))
		{
			SetCannotPerformReason("#OVT-Resource_NoAccess");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! "Cargo (used / cap m3)" for a capped holder, "<name> (used m3)" for an unlimited one.
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
	//! Rebuilds the label when it has gone stale. Reading the ledger is cheap, but building a string
	//! every frame for every holder a player can see is not.
	protected void RefreshLabel()
	{
		float now = GetWorldTimeMs();
		if (m_bHasLabel && now < m_fLabelExpiresAt)
			return;

		m_fLabelExpiresAt = now + LABEL_TTL_MS;
		m_bHasLabel = true;

		OVT_ResourceStoreComponent store = GetStore();
		if (!store)
		{
			m_sCachedName = "#OVT-Resource_OpenStore";
			return;
		}

		int capacity = store.GetCapacityLitres();
		string used = FormatCubicMetres(store.GetUsedLitres());

		if (capacity == OVT_ResourceStoreComponent.UNLIMITED_CAPACITY)
		{
			m_sCachedName = store.GetDisplayName() + " (" + used + " m³)";
			return;
		}

		m_sCachedName = "#OVT-Resource_Cargo (" + used + " / " + FormatCubicMetres(capacity) + " m³)";
	}

	//------------------------------------------------------------------------------------------------
	//! \return The holder's store, or null.
	protected OVT_ResourceStoreComponent GetStore()
	{
		return OVT_ResourceUtils.GetStore(Holder());
	}

	//------------------------------------------------------------------------------------------------
	//! \return The holder's networked name, or RplId.Invalid().
	protected RplId GetHolderId()
	{
		return OVT_ResourceUtils.GetHolderId(Holder());
	}

	//------------------------------------------------------------------------------------------------
	//! The warehouse half of the client mirror; anything that is not a warehouse building passes.
	//! \param[in] user The acting character.
	//! \return True when this holder is not a warehouse, or is one this player may open.
	protected bool WarehouseIsOpenTo(IEntity user)
	{
		OVT_RealEstateManagerComponent realEstate = OVT_Global.GetRealEstate();
		if (!realEstate)
			return true;

		string playerUid = OVT_Global.GetPlayers().GetPersistentIDFromControlledEntity(user);

		return realEstate.PlayerMayUseWarehouse(playerUid, Holder());
	}

	//------------------------------------------------------------------------------------------------
	//! The site half of the client mirror; anything that is not a production site passes. Calls the
	//! SAME predicate the server's gate does (MayAccessStore) - no ownership comparison is ever
	//! re-implemented here.
	//! \param[in] user The acting character.
	//! \return True when this holder is not a site, or is one this player may open.
	protected bool SiteIsOpenTo(IEntity user)
	{
		if (!OVT_ComponentFinder<OVT_ResourceProductionComponent>.Find(Holder()))
			return true;

		OVT_ResourceProductionManagerComponent production = OVT_Global.GetProduction();
		if (!production)
			return false;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players)
			return false;

		string playerUid = players.GetPersistentIDFromControlledEntity(user);

		return OVT_ResourceProductionRules.MayAccessStore(playerUid, production.GetSiteOwner(Holder()), production.IsSitePrivate(Holder()));
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] litres A volume in integer litres.
	//! \return The same volume in m3, one decimal.
	protected string FormatCubicMetres(int litres)
	{
		return OVT_ResourceUtils.FormatCubicMetres(litres);
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

	//------------------------------------------------------------------------------------------------
	//! \return The entity that actually carries the store - this action is also hosted on truck cargo
	//! beds, whose store lives one hop up on the vehicle root.
	protected IEntity Holder()
	{
		return OVT_ResourceUtils.ResolveStoreHolder(GetOwner());
	}
}
