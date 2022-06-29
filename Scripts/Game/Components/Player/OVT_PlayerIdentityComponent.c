class OVT_PlayerIdentityComponentClass: OVT_ComponentClass
{
};

class OVT_PlayerIdentityComponent: OVT_Component
{		
	SCR_CharacterControllerComponent m_Controller;
	protected string m_sPersistentID = "";
	
	protected const string PERSISTENT_ID_FILE_PATH = "$profile:overthrowPersistentID.txt";
	
	
	string GetPersistentID()
	{
		return m_sPersistentID;
	}
	
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);		
		
		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast( owner.FindComponent(SCR_CharacterControllerComponent) );		
				
		if(controller){
			m_Controller = controller;			
			m_Controller.m_OnControlledByPlayer.Insert(this.OnControlledByPlayer);
		}
	}
	
	protected void OnControlledByPlayer(IEntity owner, bool controlled)
	{	
		if(m_sPersistentID != "") return;	
		if (controlled)
		{			
			//Check for a saved persistent player ID			
			if(FileIO.FileExist(PERSISTENT_ID_FILE_PATH))
			{
				//File exists, use it
				FileHandle f = FileIO.OpenFile(PERSISTENT_ID_FILE_PATH, FileMode.READ);
				if(f){
					f.FGets(m_sPersistentID);
					f.CloseFile();
				}
			}else{
				//File doesnt exist, generate one
				m_sPersistentID = GenerateID(); //To-Do: Generate a random string
				FileHandle f = FileIO.OpenFile(PERSISTENT_ID_FILE_PATH, FileMode.WRITE);
				f.FPrint(m_sPersistentID);
				f.CloseFile();
			}
			
			int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(GetOwner());
			OVT_Global.GetPlayers().RegisterPlayer(playerId, m_sPersistentID);
			
			RplComponent rplComponent = RplComponent.Cast(owner.FindComponent(RplComponent));
			if (!rplComponent)
				return;
			
			if(Replication.IsServer()) return;
			OVT_Global.GetServer().RegisterPersistentID(m_sPersistentID);			
		}
	}
	
	protected string GenerateID()
	{
			
		int year = 0;
		int month = 0;
		int day = 0;	
		System.GetYearMonthDayUTC(year, month, day);
	
		int hour = 0;
		int minute = 0;
		int second = 0;			
		System.GetHourMinuteSecondUTC(hour, minute, second);
		
		string s = ""+year+month+day+hour+minute+second;
	
		//Add some random numbers (Reference: https://eager.io/blog/how-long-does-an-id-need-to-be/)
		int i = s_AIRandomGenerator.RandFloatXY(0, 16777215);
		s += i.ToString();
	
		i = s_AIRandomGenerator.RandFloatXY(0, 16777215);
		s += i.ToString();
		
		return s;
	}
	
	void ~OVT_PlayerIdentityComponent()
	{		
		if(!m_Controller) return;
		
		m_Controller.m_OnControlledByPlayer.Remove(this.OnControlledByPlayer);
	}
}