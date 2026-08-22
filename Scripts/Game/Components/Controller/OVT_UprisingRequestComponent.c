[ComponentEditorProps(category: "Overthrow/Components/Controller", description: "Server-authoritative town uprising request for one player")]
class OVT_UprisingRequestComponentClass : OVT_ControllerRequestComponentClass {};

//------------------------------------------------------------------------------------------------
//! Server-authoritative town uprising start, on the per-player OVT_OverthrowController entity.
//!
//! Follows the controller discipline (overthrow-controller.md): no new client->server RPCs go on
//! the deprecated OVT_PlayerCommsComponent. The client names a TOWN ID - a lookup key into the
//! server's own town records - never a coordinate, and identity is resolved server-side from the
//! controller entity this component sits on (ResolveOwningPlayerId), never from the payload.
//!
//! Every rule the town flag action checks client-side (OVT_StartUprisingAction) is re-checked
//! here against server state: occupied non-village town, support above
//! OVT_OccupyingFactionManager.UPRISING_SUPPORT_THRESHOLD, requesting character alive and inside
//! the town's range, no battle already running. A refusal costs the player nothing - the flag
//! action is still there to press again.
//!
//! No result RPC on purpose: success is globally visible (battle notification + HUD countdown on
//! every client), and a refusal is only reachable through a race or a crafted packet - the
//! arrival/refusal Print pair below is the play-test diagnostic, same as the other controller
//! components.
//------------------------------------------------------------------------------------------------
class OVT_UprisingRequestComponent : OVT_ControllerRequestComponent
{
	//------------------------------------------------------------------------------------------------
	//! Ask the server to start an uprising battle in a town.
	//! \param[in] townId Index into the town manager's town list (same ids the town RPCs use).
	void RequestStartUprising(int townId)
	{
		if(Replication.IsServer())
			RpcAsk_StartUprising(townId);
		else
			Rpc(RpcAsk_StartUprising, townId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: validate the request against server state and start the town QRF.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_StartUprising(int townId)
	{
		if(!Replication.IsServer()) return;

		// BUG-090 diagnostic: this line firing proves the request left the client and was decoded
		// with the right arity; if it prints and no battle starts, a rule below refused
		Print("[Overthrow] Uprising request received: townId=" + townId.ToString(), LogLevel.NORMAL);

		OVT_OccupyingFactionManager of = OVT_Global.GetOccupyingFaction();
		if(!of || of.m_bQRFActive)
		{
			Print("[Overthrow] Uprising request refused: a battle is already active", LogLevel.WARNING);
			return;
		}

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if(!towns || townId < 0 || townId >= towns.m_Towns.Count())
		{
			Print("[Overthrow] Uprising request refused: unknown town " + townId.ToString(), LogLevel.WARNING);
			return;
		}

		OVT_TownData town = towns.m_Towns[townId];
		if(town.size == OVT_TownSize.VILLAGE) return;
		if(!town.IsOccupyingFaction())
		{
			Print("[Overthrow] Uprising request refused: town is not occupied", LogLevel.WARNING);
			return;
		}
		if(town.SupportPercentage() <= OVT_OccupyingFactionManager.UPRISING_SUPPORT_THRESHOLD)
		{
			Print("[Overthrow] Uprising request refused: not enough support in town " + townId.ToString(), LogLevel.WARNING);
			return;
		}

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0)
		{
			Print("[Overthrow] Uprising request refused: could not resolve the requesting player", LogLevel.WARNING);
			return;
		}

		// The character is the server-side position truth - the payload never carries a coordinate
		ChimeraCharacter character = ChimeraCharacter.Cast(GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId));
		if(!character) return;
		CharacterControllerComponent characterController = character.GetCharacterController();
		if(characterController && characterController.IsDead()) return;
		if(vector.Distance(character.GetOrigin(), town.location) > towns.GetTownRange(town))
		{
			Print("[Overthrow] Uprising request refused: player " + playerId.ToString() + " is not in town " + townId.ToString(), LogLevel.WARNING);
			return;
		}

		of.StartTownQRF(town);
	}

}
