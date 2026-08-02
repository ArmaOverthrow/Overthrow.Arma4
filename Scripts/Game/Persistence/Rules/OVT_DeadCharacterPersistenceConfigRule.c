//------------------------------------------------------------------------------------------------
//! Persistence configuration rule that matches CORPSES - characters whose controller reports them dead.
//!
//! WHY THIS EXISTS. Overthrow's ethos is that death costs you everything you were carrying, but that
//! what you were carrying is still lying there to be reclaimed. In-session that already worked: a killed
//! character stays in the world as lootable remains. Across a save it did not, and the reason is one bit
//! of configuration:
//!
//!   - every character is tracked (Prefabs/Characters/Core/Character_Base.et carries the native
//!     Persistence component), so a corpse IS written into every save point, inventory and all;
//!   - but the configuration it is written under is Overthrow's AI-character override {64EACAC5BFDB31EC},
//!     which sets SelfSpawn 0 so that LIVE AI is not doubled on load (Overthrow's managers rebuild
//!     garrisons and patrols themselves - decision v2-5). Vanilla's own player-character config is
//!     SelfSpawn 0 too.
//!
//! So the corpse was saved and then nobody spawned it back. Nothing rebuilds corpses - no manager knows
//! they existed - which by decision v2-5 ("does an Overthrow system already rebuild it?") is precisely
//! the case where an entity SHOULD persist. This rule is how a corpse gets a configuration of its own:
//! same collection, same serializers, SelfSpawn 1.
//!
//! BOUND IN Configs/Systems/Persistence/Overthrow.conf, in the Overthrow configuration group, at
//! Priority 36000 - above vanilla's player character (33000) and above the AI/character default, so a
//! dead character takes this configuration and a live one never does.
//!
//! PersistenceConfigRule is the ONE script-extensible class in the persistence configuration layer
//! (vanilla-api-reference.md 2). Its constructor is private because instances are built by the engine
//! from the .conf entry - the same arrangement as PersistentState, which vanilla itself subclasses in
//! script (SCR_PlayerReconnectData).
//!
//! WHEN IS THIS ASKED? Only when an instance is matched to a configuration: once when it starts being
//! tracked, and again whenever something calls PersistenceSystem.ReloadConfig() on it. A character that
//! is alive when it is tracked therefore keeps its alive configuration until somebody asks again - which
//! is what OVT_OverthrowGameMode.OnCharacterKilledPersist() does on every character death.
//------------------------------------------------------------------------------------------------
class OVT_DeadCharacterPersistenceConfigRule : PersistenceConfigRule
{
	//------------------------------------------------------------------------------------------------
	//! Whether an entity is a dead character.
	//!
	//! Signature mirrors PersistenceConfigRule.IsMatch() exactly, `const` included.
	//!
	//! GetCharacterController().IsDead() is vanilla's own test for this question on this class -
	//! SCR_SpawnLogic.c:423 uses it to refuse a persistence-restored character that came back dead, which
	//! is also the evidence that a corpse RESTORES as a corpse rather than being revived: vanilla would
	//! not need the check otherwise. Note IsDead() is strictly dead - an INCAPACITATED character is still
	//! alive and is deliberately not matched.
	//! \param[in] entity The entity being matched. Non-characters, and characters whose controller is not
	//! available yet, answer false.
	//! \return True when this entity is a corpse.
	override protected bool IsMatch(const IEntity entity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character)
			return false;

		CharacterControllerComponent controller = character.GetCharacterController();
		if (!controller)
			return false;

		return controller.IsDead();
	}
}
