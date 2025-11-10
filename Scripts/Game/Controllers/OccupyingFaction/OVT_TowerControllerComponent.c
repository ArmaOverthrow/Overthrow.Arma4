class OVT_TowerControllerComponentClass: OVT_ComponentClass
{
};

class OVT_TowerControllerComponent: OVT_Component
{
	protected bool m_bRegistered = false;

	//! Constructor
	void OVT_TowerControllerComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
	{
		// Don't register in editor mode
		if(SCR_Global.IsEditMode()) return;

		// Register with the manager (updates entity IDs, doesn't set faction)
		DeferredRegister();
	}

	//! Deferred registration - retries until manager exists
	protected void DeferredRegister()
	{
		if(m_bRegistered) return;

		OVT_OccupyingFactionManager manager = OVT_Global.GetOccupyingFaction();
		if(manager)
		{
			manager.RegisterTowerController(this);
			m_bRegistered = true;
			Print(string.Format("[Overthrow] Tower controller registered at %1", GetOwner().GetOrigin().ToString()), LogLevel.NORMAL);
		}
		else
		{
			// Manager not available yet - retry after 50ms
			GetGame().GetCallqueue().CallLater(DeferredRegister, 50, false);
		}
	}
}