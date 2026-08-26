//------------------------------------------------------------------------------------------------
//! Marks an entity as an Overthrow-owned fuel source, and says whether its fuel is free.
//!
//! WHY A MARKER AND NOT A WHITELIST. Free fuel is identified STRUCTURALLY (implementation.md D5):
//! a station on a vehicle is free (fuel trucks), a station with no range is free (held jerrycans),
//! and everything else static is charged - which correctly covers every world pump on the map with
//! no world edits and no per-station configuration. The one thing that rule cannot see is a static
//! source the RESISTANCE built and already paid for: the Fuel Depot. This component is that missing
//! bit. It carries no logic and no state beyond the flag, because a prefab attribute is the only
//! thing the discrimination rule needs from it.
//!
//! IT IS ALSO THE DISCOVERY SURFACE. resistance/high-command asks "is there a fuel source here, and
//! what does a litre cost?" through OVT_FuelUtils; a component on the owning entity is what lets
//! that question be answered from an entity handle, without high-command knowing anything about
//! vanilla support stations.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "Overthrow", description: "Marks this entity as an Overthrow fuel source, and whether it dispenses free fuel")]
class OVT_FuelSourceComponentClass : ScriptComponentClass
{
}

class OVT_FuelSourceComponent : ScriptComponent
{
	[Attribute("1", desc: "Fuel from this source is free (it was already paid for). Turn off for a source that should charge the difficulty price")]
	protected bool m_bFree;

	//------------------------------------------------------------------------------------------------
	//! \return True when refuelling from this source costs nothing.
	bool IsFree()
	{
		return m_bFree;
	}
}
