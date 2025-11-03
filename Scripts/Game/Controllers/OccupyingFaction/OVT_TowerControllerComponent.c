class OVT_TowerControllerComponentClass: OVT_ComponentClass
{
};

class OVT_TowerControllerComponent: OVT_Component
{
	//! Constructor
	void OVT_TowerControllerComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
	{
		// Registration happens in OnPostInit when everything is ready
	}

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		// Don't register in editor mode
		if(SCR_Global.IsEditMode()) return;

		// Register with the occupying faction manager
		OVT_OccupyingFactionManager manager = OVT_Global.GetOccupyingFaction();
		if(manager)
		{
			manager.RegisterTowerController(this);
		}
	}
}