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

	//------------------------------------------------------------------------------------------------
	//! Whether an owner is a throwaway ItemPreview / ResourceBrowser instance rather than a live
	//! entity in the running world.
	//!
	//! The test is "its world is not a ChimeraWorld", NOT "it has no world" - a preview instance DOES
	//! have a world, it is simply a different kind, so a plain null check passes and everything a
	//! component does for a real holder (ledgers, RplComponent assertions, registrations) then runs
	//! against an icon. Vanilla applies the same test for the same reason (game.c:1121).
	//! \param[in] owner The entity a component is initialising on.
	//! \return True when nothing in the running world should be touched.
	static bool IsPreviewInstance(IEntity owner)
	{
		return !owner || !ChimeraWorld.CastFrom(owner.GetWorld());
	}

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if(SCR_Global.IsEditMode())
			return;

		m_Config = OVT_Global.GetConfig();

		// CastFrom, not a plain assignment: an assignment from a preview instance's world leaves a
		// non-null handle of the wrong type, so the guard below passes and GetTimeAndWeatherManager -
		// which only ChimeraWorld declares - faults.
		ChimeraWorld world = ChimeraWorld.CastFrom(owner.GetWorld());
		if(!world)
			return;

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
