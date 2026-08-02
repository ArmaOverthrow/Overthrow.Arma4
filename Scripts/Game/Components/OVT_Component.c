class OVT_ComponentClass: ScriptComponentClass
{

}

//! Generic component finder
//! The project-wide way to fetch a component off an entity in one line, without repeating a Cast.
//! EnforceScript has no generic METHODS, so this is a generic CLASS with a static.
//! Usage: OVT_ComponentFinder<OVT_TownManagerComponent>.Find(entity)
class OVT_ComponentFinder<Class T>
{
	//! Find a component of type T on the given entity
	//! @param entity The entity to search for the component on
	//! @return The component if found, null otherwise
	static T Find(IEntity entity)
	{
		if (!entity)
			return null;

		return T.Cast(entity.FindComponent(T));
	}
}

class OVT_Component: ScriptComponent
{
	protected TimeAndWeatherManagerEntity m_Time;
	protected OVT_OverthrowConfigComponent m_Config;

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if(SCR_Global.IsEditMode())
			return;

		m_Config = OVT_Global.GetConfig();
		ChimeraWorld world = GetOwner().GetWorld();
		m_Time = world.GetTimeAndWeatherManager();
	}

	protected string GetGUID(ResourceName prefab)
	{
		int index = prefab.IndexOf("}");
		if (index == -1) return ResourceName.Empty;
		return prefab.Substring(1, index - 1);
	}

	RplComponent GetRpl()
	{
		return RplComponent.Cast(GetOwner().FindComponent(RplComponent));
	}
}
