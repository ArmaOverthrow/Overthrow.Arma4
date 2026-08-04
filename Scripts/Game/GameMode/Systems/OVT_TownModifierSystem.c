class OVT_TownModifierSystem : ScriptAndConfig
{
	[Attribute()]
	ResourceName m_rConfigFile;
	
	ref OVT_ModifiersConfig m_Config;
	protected ref array<ref OVT_TownData> m_Towns;
	protected OVT_TownManagerComponent m_TownManager;
		
	void Init()
	{
		m_TownManager = OVT_Global.GetTowns();
		m_Towns = m_TownManager.m_Towns;
		LoadConfig();
	}
	
	protected void LoadConfig()
	{
		Resource holder = BaseContainerTools.LoadContainer(m_rConfigFile);
		if (holder)		
		{
			OVT_ModifiersConfig obj = OVT_ModifiersConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(holder.GetResource().ToBaseContainer()));
			if(obj)
			{
				m_Config = obj;
				PostInit();
			}
		}
	}
	
	protected void PostInit()
	{
		if(!Replication.IsServer()) return;
		foreach(int i, OVT_ModifierConfig config : m_Config.m_aModifiers)
		{
			if(config.handler)
			{
				config.handler.m_sName = config.name;
				config.handler.m_iIndex = i;
				config.handler.Init();
				config.handler.OnPostInit();
				
				foreach(OVT_TownData town : m_Towns)
				{
					config.handler.OnStart(town);
				}
			}
		}
	}
	
	bool OnTick(inout array<ref OVT_TownModifierData> modifiers, OVT_TownData town)
	{
		//Check if we need to time out any modifiers
		array<ref OVT_TownModifierData> rebuild = new array<ref OVT_TownModifierData>;
		bool recalc = false;		
		int townID = m_TownManager.GetTownID(town);
		foreach(OVT_TownModifierData modifier : modifiers)
		{
			if(!modifier) continue;
			OVT_ModifierConfig mod = m_Config.m_aModifiers[modifier.id];
			if(mod.timeout <= 0)
			{
				//Permanent modifiers never expire, but must survive the rebuild below
				rebuild.Insert(modifier);
				continue;
			}
			
			bool remove = false;
			modifier.timer = modifier.timer - m_TownManager.MODIFIER_FREQUENCY / 1000;
			
			if(modifier.timer <= 0)
			{
				remove = true;
			}else{				
				if(mod.handler){
					if(!mod.handler.OnActiveTick(town))
					{
						remove = true;						
					}
				}
			}
			if(!remove) {
				rebuild.Insert(modifier);								
			}else{
				recalc = true;
				OnTimeout(townID, modifier.id);
			}
		}
		
		if(recalc)
		{
			//rebuild arrays (minus removed)
			modifiers.Clear();
			foreach(OVT_TownModifierData mod : rebuild)
			{
				if(mod)
					modifiers.Insert(mod);
			}
		}
		
		//Call modifier OnTicks
		foreach(int i, OVT_ModifierConfig config : m_Config.m_aModifiers)
		{
			if(config.handler)
			{
				config.handler.OnTick(town);
			}
		}
		
		return recalc;
	}
	
	int Recalculate(array<ref OVT_TownModifierData> modifiers, int baseValue = 100, int min = 0, int max = 100)
	{
		float newval = baseValue;
		foreach(OVT_TownModifierData modifier : modifiers)
		{
			if(!modifier) continue;
			OVT_ModifierConfig mod = m_Config.m_aModifiers[modifier.id];
			newval += mod.baseEffect;
		}
		if(newval > max) newval = max;
		if(newval < min) newval = min;
		return Math.Round(newval);		
	}
	
	bool TryAddByName(int townId, string name)
	{
		foreach(int i, OVT_ModifierConfig config : m_Config.m_aModifiers)
		{
			if(config.name == name)
			{
				return TryAddModifier(townId, i);
			}
		}
		return false;
	}
	
	void RemoveByName(int townId, string name)
	{
		foreach(int i, OVT_ModifierConfig config : m_Config.m_aModifiers)
		{
			if(config.name == name)
			{
				RemoveModifier(townId, i);
				return;
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! How many of one modifier a town may carry at once.
	//!
	//! A STACKABLE modifier is allowed its configured stackLimit. A non-stackable one is allowed
	//! exactly ONE - stackLimit is not consulted for it, because a modifier that cannot stack has no
	//! stack to limit. Stating that as a NUMBER is the point of this method: TryAdd*Modifier applies
	//! the rule by refusing to insert a second copy (it refreshes the existing timer instead and
	//! reports success), which leaves callers that need to know BEFORE adding - a placement that must
	//! be refused, a UI that must grey a button - with nothing to ask. Asking the config directly is
	//! what produced the "not stackable, limit 1 = no limit at all" reading on both sides.
	//! \param[in] index Index of the modifier in this system's config
	//! \return Maximum instances allowed on one town, 0 when the index is not in this config
	int GetModifierLimit(int index)
	{
		if(!m_Config || !m_Config.m_aModifiers) return 0;
		if(index < 0 || index >= m_Config.m_aModifiers.Count()) return 0;

		OVT_ModifierConfig mod = m_Config.m_aModifiers[index];
		if(mod.flags & OVT_ModifierFlags.STACKABLE) return mod.stackLimit;

		return 1;
	}

	//------------------------------------------------------------------------------------------------
	//! How many instances of a modifier a town currently carries.
	//! \param[in] townId The town
	//! \param[in] index Index of the modifier in this system's config
	//! \return Current count, 0 when the town or the list cannot be resolved
	int CountModifier(int townId, int index)
	{
		if(!m_Towns || townId < 0 || townId >= m_Towns.Count()) return 0;

		array<ref OVT_TownModifierData> modifiers = GetModifiers(m_Towns[townId]);
		if(!modifiers) return 0;

		int count = 0;
		foreach(OVT_TownModifierData data : modifiers)
		{
			if(data && data.id == index) count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Room left for a named modifier on one town. Safe on clients: it reads the replicated town
	//! modifier lists, which is what lets the placement UI answer the same question the server will.
	//! \param[in] townId The town
	//! \param[in] name Modifier name as authored in the config
	//! \return Instances that may still be added (0 when full), or -1 when this system has no
	//! modifier by that name and therefore no opinion
	int GetModifierSpaceByName(int townId, string name)
	{
		if(!m_Config || !m_Config.m_aModifiers) return -1;

		foreach(int index, OVT_ModifierConfig config : m_Config.m_aModifiers)
		{
			if(config.name != name) continue;

			int space = GetModifierLimit(index) - CountModifier(townId, index);
			if(space < 0) return 0;

			return space;
		}

		return -1;
	}

	//! The modifier list this system owns on a town - support or stability. Overridden per system.
	//! \param[in] town The town to read
	//! \return The list, or null when this system does not own one
	protected array<ref OVT_TownModifierData> GetModifiers(OVT_TownData town)
	{
		return null;
	}

	protected bool TryAddModifier(int townId, int index)
	{

	}
	
	protected void RemoveModifier(int townId, int index)
	{
	
	}
	
	protected void OnTimeout(int townId, int index)
	{
	
	}
}