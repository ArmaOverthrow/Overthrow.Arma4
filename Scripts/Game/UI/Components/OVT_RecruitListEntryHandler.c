//------------------------------------------------------------------------------------------------
class OVT_RecruitListEntryHandler : SCR_ButtonBaseComponent
{
	protected IEntity m_RecruitEntity;
	protected string m_sRecruitName;
	int m_iIndex;
	protected OVT_RecruitData m_RecruitData;
	
	//! For backward compatibility with recruit management system
	void Populate(OVT_RecruitData recruitData, int index)
	{
		m_RecruitData = recruitData;
		m_iIndex = index;
		
		// Set recruit name
		TextWidget nameWidget = TextWidget.Cast(m_wRoot.FindAnyWidget("RecruitName"));
		if (nameWidget)
		{
			nameWidget.SetText(recruitData.GetName());
		}
		
		// Set recruit level
		TextWidget levelWidget = TextWidget.Cast(m_wRoot.FindAnyWidget("RecruitLevel"));
		if (levelWidget)
		{
			levelWidget.SetTextFormat("#OVT-Recruit_Level", recruitData.GetLevel());
		}
		
		// Set recruit status
		TextWidget statusWidget = TextWidget.Cast(m_wRoot.FindAnyWidget("RecruitStatus"));
		if (statusWidget)
		{
			OVT_RecruitManagerComponent recruitManager = OVT_RecruitManagerComponent.GetInstance();
			IEntity recruitEntity = recruitManager.GetRecruitEntity(recruitData.m_sRecruitId);
			
			if (recruitEntity)
			{
				// Check if alive
				SCR_CharacterDamageManagerComponent damageManager = SCR_CharacterDamageManagerComponent.Cast(
					recruitEntity.FindComponent(SCR_CharacterDamageManagerComponent)
				);
				
				if (damageManager && damageManager.GetState() == EDamageState.DESTROYED)
				{
					statusWidget.SetText("#OVT-Recruit_Dead");
					statusWidget.SetColor(Color.Red);
				}
				else
				{
					statusWidget.SetText("#OVT-Recruit_Active");
					statusWidget.SetColor(Color.Green);
				}
			}
			else
			{
				statusWidget.SetText("#OVT-Recruit_Offline");
				statusWidget.SetColor(Color.Gray);
			}
		}
	}
	
	//! For loadout system - populate with nearby AI character
	void PopulateFromEntity(IEntity recruitEntity, string recruitName, int index)
	{
		m_RecruitEntity = recruitEntity;
		m_sRecruitName = recruitName;
		m_iIndex = index;
		
		// Set recruit name
		TextWidget nameWidget = TextWidget.Cast(m_wRoot.FindAnyWidget("RecruitName"));
		if (nameWidget)
		{
			nameWidget.SetText(recruitName);
		}
		
		// Hide level widget if it exists (not relevant for nearby AI)
		TextWidget levelWidget = TextWidget.Cast(m_wRoot.FindAnyWidget("RecruitLevel"));
		if (levelWidget)
		{
			levelWidget.SetVisible(false);
		}
		
		// Set recruit status
		TextWidget statusWidget = TextWidget.Cast(m_wRoot.FindAnyWidget("RecruitStatus"));
		if (statusWidget)
		{
			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(recruitEntity);
			if (character)
			{
				CharacterControllerComponent charController = character.GetCharacterController();
				SCR_CharacterDamageManagerComponent damageManager = SCR_CharacterDamageManagerComponent.Cast(
					recruitEntity.FindComponent(SCR_CharacterDamageManagerComponent)
				);
				
				if (damageManager && damageManager.GetState() == EDamageState.DESTROYED)
				{
					statusWidget.SetText("#OVT-Loadouts_RecruitDead");
					statusWidget.SetColor(Color.Red);
				}
				else if (charController && charController.IsUnconscious())
				{
					statusWidget.SetText("#OVT-Loadouts_RecruitUnconscious");
					statusWidget.SetColor(Color.Yellow);
				}
				else
				{
					statusWidget.SetText("#OVT-Loadouts_RecruitReady");
					statusWidget.SetColor(Color.Green);
				}
			}
			else
			{
				statusWidget.SetText("#OVT-Loadouts_RecruitUnknown");
				statusWidget.SetColor(Color.Gray);
			}
		}
	}
	
	//! Get the recruit entity this entry represents
	IEntity GetRecruitEntity()
	{
		return m_RecruitEntity;
	}
	
	//! Get the recruit name
	string GetRecruitName()
	{
		return m_sRecruitName;
	}
	
	//! Get the index in the list
	int GetIndex()
	{
		return m_iIndex;
	}
}