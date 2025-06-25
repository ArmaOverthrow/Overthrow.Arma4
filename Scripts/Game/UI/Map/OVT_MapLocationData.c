//! Runtime data container for map location instances
class OVT_MapLocationData : Managed
{
	//! World position of the location
	vector m_vPosition;
	
	//! Display name of the location
	string m_sName;
	
	//! Location type class name
	string m_sTypeName;
	
	//! Optional entity ID (for buildings, vehicles, etc.)
	EntityID m_EntityID;
	
	//! Optional RplId (for networked objects)
	RplId m_RplID;
	
	//! Optional integer ID (for towns, bases, etc.)
	int m_iID = -1;
	
	//! Typed data storage for location-specific information
	ref map<string, string> m_mStringData;
	ref map<string, int> m_mIntData;
	ref map<string, float> m_mFloatData;
	ref map<string, bool> m_mBoolData;
	
	//! Whether this location is currently visible on the map
	bool m_bVisible = true;
	
	//! Whether this location supports fast travel
	bool m_bCanFastTravel = false;
	
	void OVT_MapLocationData(vector position, string name, string typeName)
	{
		m_vPosition = position;
		m_sName = name;
		m_sTypeName = typeName;
		m_mStringData = new map<string, string>;
		m_mIntData = new map<string, int>;
		m_mFloatData = new map<string, float>;
		m_mBoolData = new map<string, bool>;
	}
	
	//! Get data value as string
	string GetDataString(string key, string defaultValue = "")
	{
		string value;
		if (m_mStringData.Find(key, value))
			return value;
		return defaultValue;
	}
	
	//! Get data value as int
	int GetDataInt(string key, int defaultValue = 0)
	{
		int value;
		if (m_mIntData.Find(key, value))
			return value;
		return defaultValue;
	}
	
	//! Get data value as float
	float GetDataFloat(string key, float defaultValue = 0.0)
	{
		float value;
		if (m_mFloatData.Find(key, value))
			return value;
		return defaultValue;
	}
	
	//! Get data value as bool
	bool GetDataBool(string key, bool defaultValue = false)
	{
		bool value;
		if (m_mBoolData.Find(key, value))
			return value;
		return defaultValue;
	}
	
	//! Set string data
	void SetDataString(string key, string value)
	{
		m_mStringData.Set(key, value);
	}
	
	//! Set int data
	void SetDataInt(string key, int value)
	{
		m_mIntData.Set(key, value);
	}
	
	//! Set float data
	void SetDataFloat(string key, float value)
	{
		m_mFloatData.Set(key, value);
	}
	
	//! Set bool data
	void SetDataBool(string key, bool value)
	{
		m_mBoolData.Set(key, value);
	}
	
	//! Get the entity associated with this location (if any)
	IEntity GetEntity()
	{
		if (m_EntityID)
		{
			BaseWorld world = GetGame().GetWorld();
			if (world)
				return world.FindEntityByID(m_EntityID);
		}
		
		if (m_RplID)
		{
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(m_RplID));
			if (rpl)
				return rpl.GetEntity();
		}
		
		return null;
	}
	
	//! Calculate distance from player
	float GetDistanceFromPlayer()
	{
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		if (!player)
			return -1;
		
		return vector.Distance(m_vPosition, player.GetOrigin());
	}
}