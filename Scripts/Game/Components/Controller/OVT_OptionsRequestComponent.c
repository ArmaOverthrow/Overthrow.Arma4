[ComponentEditorProps(category: "Overthrow/Components/Controller", description: "Server-authoritative option writes for one player")]
class OVT_OptionsRequestComponentClass : OVT_ControllerRequestComponentClass {};

//------------------------------------------------------------------------------------------------
//! Server-authoritative FACTION and WORLD option writes, on the per-player OVT_OverthrowController
//! entity.
//!
//! The client sends an id and a value and nothing else. The server re-derives the caller, the
//! definition, the scope and the permission from scratch, then hands the write to the options
//! manager. A LOCAL value never travels at all.
//------------------------------------------------------------------------------------------------
class OVT_OptionsRequestComponent : OVT_ControllerRequestComponent
{
	//! Stands in for the scope name in a rejection that happens before a definition is found.
	protected static const string SCOPE_UNKNOWN = "unknown";

	//------------------------------------------------------------------------------------------------
	//! Ask the server to write one option.
	//!
	//! BRANCHES ON Replication.IsServer(), AND MUST: an RplRcver.Server Rpc marshalled by the authority
	//! is delivered to nobody, so a listen host or a single player would silently write nothing.
	//! \param[in] id The option id.
	//! \param[in] value The candidate value, in the option's string encoding.
	void SetOption(string id, string value)
	{
		if (Replication.IsServer())
		{
			RpcAsk_SetOption(id, value);
			return;
		}

		// Rpc arity audited: 2 payload args match RpcAsk_SetOption(string id, string value)
		Rpc(RpcAsk_SetOption, id, value);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: validate one option write and apply it.
	//!
	//! Rejection order: server, caller, manager, definition, scope not LOCAL, permission. Every
	//! rejection after the caller check logs one WARNING line.
	//! \param[in] id The option id.
	//! \param[in] value The candidate value, in the option's string encoding.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetOption(string id, string value)
	{
		if (!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if (playerId <= 0) return;

		OVT_OptionsManagerComponent options = OVT_Global.GetOptions();
		if (!options)
		{
			LogRejection(playerId, id, SCOPE_UNKNOWN, "the options manager does not exist");
			return;
		}

		if (!options.HasOption(id))
		{
			LogRejection(playerId, id, SCOPE_UNKNOWN, "no definition for this id");
			return;
		}

		OVT_OptionDefinition def = options.GetDefinition(id);
		if (!def) return;

		string scopeName = typename.EnumToString(OVT_EOptionScope, def.m_eScope);

		if (!OVT_OptionsRegistry.ScopeIsServerAuthoritative(def.m_eScope))
		{
			LogRejection(playerId, id, scopeName, "a LOCAL value never travels");
			return;
		}

		if (!PlayerMayEditScope(playerId, def.m_eScope))
		{
			LogRejection(playerId, id, scopeName, "the player lacks the permission");
			return;
		}

		options.SetOption(id, value);
	}

	//------------------------------------------------------------------------------------------------
	//! One WARNING line for a refused write.
	//! \param[in] playerId The requesting player's runtime id.
	//! \param[in] id The option id the client named.
	//! \param[in] scopeName The scope name, or SCOPE_UNKNOWN before a definition is found.
	//! \param[in] reason Why the server refused.
	protected void LogRejection(int playerId, string id, string scopeName, string reason)
	{
		Print("[Overthrow] Options: refused player " + playerId.ToString() + " a write to '" + id + "' (scope " + scopeName + ", " + reason + ")", LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a player may write an option in one scope (D7).
	//!
	//! The server handler above and the Phase 7 form both call this, so a player sees the same gate the
	//! server applies. An admin passes every scope, because an admin outranks an officer.
	//! \param[in] playerId The player's runtime id.
	//! \param[in] scope The option's scope.
	//! \return True when the player may write the option.
	static bool PlayerMayEditScope(int playerId, OVT_EOptionScope scope)
	{
		if (scope == OVT_EOptionScope.LOCAL)
			return true;

		if (OVT_OptionsRegistry.ScopeRequiresOfficer(scope))
		{
			OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
			OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();

			if (players && resistance && resistance.IsOfficer(playerId))
				return true;

			return PlayerMayEditWorld(playerId);
		}

		if (OVT_OptionsRegistry.ScopeRequiresAdmin(scope))
			return PlayerMayEditWorld(playerId);

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a player may write a WORLD option: the admin role, with a host fallback (D7).
	//!
	//! P3.3 finding, read from the vanilla source: single player reports NO admin role. EPlayerRole
	//! comes from the server admin list or a session vote, and vanilla itself pairs every admin test
	//! with Replication.IsRunning() (SCR_EditorManagerCore.c:443, "suppress it in SP"). The host
	//! fallback is therefore the branch that answers in single player and in Workbench.
	//! \param[in] playerId The player's runtime id.
	//! \return True for an admin, or for the local player on the authority.
	static bool PlayerMayEditWorld(int playerId)
	{
		if (playerId <= 0) return false;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (playerManager && SCR_Global.IsAdminRole(playerManager.GetPlayerRoles(playerId)))
			return true;

		return Replication.IsServer() && playerId == SCR_PlayerController.GetLocalPlayerId();
	}
}
