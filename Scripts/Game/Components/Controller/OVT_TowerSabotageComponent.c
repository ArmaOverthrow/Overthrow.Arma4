[ComponentEditorProps(category: "Overthrow/Components/Controller", description: "Client->server relay for radio tower sabotage")]
class OVT_TowerSabotageComponentClass : OVT_ControllerRequestComponentClass {};

//------------------------------------------------------------------------------------------------
//! Client->server relay for sabotaging occupying-faction radio towers, on the per-player
//! OVT_OverthrowController entity (project rule: every client->server RPC lives on a controller
//! component). The server validates everything itself: tower existence, ownership,
//! current disabled state, and the saboteur's distance to the tower - never trusting the client.
//!
//! A successful sabotage takes the tower off the air for
//! OVT_DifficultySettings.radioTowerSabotageTime seconds (see OVT_RadioTowerData.disabledRemaining)
//! and, if the saboteur is currently seen, makes them wanted (OVT_PlayerWantedComponent).
//------------------------------------------------------------------------------------------------
class OVT_TowerSabotageComponent : OVT_ControllerRequestComponent
{
	//! How far the saboteur may be from the tower when the action completes (action radius + slack)
	protected const float SABOTAGE_MAX_DISTANCE = 25;

	//------------------------------------------------------------------------------------------------
	//! Asks the server to sabotage the tower nearest towerPos.
	//!
	//! BRANCHES ON Replication.IsServer(), AND MUST. Until the Phase 10 arity audit of the controller
	//! migration this was an unconditional Rpc() to an RplRcver.Server handler - and an RplRcver.Server
	//! RPC marshalled BY the authority is delivered to nobody, so on a LISTEN-SERVER HOST sabotaging a
	//! radio tower did nothing at all: no disable, no wanted level, no error. Same class of defect as
	//! the ten handlers migrated in P2-P8 (see P2-5 in that feature's context.md).
	//! \param[in] towerPos Client's idea of where the tower is; the server re-resolves the nearest one.
	void RequestSabotage(vector towerPos)
	{
		if(Replication.IsServer())
		{
			RpcAsk_SabotageTower(towerPos);
		}else{
			Rpc(RpcAsk_SabotageTower, towerPos);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: take the tower nearest the claimed position off the air.
	//!
	//! The client's towerPos is a HINT, never an authority: the server re-resolves the nearest recorded
	//! tower from its own registry and then measures the CALLER'S CHARACTER against that record's
	//! position, so a crafted vector can only ever name a real tower the caller is already standing at.
	//! Every refusal is logged (Q9) - a sabotage that silently did nothing was indistinguishable from a
	//! dropped packet, which is exactly how the listen-host routing defect (P10-3) survived unnoticed.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SabotageTower(vector towerPos)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0)
		{
			RejectSabotage(playerId, "could not resolve the requesting player");
			return;
		}

		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!character)
		{
			RejectSabotage(playerId, "the caller has no controlled character");
			return;
		}

		OVT_OccupyingFactionManager of = OVT_Global.GetOccupyingFaction();
		if(!of) return;

		OVT_RadioTowerData tower = of.GetNearestRadioTower(towerPos);
		if(!tower || !tower.IsOccupyingFaction())
		{
			RejectSabotage(playerId, "no occupying-faction radio tower at the claimed position");
			return;
		}

		if(tower.IsDisabled())
		{
			RejectSabotage(playerId, "the tower is already off the air");
			return;
		}

		if(vector.Distance(character.GetOrigin(), tower.location) > SABOTAGE_MAX_DISTANCE)
		{
			RejectSabotage(playerId, "the caller is not at the tower");
			return;
		}

		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if(!difficulty) return;

		of.SetRadioTowerDisabled(tower, difficulty.radioTowerSabotageTime);

		// Sabotage is illegal - being seen doing it makes you wanted (same seen-gate as looting/posters)
		OVT_PlayerWantedComponent wanted = OVT_PlayerWantedComponent.Cast(character.FindComponent(OVT_PlayerWantedComponent));
		if(wanted)
			wanted.OnIllegalActionSeen("WantedSabotage");
	}

	//------------------------------------------------------------------------------------------------
	//! Logs a refused sabotage with its reason (quality bar Q9).
	//! \param[in] playerId The rejected player, or -1 when even that could not be resolved.
	//! \param[in] reason Why.
	protected void RejectSabotage(int playerId, string reason)
	{
		Print(string.Format("[OVT_TowerSabotageComponent] Rejected sabotage request from player %1: %2", playerId.ToString(), reason), LogLevel.WARNING);
	}

}
