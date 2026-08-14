class OVT_ContainerTransferComponentClass : OVT_BaseServerProgressComponentClass {};

//------------------------------------------------------------------------------------------------
//! Callback class to bridge inventory manager callbacks to RPC updates
class OVT_ContainerTransferCallback : OVT_StorageProgressCallback
{
	protected OVT_ContainerTransferComponent m_Component;

	void OVT_ContainerTransferCallback(OVT_ContainerTransferComponent component)
	{
		m_Component = component;
	}

	override void OnProgressUpdate(float progress, int currentItem, int totalItems, string operation)
	{
		if (m_Component && Replication.IsServer())
			m_Component.SendProgressUpdate(progress, currentItem, totalItems, operation);
	}

	override void OnComplete(int itemsTransferred, int itemsSkipped)
	{
		if (m_Component && Replication.IsServer())
			m_Component.SendOperationComplete(itemsTransferred, itemsSkipped);
	}

	override void OnError(string errorMessage)
	{
		if (m_Component && Replication.IsServer())
			m_Component.SendOperationError(errorMessage);
	}
}

//------------------------------------------------------------------------------------------------
//! Component that handles all container transfer operations with progress tracking.
//! Replaced the container transfer functionality on the legacy comms monolith (deleted in Phase 10).
//!
//! VALIDATION (added 2026-08-14 by the pre-migration audit - see docs/bugs/BUG-166.md). This is the
//! OLDEST controller component and it predates the §3.4 validation ladder: every one of its six
//! RplRcver.Server handlers used to be a bare forward that trusted an RplId, a position and a radius
//! from the client and never asked WHO was calling. That made "move the contents of any replicated
//! storage on the map into any other" a one-packet operation for a modified client. Every handler now
//! resolves the caller from its own controller entity and requires that caller to be standing at both
//! ends of the transfer and entitled to touch them.
//!
//! IT CANNOT EXTEND OVT_ControllerRequestComponent - it needs OVT_BaseServerProgressComponent's
//! progress plumbing and EnforceScript has no multiple inheritance - so it reuses the SAME two rule
//! bodies through their static forms rather than carrying copies (see ResolveCallerPlayerId).
class OVT_ContainerTransferComponent : OVT_BaseServerProgressComponent
{
	//! How far the caller may be from EITHER end of a transfer.
	//!
	//! The two ends are legitimately apart: OVT_LoadStorageAction / OVT_UnloadStorageAction pick the
	//! nearest vehicle within 15 m of the container and the player is standing at the container, so the
	//! gate has to cover 15 m plus interaction range plus latency. 30 m is the number the shop and
	//! import seams already use for "the player and the vehicle are at the same place", so a transfer is
	//! never refused where a sale at the same spot would be accepted.
	protected const float TRANSFER_MAX_DISTANCE = 30;

	//! Upper bound on a client-supplied container-collection radius. The only wrapper that sends one
	//! defaults to 75 m; the cap exists because the value goes straight into a world sphere query on the
	//! SERVER, so an unbounded radius from a modified client is a denial of service, not a big transfer.
	protected const float COLLECT_MAX_RADIUS = 100;

	//! Upper bound on a client-supplied battlefield-loot radius. The only caller sends 25 m.
	protected const float LOOT_MAX_RADIUS = 50;

	protected ref OVT_ContainerTransferCallback m_Callback;

	//------------------------------------------------------------------------------------------------
	//! Get or create the callback instance
	protected OVT_ContainerTransferCallback GetCallback()
	{
		if (!m_Callback)
			m_Callback = new OVT_ContainerTransferCallback(this);
		return m_Callback;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Transfer contents from one container to another
	//! \param[in] from Source container entity
	//! \param[in] to Target container entity
	//! \param[in] deleteEmpty Delete source if empty after transfer
	void TransferStorage(IEntity from, IEntity to, bool deleteEmpty = false)
	{
		if (!from || !to) return;
		
		RplComponent fromRpl = RplComponent.Cast(from.FindComponent(RplComponent));
		RplComponent toRpl = RplComponent.Cast(to.FindComponent(RplComponent));
		
		if (!fromRpl || !toRpl) return;
		
		if(Replication.IsServer())
		{
			RpcAsk_TransferStorage(fromRpl.Id(),toRpl.Id(), deleteEmpty);
		}else{
			Rpc(RpcAsk_TransferStorage, fromRpl.Id(), toRpl.Id(), deleteEmpty);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Transfer contents for FOB deployment with custom progress message
	//! \param[in] from Source container entity (mobile FOB)
	//! \param[in] to Target container entity (deployed FOB)
	//! \param[in] deleteEmpty Delete source if empty after transfer
	void TransferStorageForDeployment(IEntity from, IEntity to, bool deleteEmpty = false)
	{
		if (!from || !to) return;
		
		RplComponent fromRpl = RplComponent.Cast(from.FindComponent(RplComponent));
		RplComponent toRpl = RplComponent.Cast(to.FindComponent(RplComponent));
		
		if (!fromRpl || !toRpl) return;
		
		if(Replication.IsServer())
		{
			RpcAsk_TransferStorageForDeployment(fromRpl.Id(),toRpl.Id(), deleteEmpty);
		}else{
			Rpc(RpcAsk_TransferStorageForDeployment, fromRpl.Id(), toRpl.Id(), deleteEmpty);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Server: move one container's contents into another.
	//!
	//! Also reached SERVER-SIDE from OVT_ResistanceFactionManager.DeployFOB, on the deploying player's
	//! own controller - which is why every gate below is one the FOB seam has already applied to the
	//! same player and the same vehicle (RpcAsk_DeployFOB: 15 m + PlayerMayUseVehicle). A refusal here
	//! that the FOB seam would have allowed is therefore not reachable; RejectTransfer still reports
	//! through the error channel so that if one ever is, the manager's in-flight latch clears instead of
	//! wedging FOB operations for the whole session.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_TransferStorage(RplId fromId, RplId toId, bool deleteEmpty)
	{
		// Validate we're on server
		if (!Replication.IsServer()) return;

		int playerId = ResolveCallerPlayerId();
		if (playerId <= 0)
		{
			RejectTransfer(playerId, "transfer storage", "could not resolve the requesting player");
			return;
		}

		IEntity from = GetEntityFromRplId(fromId);
		IEntity to = GetEntityFromRplId(toId);
		if (!from || !to)
		{
			RejectTransfer(playerId, "transfer storage", "source or target no longer exists");
			return;
		}

		if (!CallerMayReach(playerId, from, "transfer storage")) return;
		if (!CallerMayReach(playerId, to, "transfer storage")) return;

		// Start the operation
		StartOperation("#OVT-Progress-TransferringItems");
		
		// Configure transfer
		OVT_StorageOperationConfig config = new OVT_StorageOperationConfig(
			deleteEmpty,// skipWeaponsOnGround
			false,      // deleteEmptyContainers
			50,         // itemsPerBatch
			100,        // batchDelayMs
			-1,         // searchRadius (not needed for direct transfer)
			1           // maxBatchesPerFrame
		);
		
		// Use inventory manager with our callback
		OVT_Global.GetInventory().TransferStorageByRplId(fromId, toId, config, GetCallback());
	}
	
	//------------------------------------------------------------------------------------------------
	//! Server: as RpcAsk_TransferStorage, with the FOB deployment progress caption.
	//!
	//! ! CALLER-LESS as of the 2026-08-14 audit: nothing in Scripts/ calls TransferStorageForDeployment.
	//! It is validated rather than deleted because deleting a public entry point is outside an audit's
	//! remit, but it is a deletion candidate - an endpoint with no callers is an attack surface with no
	//! purpose (the same reasoning §3.7/D6 applied to three monolith RPCs).
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_TransferStorageForDeployment(RplId fromId, RplId toId, bool deleteEmpty)
	{
		// Validate we're on server
		if (!Replication.IsServer()) return;

		int playerId = ResolveCallerPlayerId();
		if (playerId <= 0)
		{
			RejectTransfer(playerId, "deployment transfer", "could not resolve the requesting player");
			return;
		}

		IEntity from = GetEntityFromRplId(fromId);
		IEntity to = GetEntityFromRplId(toId);
		if (!from || !to)
		{
			RejectTransfer(playerId, "deployment transfer", "source or target no longer exists");
			return;
		}

		if (!CallerMayReach(playerId, from, "deployment transfer")) return;
		if (!CallerMayReach(playerId, to, "deployment transfer")) return;

		// Start the operation with deployment message
		StartOperation("#OVT-Progress-DeployingFOB");
		
		// Configure transfer
		OVT_StorageOperationConfig config = new OVT_StorageOperationConfig(
			deleteEmpty,// skipWeaponsOnGround
			false,      // deleteEmptyContainers
			50,         // itemsPerBatch
			100,        // batchDelayMs
			-1,         // searchRadius (not needed for direct transfer)
			1           // maxBatchesPerFrame
		);
		
		// Use inventory manager with our callback
		OVT_Global.GetInventory().TransferStorageByRplId(fromId, toId, config, GetCallback());
	}
	
	//------------------------------------------------------------------------------------------------
	//! Collect all containers in area and transfer to vehicle
	//! \param[in] pos Center position for search
	//! \param[in] targetVehicle Vehicle to transfer items to
	//! \param[in] radius Search radius in meters
	void CollectContainers(vector pos, IEntity targetVehicle, float radius = 75.0)
	{
		if (!targetVehicle) return;
		
		RplComponent vehicleRpl = RplComponent.Cast(targetVehicle.FindComponent(RplComponent));
		if (!vehicleRpl) return;
		
		if(Replication.IsServer())
		{
			RpcAsk_CollectContainers(pos, vehicleRpl.Id(), radius);
		}else{
			Rpc(RpcAsk_CollectContainers, pos, vehicleRpl.Id(), radius);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Server: sweep every container within a radius of a position into a vehicle.
	//!
	//! BOTH client-supplied numbers are checked, and they are checked for different reasons: \a pos
	//! decides WHOSE containers get emptied (so the caller must be standing at it), \a radius decides
	//! how much server work one packet buys (so it is capped). Neither was checked before.
	//!
	//! ! CALLER-LESS as of the 2026-08-14 audit - same deletion-candidate note as
	//! RpcAsk_TransferStorageForDeployment. The FOB undeploy path uses RpcAsk_UndeployFOB, not this.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_CollectContainers(vector pos, RplId vehicleId, float radius)
	{
		// Validate we're on server
		if (!Replication.IsServer()) return;

		int playerId = ResolveCallerPlayerId();
		if (playerId <= 0)
		{
			RejectTransfer(playerId, "collect containers", "could not resolve the requesting player");
			return;
		}

		if (radius <= 0 || radius > COLLECT_MAX_RADIUS)
		{
			RejectTransfer(playerId, "collect containers", "search radius out of range");
			return;
		}

		IEntity vehicle = GetEntityFromRplId(vehicleId);
		if (!vehicle)
		{
			SendOperationError("Vehicle not found");
			return;
		}

		if (!CallerMayReach(playerId, vehicle, "collect containers")) return;

		if (!CallerIsWithin(playerId, pos, TRANSFER_MAX_DISTANCE))
		{
			RejectTransfer(playerId, "collect containers", "the caller is not at the claimed collection point");
			return;
		}

		// Start the operation
		StartOperation("#OVT-Progress-CollectingContainers");
		
		// Configure collection
		OVT_StorageOperationConfig config = new OVT_StorageOperationConfig(
			true,       // skipWeaponsOnGround
			true,       // deleteEmptyContainers
			100,        // itemsPerBatch
			300,        // batchDelayMs
			radius,     // searchRadius
			3           // maxBatchesPerFrame
		);
		
		// Use inventory manager with our callback
		OVT_Global.GetInventory().CollectContainersToVehicle(pos, vehicle, config, GetCallback());
	}
	
	//------------------------------------------------------------------------------------------------
	//! Transfer container contents to nearest warehouse
	//! \param[in] from Source container entity
	void TransferToWarehouse(IEntity from)
	{
		if (!from) return;
		
		RplComponent fromRpl = RplComponent.Cast(from.FindComponent(RplComponent));
		if (!fromRpl) return;
		
		if(Replication.IsServer())
		{
			RpcAsk_TransferToWarehouse(fromRpl.Id());
		}else{
			Rpc(RpcAsk_TransferToWarehouse, fromRpl.Id());
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Server: empty a container into the warehouse nearest to IT (the manager's own 50 m rule).
	//!
	//! The warehouse's own accessibility rule (private / owned / rented) is deliberately NOT applied
	//! here: OVT_VehicleMenuContext.PutInWarehouse does not apply it either, and adding it would refuse
	//! deposits the menu currently offers - a gameplay change, which this audit does not make (G6).
	//! What IS new is that the caller must be at the container and entitled to touch it, so this stops
	//! being "empty any replicated storage on the map into the resistance's stock".
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_TransferToWarehouse(RplId fromId)
	{
		// Validate we're on server
		if (!Replication.IsServer()) return;

		int playerId = ResolveCallerPlayerId();
		if (playerId <= 0)
		{
			RejectTransfer(playerId, "warehouse transfer", "could not resolve the requesting player");
			return;
		}

		IEntity from = GetEntityFromRplId(fromId);
		if (!from)
		{
			RejectTransfer(playerId, "warehouse transfer", "source no longer exists");
			return;
		}

		if (!CallerMayReach(playerId, from, "warehouse transfer")) return;

		// Start the operation
		StartOperation("#OVT-Progress-TransferringToWarehouse");
		
		// This uses the existing warehouse transfer logic which is instant.
		// Lives on the real estate manager since P3 of the controller migration - it was a static on
		// OVT_Global, which is a locator, not a place for warehouse mutation.
		OVT_RealEstateManagerComponent realEstate = OVT_Global.GetRealEstate();
		if(realEstate) realEstate.TransferToWarehouse(fromId);
		
		// Send completion immediately as warehouse transfers are instant
		SendOperationComplete(1, 0);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Specialized method for FOB undeployment with container collection
	//! \param[in] deployedFOB The deployed FOB entity
	//! \param[in] mobileFOB The mobile FOB vehicle to transfer to
	void UndeployFOBWithCollection(IEntity deployedFOB, IEntity mobileFOB)
	{
		if (!deployedFOB || !mobileFOB) return;
		
		RplComponent fobRpl = RplComponent.Cast(deployedFOB.FindComponent(RplComponent));
		RplComponent vehicleRpl = RplComponent.Cast(mobileFOB.FindComponent(RplComponent));
		
		if (!fobRpl || !vehicleRpl) return;
		
		if(Replication.IsServer())
		{
			RpcAsk_UndeployFOB(fobRpl.Id(), vehicleRpl.Id());
		}else{
			Rpc(RpcAsk_UndeployFOB, fobRpl.Id(), vehicleRpl.Id());
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Server: sweep everything around a deployed FOB into the mobile FOB truck.
	//!
	//! Reached server-side from OVT_ResistanceFactionManager.UndeployFOB on the undeploying player's own
	//! controller, behind the FOB seam's own 15 m + PlayerMayUseVehicle gate on the deployed FOB - same
	//! relationship as RpcAsk_TransferStorage and the deploy path.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_UndeployFOB(RplId fobId, RplId vehicleId)
	{
		// Validate we're on server
		if (!Replication.IsServer()) return;

		int playerId = ResolveCallerPlayerId();
		if (playerId <= 0)
		{
			RejectTransfer(playerId, "undeploy FOB collection", "could not resolve the requesting player");
			return;
		}

		IEntity fob = GetEntityFromRplId(fobId);
		IEntity vehicle = GetEntityFromRplId(vehicleId);

		if (!fob || !vehicle)
		{
			SendOperationError("Invalid FOB or vehicle");
			return;
		}

		if (!CallerMayReach(playerId, fob, "undeploy FOB collection")) return;
		if (!CallerMayReach(playerId, vehicle, "undeploy FOB collection")) return;

		// Start the operation
		StartOperation("#OVT-Progress-UndeployingFOB");
		
		// Use container collection which handles placeables + the deployed FOB
		OVT_StorageOperationConfig config = new OVT_StorageOperationConfig(
			true,       // skipWeaponsOnGround
			true,       // deleteEmptyContainers
			100,        // itemsPerBatch
			300,        // batchDelayMs
			75.0,       // searchRadius
			3           // maxBatchesPerFrame
		);
		
		OVT_Global.GetInventory().CollectContainersToVehicle(fob.GetOrigin(), vehicle, config, GetCallback());
	}
	
	//------------------------------------------------------------------------------------------------
	//! Loot battlefield items (bodies and weapons) into a vehicle
	//! \param[in] vehicle Target vehicle to loot into
	//! \param[in] searchRadius Search radius for lootable items
	void LootBattlefield(IEntity vehicle, float searchRadius = 25.0)
	{
		if (!vehicle) return;
		
		RplComponent vehicleRpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		if (!vehicleRpl) return;
		
		if(Replication.IsServer())
		{
			RpcAsk_LootBattlefield(vehicleRpl.Id(), searchRadius);
		}else{
			Rpc(RpcAsk_LootBattlefield, vehicleRpl.Id(), searchRadius);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Server: sweep nearby bodies and dropped weapons into a vehicle.
	//!
	//! The radius is bounded for the same reason as RpcAsk_CollectContainers: it drives a world query on
	//! the server. The caller must be at the vehicle and entitled to use it - looting into somebody
	//! else's locked truck from across the map was one packet before.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LootBattlefield(RplId vehicleId, float searchRadius)
	{
		// Validate we're on server
		if (!Replication.IsServer()) return;

		int playerId = ResolveCallerPlayerId();
		if (playerId <= 0)
		{
			RejectTransfer(playerId, "loot battlefield", "could not resolve the requesting player");
			return;
		}

		if (searchRadius <= 0 || searchRadius > LOOT_MAX_RADIUS)
		{
			RejectTransfer(playerId, "loot battlefield", "search radius out of range");
			return;
		}

		IEntity vehicle = GetEntityFromRplId(vehicleId);
		if (!vehicle)
		{
			SendOperationError("Vehicle not found");
			return;
		}

		if (!CallerMayReach(playerId, vehicle, "loot battlefield")) return;

		// Start the operation
		StartOperation("#OVT-Progress-LootingBattlefield");
		
		// Use inventory manager for battlefield looting
		OVT_Global.GetInventory().LootBattlefieldIntoVehicle(vehicle, searchRadius, GetCallback());
	}
	
	//------------------------------------------------------------------------------------------------
	//! Check if the component is available and ready
	//! \return True if component can be used
	bool IsAvailable()
	{
		return !m_bIsRunning;
	}

	//------------------------------------------------------------------------------------------------
	//! Which player owns the controller this component sits on, on the SERVER.
	//!
	//! Delegates to the identity rule's ONE body on OVT_ControllerRequestComponent. This component
	//! cannot inherit that base (it needs the progress hierarchy and EnforceScript has no multiple
	//! inheritance), and a private copy is exactly what T1.6 deleted from five other components after
	//! proving the copies drift.
	//! \return Runtime player id, or -1 when it cannot be resolved (always a rejection).
	protected int ResolveCallerPlayerId()
	{
		return OVT_ControllerRequestComponent.ResolveOwningPlayerIdFor(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the caller's controlled character is within a radius of a position.
	//!
	//! A caller with no controlled character fails, which is correct for every request here: all six are
	//! user actions or vehicle menus performed by a body standing at a container.
	//! \param[in] playerId The caller.
	//! \param[in] pos The position to test against.
	//! \param[in] maxDistance The radius.
	//! \return True when the caller has a character and it is close enough.
	protected bool CallerIsWithin(int playerId, vector pos, float maxDistance)
	{
		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!character) return false;

		return vector.Distance(character.GetOrigin(), pos) <= maxDistance;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the caller may act on one end of a transfer: standing at it, and not locked out of it.
	//!
	//! Reports its own rejection so a handler reads as a flat ladder. The lock rule is the shared
	//! "unlocked, or locked and yours" one (PlayerMayUseVehicleFor) - deliberately the SAME rule the
	//! trunk sell, the upgrade and the FOB deploy paths use, so those four can never disagree about who
	//! may empty a given vehicle. An entity with no OVT_PlayerOwnerComponent (a plain crate) passes it.
	//! \param[in] playerId The caller.
	//! \param[in] entity One end of the transfer. Never null at any call site.
	//! \param[in] request Which request is being validated, for the log line.
	//! \return True when the caller may act on this entity.
	protected bool CallerMayReach(int playerId, IEntity entity, string request)
	{
		if (!entity)
		{
			RejectTransfer(playerId, request, "an end of the transfer no longer exists");
			return false;
		}

		if (!CallerIsWithin(playerId, entity.GetOrigin(), TRANSFER_MAX_DISTANCE))
		{
			RejectTransfer(playerId, request, "the caller is not standing at one end of the transfer");
			return false;
		}

		if (!OVT_ControllerRequestComponent.PlayerMayUseVehicleFor(playerId, entity))
		{
			RejectTransfer(playerId, request, "one end of the transfer is locked to another player");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Reports a refused request (quality bar Q9: a rejection is never indistinguishable from a dropped
	//! packet).
	//!
	//! TWO CHANNELS, AND THE SECOND ONE IS CONDITIONAL ON PURPOSE.
	//! - The server log line is unconditional and is the real evidence.
	//! - SendOperationError() exists for the server-side subscriber, not for the player: the resistance
	//!   manager latches m_pCurrentDeploymentSource/Target for the duration of a FOB operation and clears
	//!   it from m_OnOperationError, so a refusal that returned silently would wedge FOB deploys for
	//!   every player until the session ended. It is skipped while an operation IS running, because then
	//!   the error would cancel the bookkeeping of a DIFFERENT, legitimate transfer that is still in
	//!   flight. Nothing is drawn on the client either way: OVT_ProgressInfo only makes its widget
	//!   visible from OnProgressStart, and every rejection happens before StartOperation.
	//! \param[in] playerId The rejected player, or -1 when even that could not be resolved.
	//! \param[in] request Which request was rejected.
	//! \param[in] reason Why.
	protected void RejectTransfer(int playerId, string request, string reason)
	{
		Print(string.Format("[OVT_ContainerTransferComponent] Rejected %1 request from player %2: %3", request, playerId.ToString(), reason), LogLevel.WARNING);

		if (!m_bIsRunning)
			SendOperationError(reason);
	}

	// SendProgressUpdate / SendOperationComplete / SendOperationError live on
	// OVT_BaseServerProgressComponent - the callback above calls the inherited versions, which route
	// around the engine's "never loop an Rpc back to the sender" rule for a listen-server host
	// (BUG-090). The copies that used to live here sent RpcDo_UpdateProgress a fourth argument the
	// RPC does not declare, so progress updates never arrived for anyone.
}