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
	
	//Called at game start for each town
	void OnStart(OVT_TownData town)
	{
	
	}
	
	//Called every so often
	void OnTick(OVT_TownData town)
	{
	
	}
	
	//Called every so often when town has this modifier, return false to remove modifier
	bool OnActiveTick(OVT_TownData town)
	{
		return true;
	}
	
	//Cleanup yourself here
	void OnDestroy()
	{
		
	}
}