class OVT_ComponentClass: ScriptComponentClass
{
	
}

class OVT_Component: ScriptComponent
{
	protected OVT_OverthrowConfigComponent m_Config;
	protected TimeAndWeatherManagerEntity m_Time;
	
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		m_Config = OVT_OverthrowConfigComponent.GetInstance();
		m_Time = GetGame().GetTimeAndWeatherManager();
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

	protected void RPL_WritePlayerID(ScriptBitWriter writer, string id)
	{
		writer.Write((id.Substring(0,9)).ToInt(), 32);
		writer.Write((id.Substring(9,9)).ToInt(), 32);
		writer.Write((id.Substring(18,id.Length() - 18)).ToInt(), 32);
	}

	protected bool RPL_ReadPlayerID(ScriptBitReader reader, out string id)
	{
		id = "";
		int i;
		if(!reader.Read(i, 32)) return false;
		id += i.ToString();
			
		if(!reader.Read(i, 32)) return false;
		id += i.ToString();
	
		if(!reader.Read(i, 32)) return false;
		id += i.ToString();
	
		return true;
	}
	
	protected void RPL_WriteString(ScriptBitWriter writer, string s)
	{
		writer.Write(s.Length(), 32);
		writer.Write(s, 8 * s.Length());
	}
	
	protected bool RPL_ReadString(ScriptBitReader reader, out string s)
	{
		int length;
		s = "";
		if (!reader.Read(length, 32)) return false;		
		if (!reader.Read(s, length)) return false;	
		
		return true;
	}
}