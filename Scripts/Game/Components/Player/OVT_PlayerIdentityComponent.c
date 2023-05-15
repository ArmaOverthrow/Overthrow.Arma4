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
			int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(GetOwner());
			
			m_sPersistentID = OVT_Global.GetPlayerUID(playerId);	
						
			OVT_Global.GetPlayers().RegisterPlayer(playerId, m_sPersistentID);
			
			if(Replication.IsServer()) return;
			OVT_Global.GetServer().RegisterPersistentID(m_sPersistentID);			
		}
	}
	
	void ~OVT_PlayerIdentityComponent()
	{		
		if(!m_Controller) return;
		
		m_Controller.m_OnControlledByPlayer.Remove(this.OnControlledByPlayer);
	}
}