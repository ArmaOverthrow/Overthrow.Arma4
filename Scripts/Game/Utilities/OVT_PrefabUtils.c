//------------------------------------------------------------------------------------------------
//! Stateless prefab/resource inspection helpers: resolving the prefab an entity came from, and reading
//! UI info out of a prefab resource without spawning it.
//!
//! Split out of OVT_Global (see OVT_WorldUtils). OVT_Global keeps a one-line GetPrefabName forwarder;
//! the UI-info readers are called on this class directly.
class OVT_PrefabUtils : Managed
{
	//------------------------------------------------------------------------------------------------
	//! Gets the prefab an entity was spawned from, resolving prefab inheritance the way the engine does.
	//!
	//! Walks the container ancestry to the first container that actually came from a .et file, which is
	//! what makes this correct for nested/inherited prefabs where EntityPrefabData.GetPrefabName() can
	//! answer with an intermediate name. This is the vanilla replacement for the prefab-name utility the
	//! old persistence framework provided, and is deliberately identical to it.
	//! \param[in] entity The entity to look up.
	//! \return Its prefab resource name, or an empty resource name.
	static ResourceName GetPrefabName(IEntity entity)
	{
		if (!entity)
			return ResourceName.Empty;

		EntityPrefabData prefabData = entity.GetPrefabData();
		if (!prefabData)
			return ResourceName.Empty;

		return SCR_BaseContainerTools.GetPrefabResourceName(prefabData.GetPrefab());
	}
	static SCR_EditableVehicleUIInfo GetVehicleUIInfo(ResourceName res)
	{
		Resource holder = BaseContainerTools.LoadContainer(res);
		if (holder)
		{
			IEntitySource ent = holder.GetResource().ToEntitySource();
			for(int t=0; t<ent.GetComponentCount(); t++)
			{
				IEntityComponentSource comp = ent.GetComponent(t);
				if(comp.GetClassName() == "SCR_EditableVehicleComponent")
				{
					SCR_EditableVehicleUIInfo info;
					comp.Get("m_UIInfo",info);
					return info;
				}
			}
		}
		return null;
	}
	
	static SCR_EditableEntityUIInfo GetEditableUIInfo(ResourceName res)
	{
		Resource holder = BaseContainerTools.LoadContainer(res);
		if (holder)
		{
			IEntitySource ent = holder.GetResource().ToEntitySource();
			for(int t=0; t<ent.GetComponentCount(); t++)
			{
				IEntityComponentSource comp = ent.GetComponent(t);
				if(comp.GetClassName() == "SCR_EditableVehicleComponent")
				{
					SCR_EditableEntityUIInfo info;
					comp.Get("m_UIInfo",info);
					return info;
				}
			}
		}
		return null;
	}
	
	static UIInfo GetItemUIInfo(ResourceName prefab)
	{
		IEntitySource entitySource = SCR_BaseContainerTools.FindEntitySource(Resource.Load(prefab));
		if (entitySource)
		{
		    for(int nComponent, componentCount = entitySource.GetComponentCount(); nComponent < componentCount; nComponent++)
		    {
		        IEntityComponentSource componentSource = entitySource.GetComponent(nComponent);
		        if(componentSource.GetClassName().ToType().IsInherited(InventoryItemComponent))
		        {
		            BaseContainer attributesContainer = componentSource.GetObject("Attributes");
		            if (attributesContainer)
		            {
		                BaseContainer itemDisplayNameContainer = attributesContainer.GetObject("ItemDisplayName");
		                if (itemDisplayNameContainer)
		                {
		                    UIInfo resultInfo = UIInfo.Cast(BaseContainerTools.CreateInstanceFromContainer(itemDisplayNameContainer));
		                    return resultInfo;
		                }
		            }
		        }
		    }
		}
		return null;
	}
}
