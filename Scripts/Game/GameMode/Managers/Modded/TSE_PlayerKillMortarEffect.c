[EntityEditorProps(category: "Overthrow/Managers/Modded", description: "Spawns mortar barrage effects when players kill other players")]
class TSE_PlayerKillMortarEffectClass : OVT_ComponentClass
{
}

class TSE_PlayerKillMortarEffect : OVT_Component
{
	[Attribute("{5D48E2F7DB0C3714}PrefabsEditable/EffectsModules/Mortar/EffectModule_Zoned_MortarBarrage_Small.et", desc: "Mortar barrage effect prefab to spawn")]
	ResourceName m_MortarBarragePrefab;
	
	[Attribute(defvalue: "3", desc: "Delay in seconds before spawning the mortar barrage")]
	float m_fSpawnDelay;
	
	[Attribute(defvalue: "50", desc: "Random offset radius in meters for mortar spawn position")]
	float m_fSpawnRadius;
	
	[Attribute(defvalue: "true", desc: "Whether to enable the mortar barrage effect")]
	bool m_bEnabled;
	
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		Print("TSE_PlayerKillMortarEffect: OnPostInit called - Server: " + Replication.IsServer() + ", Enabled: " + m_bEnabled);
		
		if (!Replication.IsServer()) 
		{
			Print("TSE_PlayerKillMortarEffect: Not on server, skipping initialization");
			return;
		}
		
		// Subscribe to character kill events (which includes player kills)
		OVT_OverthrowGameMode gameMode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if (gameMode)
		{
			gameMode.GetOnCharacterKilled().Insert(OnCharacterKilled);
			Print("TSE_PlayerKillMortarEffect: Successfully subscribed to character kill events");
			
			// Test if the event system is working by checking the invoker
			ScriptInvoker<IEntity, IEntity> invoker = gameMode.GetOnCharacterKilled();
			if (invoker)
			{
				Print("TSE_PlayerKillMortarEffect: Event invoker is valid");
			}
		}
		else
		{
			Print("TSE_PlayerKillMortarEffect: ERROR - Could not get game mode!");
		}
	}
	
	// Note: No destroy method in base class, so we don't override anything
	// The component will be cleaned up automatically when the entity is destroyed
	
	//------------------------------------------------------------------------------------------------
	//! Called when a character is killed (including players)
	protected void OnCharacterKilled(IEntity victim, IEntity instigator)
	{
		Print("TSE_PlayerKillMortarEffect: OnCharacterKilled called");
		
		if (!m_bEnabled) 
		{
			Print("TSE_PlayerKillMortarEffect: Component is disabled, ignoring kill event");
			return;
		}
		
		if (!victim || !instigator)
		{
			Print("TSE_PlayerKillMortarEffect: Victim or instigator is null, ignoring kill event");
			return;
		}
		
		// Check if victim is a player
		int victimPlayerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(victim);
		Print("TSE_PlayerKillMortarEffect: Victim player ID: " + victimPlayerId);
		if (victimPlayerId <= 0) 
		{
			Print("TSE_PlayerKillMortarEffect: Victim is not a player, ignoring kill event");
			return;
		}
		
		// Check if instigator is a player
		int instigatorPlayerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(instigator);
		Print("TSE_PlayerKillMortarEffect: Instigator player ID: " + instigatorPlayerId);
		if (instigatorPlayerId <= 0) 
		{
			Print("TSE_PlayerKillMortarEffect: Instigator is not a player, ignoring kill event");
			return;
		}
		
		// This is a player vs player kill
		Print("TSE_PlayerKillMortarEffect: Player " + instigatorPlayerId + " killed player " + victimPlayerId + ", spawning mortar barrage");
		
		// Schedule the mortar barrage spawn
		GetGame().GetCallqueue().CallLater(SpawnMortarBarrage, m_fSpawnDelay * 1000, false, instigator.GetOrigin());
	}
	
	//------------------------------------------------------------------------------------------------
	//! Spawns the mortar barrage effect at the specified location
	protected void SpawnMortarBarrage(vector spawnPosition)
	{
		if (!m_MortarBarragePrefab) 
		{
			Print("TSE_PlayerKillMortarEffect: ERROR - No mortar barrage prefab set!");
			return;
		}
		
		// Add random offset to spawn position
		vector randomOffset = Vector(s_AIRandomGenerator.RandFloatXY(-m_fSpawnRadius, m_fSpawnRadius), 0, s_AIRandomGenerator.RandFloatXY(-m_fSpawnRadius, m_fSpawnRadius));
		
		vector finalPosition = spawnPosition + randomOffset;
		
		// Spawn the mortar barrage effect
		EntitySpawnParams spawnParams = EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		spawnParams.Transform[3] = finalPosition;
		
		IEntity mortarEffect = GetGame().SpawnEntityPrefab(Resource.Load(m_MortarBarragePrefab), null, spawnParams);
		
		if (mortarEffect)
		{
			Print("TSE_PlayerKillMortarEffect: Spawned mortar barrage at position " + finalPosition);
			
			// Auto-delete the effect after some time to prevent clutter
			GetGame().GetCallqueue().CallLater(DeleteMortarEffect, 30000, false, mortarEffect); // 30 seconds
		}
		else
		{
			Print("TSE_PlayerKillMortarEffect: ERROR - Failed to spawn mortar barrage effect!");
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Deletes the mortar effect entity to prevent clutter
	protected void DeleteMortarEffect(IEntity mortarEffect)
	{
		if (mortarEffect && !mortarEffect.IsDeleted())
		{
			SCR_EntityHelper.DeleteEntityAndChildren(mortarEffect);
			Print("TSE_PlayerKillMortarEffect: Deleted mortar barrage effect");
		}
	}
} 