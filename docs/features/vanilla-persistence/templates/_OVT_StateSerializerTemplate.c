//! Template for creating state serializers for Overthrow persistence
//! This is a REFERENCE FILE - copy and modify for each state that needs persistence
//!
//! USAGE:
//! 1. Create a proxy state class extending PersistentState (see States/ directory)
//! 2. Copy this file and rename to match your state (e.g., OVT_MyStateSerializer.c)
//! 3. Update class name and GetTargetType() to match your proxy state
//! 4. Implement Serialize() to save state data
//! 5. Implement Deserialize() to load state data
//! 6. Instantiate and track proxy state in your manager/system

//! Example State Serializer
//! Serializes state data that doesn't belong to a component or entity
//! Uses proxy PersistentState class for tracking
class _OVT_StateSerializerTemplate : ScriptedStateSerializer
{
	//! Specify which proxy state type this serializer handles
	//! @return The typename of the proxy state class
	override static typename GetTargetType()
	{
		// Replace with your proxy state class
		return OVT_MyStateData;
	}

	//! Serialize state data
	//! @param instance The proxy state instance (cast to your type)
	//! @param context Serialization context for writing data
	//! @return Serialization result
	override protected ESerializeResult Serialize(
		Managed instance,
		BaseSerializationSaveContext context)
	{
		// Get the actual manager/system that owns the data
		// State serializers typically access global managers via OVT_Global
		OVT_MyManagerComponent manager = OVT_Global.GetMyManager();
		if (!manager)
			return ESerializeResult.DEFAULT;

		// ALWAYS write version first
		context.WriteValue("version", 1);

		// Example: Serialize manager state
		// context.WriteValue("m_iCachedCount", manager.GetCachedCount());
		// context.Write(manager.GetDataMap());
		// context.Write(manager.GetDataArray());

		// Example: Serialize complex nested data
		// map<string, ref OVT_MyData> dataMap = manager.GetAllData();
		// context.Write(dataMap);

		return ESerializeResult.OK;
	}

	//! Deserialize state data
	//! @param instance The proxy state instance (cast to your type)
	//! @param context Serialization context for reading data
	//! @return True if successful
	override protected bool Deserialize(
		Managed instance,
		BaseSerializationLoadContext context)
	{
		// Get the actual manager/system that owns the data
		OVT_MyManagerComponent manager = OVT_Global.GetMyManager();
		if (!manager)
			return false;

		// Read version
		int version;
		if (!context.ReadValue("version", version))
			return false;

		if (version == 1)
		{
			// Example: Deserialize manager state
			// int cachedCount;
			// context.ReadValue("m_iCachedCount", cachedCount);
			// manager.SetCachedCount(cachedCount);

			// map<string, ref OVT_MyData> dataMap();
			// context.Read(dataMap);
			// manager.RestoreDataMap(dataMap);

			// array<ref OVT_MyData> dataArray();
			// context.Read(dataArray);
			// foreach (OVT_MyData data : dataArray)
			// {
			//     manager.AddData(data);
			// }
		}
		else
		{
			Print(string.Format("OVT_MyStateSerializer: Unknown version %1", version), LogLevel.ERROR);
			return false;
		}

		return true;
	}
}

//! PROXY STATE CLASS
//! Create a proxy state class to track non-component/entity data:
//!
//! File: Scripts/Game/Persistence/States/OVT_MyStateData.c
//!
//! //! Proxy state for tracking MyManager data
//! class OVT_MyStateData : PersistentState
//! {
//!     // This class is typically empty - it's just a tracking handle
//!     // The actual data lives in your manager component
//! }

//! STATE TRACKING
//! Instantiate and track the proxy state in your manager:
//!
//! class OVT_MyManagerComponent : OVT_Component
//! {
//!     protected OVT_MyStateData m_ProxyState;
//!
//!     override void OnPostInit(IEntity owner)
//!     {
//!         super.OnPostInit(owner);
//!
//!         if (Replication.IsServer())
//!         {
//!             // Create and track proxy state
//!             m_ProxyState = new OVT_MyStateData();
//!             PersistenceSystem.GetInstance().StartTracking(m_ProxyState);
//!         }
//!     }
//! }

//! COLLECTION REGISTRATION
//! Register your serializer with the appropriate collection in OVT_PersistenceManagerComponent:
//!
//! PersistenceCollection.GetOrCreate("YourStateCollection").SetSaveTypes(
//!     ESaveGameType.MANUAL | ESaveGameType.AUTO | ESaveGameType.SHUTDOWN);

//! WHEN TO USE STATE SERIALIZERS
//!
//! Use ScriptedStateSerializer when:
//! - Data doesn't belong to a specific component or entity
//! - You need to persist global/manager-level state
//! - Data is shared across multiple systems
//! - You want to separate persistence concerns from component logic
//!
//! Examples:
//! - Loadout manager state (loadout templates, player loadouts)
//! - Global game state (time, weather settings)
//! - Campaign progress tracking
//! - Server-wide configuration

//! STATE vs COMPONENT SERIALIZERS
//!
//! ScriptedComponentSerializer:
//! - Attached to an entity via component
//! - Entity is tracked via PersistenceSystem.StartTracking(entity)
//! - Data belongs to that specific component instance
//! - Example: OVT_TownManagerComponent, OVT_PlayerManagerComponent
//!
//! ScriptedStateSerializer:
//! - Not attached to any entity
//! - Proxy state object is tracked via PersistenceSystem.StartTracking(state)
//! - Data accessed via global managers (OVT_Global)
//! - Example: OVT_LoadoutManagerData, global campaign state
//!
//! Rule of thumb:
//! - If it's a manager component on game mode → use component serializer
//! - If it's global state without a component → use state serializer

//! COMMON PATTERNS
//!
//! 1. ACCESSING MANAGER
//!    OVT_MyManager mgr = OVT_Global.GetMyManager();
//!    if (!mgr) return false;
//!
//! 2. SERIALIZING MANAGER DATA
//!    context.Write(mgr.GetAllData());
//!
//! 3. DESERIALIZING MANAGER DATA
//!    map<string, ref OVT_MyData> data();
//!    context.Read(data);
//!    mgr.RestoreData(data);
//!
//! 4. VALIDATION
//!    if (!mgr || !data) return ESerializeResult.DEFAULT;
//!
//! 5. VERSIONING
//!    Always write/read version first for forward compatibility

//! DEBUGGING
//! Use Print() statements to debug:
//! Print(string.Format("Serializing %1 state entries", count), LogLevel.NORMAL);
//! Print(string.Format("Restored state data: %1", data), LogLevel.NORMAL);

//! TESTING
//! Test your state serializer:
//! 1. Modify manager state in-game
//! 2. Trigger manual save
//! 3. Restart server
//! 4. Verify state restored correctly via manager
//! 5. Check console for errors
//! 6. Verify proxy state is being tracked (debug prints)
