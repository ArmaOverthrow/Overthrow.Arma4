[ComponentEditorProps(category: "Overthrow/Components/Controller", description: "Client->server relay for radio tower sabotage")]
class OVT_TowerSabotageComponentClass : OVT_ComponentClass {};

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
class OVT_TowerSabotageComponent : OVT_Component
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

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SabotageTower(vector towerPos)
	{
		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!character) return;

		OVT_OccupyingFactionManager of = OVT_Global.GetOccupyingFaction();
		if(!of) return;

		OVT_RadioTowerData tower = of.GetNearestRadioTower(towerPos);
		if(!tower || !tower.IsOccupyingFaction() || tower.IsDisabled()) return;

		if(vector.Distance(character.GetOrigin(), tower.location) > SABOTAGE_MAX_DISTANCE) return;

		of.SetRadioTowerDisabled(tower, OVT_Global.GetConfig().m_Difficulty.radioTowerSabotageTime);

		// Sabotage is illegal - being seen doing it makes you wanted (same seen-gate as looting/posters)
		OVT_PlayerWantedComponent wanted = OVT_PlayerWantedComponent.Cast(character.FindComponent(OVT_PlayerWantedComponent));
		if(wanted)
			wanted.OnIllegalActionSeen("WantedSabotage");
	}

	//! Same owner resolution as OVT_ShopTransactionComponent - candidate for a shared base class
	//! when a third controller component needs it
	protected int ResolveOwningPlayerId()
	{
		OVT_OverthrowController owner = OVT_OverthrowController.Cast(GetOwner());
		if(!owner) return -1;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return -1;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if(!playerManager) return -1;

		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);

		foreach(int playerId : playerIds)
		{
			OVT_OverthrowController controller = players.GetController(playerId);
			if(controller == owner) return playerId;
		}

		return -1;
	}
}
