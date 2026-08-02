//------------------------------------------------------------------------------------------------
//! Persists the resistance treasury and tax rate from OVT_EconomyManagerComponent.
//!
//! BINDING. Listed in the ComponentSerializers block of the game-mode configuration in
//! Configs/Systems/Persistence/Overthrow.conf.
//!
//! SCOPE - READ THIS BEFORE ASSUMING PLAYER MONEY IS COVERED.
//! Player balances are NOT economy-manager state: OVT_EconomyManagerComponent.AddPlayerMoney() and
//! friends write OVT_PlayerData.money, which lives in OVT_PlayerManagerComponent. EPF's
//! OVT_EconomySaveData persisted exactly two fields for the same reason, and this serializer is a
//! faithful port of it. Player money round-trips only once OVT_PlayerManagerSerializer exists.
//!
//! POST-LOAD. Setting the two fields is all EPF's ApplyTo did. This serializer additionally fires
//! m_OnResistanceMoneyChanged through OVT_EconomyManagerComponent.ApplyPersistedEconomy(), so
//! anything already listening agrees with the restored campaign. It deliberately does NOT broadcast
//! an RPC: deserialization happens while the world is still being built, and every client - joining
//! or rejoining - receives both values through the component's own RplSave/RplLoad JIP payload
//! (OVT_EconomyManagerComponent.c:1570-1613), which is the authoritative path for clients.
//!
//! IDEMPOTENT ON A LIVE SESSION. Deserialize also runs when saved data is re-applied to a running
//! campaign (OVT_PersistenceManagerComponent.ReapplyLatestSaveData). ApplyPersistedEconomy() is two
//! assignments plus a UI-facing change event, all of which are safe to repeat with the same values.
//!
//! FORMAT. Binary contexts are POSITIONAL: write order must equal read order. Version first.
//------------------------------------------------------------------------------------------------
class OVT_EconomyManagerSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	//! \return The component class this serializer is responsible for.
	override static typename GetTargetType()
	{
		return OVT_EconomyManagerComponent;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the resistance treasury and tax rate.
	//! \param[in] owner The game mode entity owning the economy manager.
	//! \param[in] component The economy manager being saved.
	//! \param[in] context Save context to write into.
	//! \return OK, or ERROR when the component is not an Overthrow economy manager.
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		const OVT_EconomyManagerComponent economy = OVT_EconomyManagerComponent.Cast(component);
		if (!economy)
			return ESerializeResult.ERROR;

		context.WriteValue("version", 1);

		const int resistanceMoney = economy.GetResistanceMoney();
		context.Write(resistanceMoney);

		const float resistanceTax = economy.m_fResistanceTax;
		context.Write(resistanceTax);

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Restores the resistance treasury and tax rate, and fires the post-load change event.
	//! \param[in] owner The game mode entity owning the economy manager.
	//! \param[in] component The economy manager being loaded.
	//! \param[in] context Load context to read from.
	//! \return True when the payload was consumed.
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		OVT_EconomyManagerComponent economy = OVT_EconomyManagerComponent.Cast(component);
		if (!economy)
			return false;

		int version;
		context.ReadValue("version", version);

		// Absent payload (older save): do not apply zero-valued defaults over live state.
		if (version < 1)
			return true;

		int resistanceMoney;
		context.Read(resistanceMoney);

		float resistanceTax;
		context.Read(resistanceTax);

		economy.ApplyPersistedEconomy(resistanceMoney, resistanceTax);

		return true;
	}
}
