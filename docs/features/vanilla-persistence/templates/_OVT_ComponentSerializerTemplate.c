//! Template for creating component serializers for Overthrow persistence
//! This is a REFERENCE FILE - copy and modify for each component that needs persistence
//!
//! USAGE:
//! 1. Copy this file and rename to match your component (e.g., OVT_MyComponentSerializer.c)
//! 2. Update class name and GetTargetType() to match your component
//! 3. Implement Serialize() to write component data
//! 4. Implement Deserialize() to read component data
//! 5. Add StartTracking() call in your component's OnPostInit()
//! 6. Register with appropriate PersistenceCollection

//! Example Component Serializer
//! Serializes data for a component attached to an entity (typically manager components)
class _OVT_ComponentSerializerTemplate : ScriptedComponentSerializer
{
	//! Specify which component type this serializer handles
	//! @return The typename of the component this serializer works with
	override static typename GetTargetType()
	{
		// Replace with your component type
		return OVT_YourComponentClass;
	}

	//! Serialize component data to save context
	//! @param owner The entity that owns this component
	//! @param component The component instance to serialize
	//! @param context Serialization context for writing data
	//! @return Serialization result (OK, DEFAULT, or DELETING)
	override protected ESerializeResult Serialize(
		IEntity owner,
		GenericComponent component,
		BaseSerializationSaveContext context)
	{
		// Cast to your component type
		OVT_YourComponentClass comp = OVT_YourComponentClass.Cast(component);
		if (!comp)
			return ESerializeResult.DEFAULT;

		// ALWAYS write version first for forward compatibility
		context.WriteValue("version", 1);

		// Example: Serialize primitives
		// context.WriteValue("m_iMyInt", comp.m_iMyInt);
		// context.WriteValue("m_fMyFloat", comp.m_fMyFloat);
		// context.WriteValue("m_bMyBool", comp.m_bMyBool);
		// context.WriteValue("m_sMyString", comp.m_sMyString);

		// Example: Serialize ref objects (vanilla handles automatically)
		// context.Write(comp.m_MyRefObject);

		// Example: Serialize arrays (vanilla handles automatically - no manual loops!)
		// context.Write(comp.m_aMyArray);

		// Example: Serialize maps (vanilla handles automatically)
		// context.Write(comp.m_mMyMap);

		// Example: Serialize array of ref objects
		// array<ref OVT_MyData> myData = comp.GetData();
		// context.Write(myData);

		// Example: Serialize map with ref values
		// map<string, ref OVT_MyData> myMap = comp.GetDataMap();
		// context.Write(myMap);

		// NOTE: Do NOT serialize entity references directly!
		// Use UUID pattern instead (see entity serializer template)

		return ESerializeResult.OK;
	}

	//! Deserialize component data from load context
	//! @param owner The entity that owns this component
	//! @param component The component instance to deserialize into
	//! @param context Serialization context for reading data
	//! @return True if successful, false otherwise
	override protected bool Deserialize(
		IEntity owner,
		GenericComponent component,
		BaseSerializationLoadContext context)
	{
		// Cast to your component type
		OVT_YourComponentClass comp = OVT_YourComponentClass.Cast(component);
		if (!comp)
			return false;

		// Read version for forward compatibility
		int version;
		if (!context.ReadValue("version", version))
			return false;

		// Handle version-specific deserialization
		if (version == 1)
		{
			// Example: Deserialize primitives
			// int myInt;
			// context.ReadValue("m_iMyInt", myInt);
			// comp.SetMyInt(myInt);

			// Example: Deserialize ref objects
			// OVT_MyRefObject myObj();
			// context.Read(myObj);
			// comp.SetMyObject(myObj);

			// Example: Deserialize arrays
			// array<int> myArray();
			// context.Read(myArray);
			// comp.SetMyArray(myArray);

			// Example: Deserialize maps
			// map<string, int> myMap();
			// context.Read(myMap);
			// comp.SetMyMap(myMap);

			// Example: Deserialize array of ref objects
			// array<ref OVT_MyData> myData();
			// context.Read(myData);
			// foreach (OVT_MyData data : myData)
			// {
			//     comp.AddData(data);
			// }
		}
		else
		{
			// Unknown version - log error
			Print(string.Format("OVT_YourComponentSerializer: Unknown version %1", version), LogLevel.ERROR);
			return false;
		}

		return true;
	}
}

//! COMPONENT SETUP
//! Add this to your component's OnPostInit() to enable persistence:
//!
//! override void OnPostInit(IEntity owner)
//! {
//!     super.OnPostInit(owner);
//!
//!     if (Replication.IsServer())
//!         PersistenceSystem.GetInstance().StartTracking(GetOwner());
//! }

//! COLLECTION REGISTRATION
//! Register your serializer with the appropriate collection in OVT_PersistenceManagerComponent:
//!
//! PersistenceCollection.GetOrCreate("YourCollectionName").SetSaveTypes(
//!     ESaveGameType.MANUAL | ESaveGameType.AUTO | ESaveGameType.SHUTDOWN);

//! COMMON PATTERNS
//!
//! 1. SIMPLE DATA (primitives, strings)
//!    context.WriteValue("key", value);  // Serialize
//!    context.ReadValue("key", value);   // Deserialize
//!
//! 2. REF OBJECTS
//!    context.Write(myRefObject);        // Serialize
//!    context.Read(myRefObject);         // Deserialize
//!
//! 3. ARRAYS
//!    context.Write(myArray);            // Serialize (automatic!)
//!    context.Read(myArray);             // Deserialize (automatic!)
//!
//! 4. MAPS
//!    context.Write(myMap);              // Serialize (automatic!)
//!    context.Read(myMap);               // Deserialize (automatic!)
//!
//! 5. VALIDATION
//!    Always validate data before saving:
//!    if (!data || data.IsEmpty()) return ESerializeResult.DEFAULT;
//!
//! 6. VERSIONING
//!    Always write/read version first for forward compatibility
//!    Support multiple versions in Deserialize() for backward compatibility

//! DEBUGGING
//! Use Print() statements to debug serialization:
//! Print(string.Format("Serializing %1 items", myArray.Count()), LogLevel.NORMAL);
//! Print(string.Format("Deserialized value: %1", myValue), LogLevel.NORMAL);

//! TESTING
//! Test your serializer:
//! 1. Modify component data in-game
//! 2. Trigger manual save
//! 3. Restart server
//! 4. Verify data restored correctly
//! 5. Check console for errors
