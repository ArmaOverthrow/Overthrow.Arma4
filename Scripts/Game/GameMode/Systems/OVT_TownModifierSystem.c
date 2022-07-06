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
	
	bool OnTick(inout array<ref int> modifiers, inout array<ref int> modifierTimers, OVT_TownData town)
	{
		//Check if we need to time out any modifiers
		array<int> rebuild = new array<int>;
		array<int> rebuildtimers = new array<int>;
		bool recalc = false;
		foreach(int i, int index : modifiers)
		{
			bool remove = false;
			modifierTimers[i] = modifierTimers[i] - m_TownManager.MODIFIER_FREQUENCY / 1000;
			if(modifierTimers[i] <= 0)
			{
				recalc = true;
				remove = true;
			}else{
				OVT_ModifierConfig mod = m_Config.m_aModifiers[index];
				if(mod.handler){
					if(!mod.handler.OnActiveTick(town))
					{
						remove = true;
						recalc = true;
					}
				}
			}
			if(!remove) {
				rebuild.Insert(index);
				rebuildtimers.Insert(modifierTimers[i]);
			}
		}
		
		if(recalc)
		{
			//rebuild arrays (minus removed)
			modifiers.Clear();
			modifierTimers.Clear();
			foreach(int index : rebuild)
			{
				modifiers.Insert(index);
			}
			foreach(int index : rebuildtimers)
			{
				modifierTimers.Insert(index);
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
	
	int Recalculate(array<ref int> modifiers, int baseValue = 100, int min = 0, int max = 100)
	{
		float newval = baseValue;
		foreach(int index : modifiers)
		{
			OVT_ModifierConfig mod = m_Config.m_aModifiers[index];
			newval += mod.baseEffect;
		}
		if(newval > max) newval = max;
		if(newval < min) newval = min;
		return Math.Round(newval);		
	}
	
	void TryAddByName(int townId, string name)
	{
		foreach(int i, OVT_ModifierConfig config : m_Config.m_aModifiers)
		{
			if(config.name == name)
			{
				TryAddModifier(townId, i);
				return;
			}
		}
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
	
	protected void TryAddModifier(int townId, int index)
	{
		
	}
	
	protected void RemoveModifier(int townId, int index)
	{
	
	}
}