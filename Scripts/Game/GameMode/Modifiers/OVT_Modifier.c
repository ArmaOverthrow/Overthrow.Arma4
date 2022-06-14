[BaseContainerProps()]
class OVT_Modifier : ScriptAndConfig
{
	string m_sName;
	int m_iIndex;
	
	protected OVT_TownManagerComponent m_Towns;
	
	void Init()
	{
		m_Towns = OVT_Global.GetTowns();
	}
	
	//Called after game init
	void OnPostInit()
	{
		
	}
	
	//Called every so often, return false to remove modifier (or one of the stack)
	bool OnTick()
	{
	
	}
	
	//Cleanup yourself here
	void OnDestroy()
	{
		
	}
}