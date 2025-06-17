//! Save data for AI recruit system
[EPF_ComponentSaveDataType(OVT_RecruitManagerComponent), BaseContainerProps()]
class OVT_RecruitSaveDataClass : EPF_ComponentSaveDataClass
{
}

//! Handles persistence of recruit data
[EDF_DbName.Automatic()]
class OVT_RecruitSaveData : EPF_ComponentSaveData
{
	//! All recruits indexed by recruit ID
	ref map<string, ref OVT_RecruitData> m_mRecruits;
	
	//! Recruit IDs indexed by owner persistent ID
	ref map<string, ref array<string>> m_mRecruitsByOwner;
	
	//------------------------------------------------------------------------------------------------
	override EPF_EReadResult ReadFrom(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{
		Print("[Overthrow] RecruitSaveData.ReadFrom called");
		
		OVT_RecruitManagerComponent recruitManager = OVT_RecruitManagerComponent.Cast(component);
		if (!recruitManager)
		{
			Print("[Overthrow] Error saving recruits - component is null");
			return EPF_EReadResult.ERROR;
		}
		
		m_mRecruits = new map<string, ref OVT_RecruitData>;
		m_mRecruitsByOwner = new map<string, ref array<string>>;
		
		// Copy recruit data
		if (recruitManager.m_mRecruits)
		{
			for (int i = 0; i < recruitManager.m_mRecruits.Count(); i++)
			{
				string recruitId = recruitManager.m_mRecruits.GetKey(i);
				OVT_RecruitData recruit = recruitManager.m_mRecruits.GetElement(i);
				
				if (!recruitId || recruitId.IsEmpty())
				{
					Print("[Overthrow] WARNING: Skipping recruit with empty/null ID");
					continue;
				}
				
				if (!recruit)
				{
					Print("[Overthrow] WARNING: Skipping null recruit data for ID: " + recruitId);
					continue;
				}
				
				Print("[Overthrow] Saving recruit " + recruitId + " (" + recruit.m_sName + ")");
				m_mRecruits[recruitId] = recruit;
			}
		}
		
		// Copy ownership data
		if (recruitManager.m_mRecruitsByOwner)
		{
			for (int i = 0; i < recruitManager.m_mRecruitsByOwner.Count(); i++)
			{
				string ownerId = recruitManager.m_mRecruitsByOwner.GetKey(i);
				array<string> recruitIds = recruitManager.m_mRecruitsByOwner.GetElement(i);
				
				if (!ownerId || ownerId.IsEmpty())
				{
					Print("[Overthrow] WARNING: Skipping recruits with empty/null owner ID");
					continue;
				}
				
				if (!recruitIds || recruitIds.IsEmpty())
				{
					Print("[Overthrow] WARNING: Skipping empty recruit list for owner: " + ownerId);
					continue;
				}
				
				// Create a copy of the array
				array<string> recruitIdsCopy = new array<string>;
				foreach (string id : recruitIds)
				{
					recruitIdsCopy.Insert(id);
				}
				
				m_mRecruitsByOwner[ownerId] = recruitIdsCopy;
				Print("[Overthrow] Saved " + recruitIds.Count() + " recruits for owner: " + ownerId);
			}
		}
		
		Print("[Overthrow] Saved " + m_mRecruits.Count() + " total recruits");
		return EPF_EReadResult.OK;
	}
	
	//------------------------------------------------------------------------------------------------
	override EPF_EApplyResult ApplyTo(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{
		Print("[Overthrow] RecruitSaveData.ApplyTo called");
		
		OVT_RecruitManagerComponent recruitManager = OVT_RecruitManagerComponent.Cast(component);
		if (!recruitManager)
		{
			Print("[Overthrow] Error loading recruits - component is null");
			return EPF_EApplyResult.ERROR;
		}
		
		// Initialize collections if needed
		if (!recruitManager.m_mRecruits)
			recruitManager.m_mRecruits = new map<string, ref OVT_RecruitData>;
			
		if (!recruitManager.m_mRecruitsByOwner)
			recruitManager.m_mRecruitsByOwner = new map<string, ref array<string>>;
			
		if (!recruitManager.m_mEntityToRecruit)
			recruitManager.m_mEntityToRecruit = new map<EntityID, string>;
		
		// Load recruit data
		if (m_mRecruits)
		{
			for (int i = 0; i < m_mRecruits.Count(); i++)
			{
				string recruitId = m_mRecruits.GetKey(i);
				OVT_RecruitData recruit = m_mRecruits.GetElement(i);
				
				if (!recruitId || recruitId.IsEmpty())
				{
					Print("[Overthrow] WARNING: Skipping loading recruit with empty/null ID");
					continue;
				}
				
				if (!recruit)
				{
					Print("[Overthrow] WARNING: Skipping loading null recruit data for ID: " + recruitId);
					continue;
				}
				
				recruitManager.m_mRecruits[recruitId] = recruit;
				Print("[Overthrow] Loaded recruit " + recruitId + " (" + recruit.m_sName + ")");
			}
		}
		
		// Load ownership data
		if (m_mRecruitsByOwner)
		{
			for (int i = 0; i < m_mRecruitsByOwner.Count(); i++)
			{
				string ownerId = m_mRecruitsByOwner.GetKey(i);
				array<string> recruitIds = m_mRecruitsByOwner.GetElement(i);
				
				if (!ownerId || ownerId.IsEmpty())
				{
					Print("[Overthrow] WARNING: Skipping loading recruits with empty/null owner ID");
					continue;
				}
				
				if (!recruitIds || recruitIds.IsEmpty())
				{
					Print("[Overthrow] WARNING: Skipping loading empty recruit list for owner: " + ownerId);
					continue;
				}
				
				recruitManager.m_mRecruitsByOwner[ownerId] = recruitIds;
				Print("[Overthrow] Loaded " + recruitIds.Count() + " recruits for owner: " + ownerId);
			}
		}
		
		Print("[Overthrow] Loaded " + recruitManager.m_mRecruits.Count() + " total recruits");
		
		// TODO: Restore character entities from saved data
		// This will need to spawn characters at their last known positions
		// and re-establish the entity-to-recruit mappings
		
		return EPF_EApplyResult.OK;
	}
}