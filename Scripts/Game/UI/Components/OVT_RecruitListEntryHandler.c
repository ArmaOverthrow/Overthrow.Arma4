//------------------------------------------------------------------------------------------------
class OVT_RecruitListEntryHandler : SCR_ButtonBaseComponent
{
	OVT_RecruitData m_RecruitData;
	int m_iIndex;
	
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
}