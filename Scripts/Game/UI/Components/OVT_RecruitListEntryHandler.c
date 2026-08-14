//------------------------------------------------------------------------------------------------
class OVT_RecruitListEntryHandler : SCR_ButtonBaseComponent
{
	//! Where the status quads live. A named constant rather than a GUID buried in a call, so the one
	//! place that has to change when the atlas moves is here (OVT_MapShopPriceBands does the same).
	static const ResourceName STATUS_ICON_IMAGESET = "{C7691945DE01FB28}UI/Imagesets/overthrow_mapicons.imageset";

	//! The wounded/unconscious quad. Armed/ammo comes from OVT_RecruitStatus.TagIcon so the roster row
	//! and the map marker cannot disagree about what "armed" looks like.
	static const string ICON_WOUNDED = "recruit_wounded";

	//! How much a parked row is dimmed. Inactive is communicated by opacity PLUS section membership
	//! PLUS the status text - never by opacity alone, which is unreadable on a TV across a room.
	static const float INACTIVE_ROW_OPACITY = 0.55;

	protected IEntity m_RecruitEntity;
	protected string m_sRecruitName;
	int m_iIndex;
	protected OVT_RecruitData m_RecruitData;

	//------------------------------------------------------------------------------------------------
	//! Fill this row from a recruit record.
	//! \param[in] recruitData The record.
	//! \param[in] index Its position in the roster's FLAT display list (see OVT_RecruitEntryRef).
	//! \param[in] inactive Whether it is parked - drives the dimming and the status line.
	//! \param[in] statusFlags OVT_RecruitStatus mask from the client cache. 0 means "nothing reported",
	//!            which draws no icons at all rather than a wrong one.
	void Populate(OVT_RecruitData recruitData, int index, bool inactive = false, int statusFlags = 0)
	{
		if (!recruitData)
			return;

		m_RecruitData = recruitData;
		m_iIndex = index;

		// Set recruit name
		TextWidget nameWidget = TextWidget.Cast(m_wRoot.FindAnyWidget("RecruitName"));
		if (nameWidget)
		{
			nameWidget.SetText(recruitData.GetName());
			if (inactive)
				nameWidget.SetOpacity(INACTIVE_ROW_OPACITY);
			else
				nameWidget.SetOpacity(1.0);
		}

		// Set recruit level
		TextWidget levelWidget = TextWidget.Cast(m_wRoot.FindAnyWidget("RecruitLevel"));
		if (levelWidget)
		{
			levelWidget.SetTextFormat("#OVT-Recruit_Level", recruitData.GetLevel());
			if (inactive)
				levelWidget.SetOpacity(INACTIVE_ROW_OPACITY);
			else
				levelWidget.SetOpacity(1.0);
		}

		UpdateStatusIcons(statusFlags);

		// Set recruit status
		TextWidget statusWidget = TextWidget.Cast(m_wRoot.FindAnyWidget("RecruitStatus"));
		if (statusWidget)
		{
			IEntity recruitEntity;
			OVT_RecruitManagerComponent recruitManager = OVT_RecruitManagerComponent.GetInstance();
			if (recruitManager)
				recruitEntity = recruitManager.GetRecruitEntity(recruitData.m_sRecruitId);

			if (recruitEntity)
			{
				// Check if unconscious. A recruit body is not necessarily a ChimeraCharacter on a
				// client mid-stream, so neither the cast nor the controller may be assumed.
				CharacterControllerComponent controller;
				ChimeraCharacter character = ChimeraCharacter.Cast(recruitEntity);
				if (character)
					controller = character.GetCharacterController();

				if (controller && controller.GetLifeState() == ECharacterLifeState.INCAPACITATED)
				{
					statusWidget.SetText("#OVT-Recruit_Unconscious");
					statusWidget.SetColor(Color.Red);
				}else if (inactive){
					// TODO localize: OVT-Recruits_StatusHolding
					statusWidget.SetText("Holding");
					statusWidget.SetColor(Color.Orange);
				}else{
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

	//------------------------------------------------------------------------------------------------
	//! Draw the armed/ammo and wounded quads for a status mask.
	//!
	//! SetVisible -> LoadImageFromSet -> SetColor, in that order (the OVT_WantedInfo order): loading
	//! into a hidden widget leaves a frame of the authored quad on screen. The layout carries an
	//! authored Texture already, so a widget that this method never reaches is still not blank.
	//! \param[in] statusFlags OVT_RecruitStatus mask. 0 hides both icons.
	protected void UpdateStatusIcons(int statusFlags)
	{
		ImageWidget armedIcon = ImageWidget.Cast(m_wRoot.FindAnyWidget("ArmedIcon"));
		if (armedIcon)
		{
			string tag = OVT_RecruitStatus.TagIcon(statusFlags);
			if (tag.IsEmpty())
			{
				armedIcon.SetVisible(false);
			}
			else
			{
				armedIcon.SetVisible(true);
				armedIcon.LoadImageFromSet(0, STATUS_ICON_IMAGESET, tag);
				armedIcon.SetColor(Color.White);
			}
		}

		ImageWidget woundedIcon = ImageWidget.Cast(m_wRoot.FindAnyWidget("WoundedIcon"));
		if (woundedIcon)
		{
			// Unconscious is a stronger fact than wounded and shows the same icon - a player only needs
			// to know that this recruit needs help.
			if (OVT_RecruitStatus.IsWounded(statusFlags) || OVT_RecruitStatus.IsUnconscious(statusFlags))
			{
				woundedIcon.SetVisible(true);
				woundedIcon.LoadImageFromSet(0, STATUS_ICON_IMAGESET, ICON_WOUNDED);
				woundedIcon.SetColor(Color.Red);
			}
			else
			{
				woundedIcon.SetVisible(false);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return The recruit record this row was populated from, or null when it was built from a live
	//! entity instead (PopulateFromEntity).
	OVT_RecruitData GetRecruitData()
	{
		return m_RecruitData;
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