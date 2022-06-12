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