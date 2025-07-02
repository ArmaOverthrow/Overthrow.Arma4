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
			m_Component.Rpc(m_Component.RpcDo_UpdateProgress, progress, currentItem, totalItems, operation);
	}
	
	override void OnComplete(int itemsTransferred, int itemsSkipped)
	{
		if (m_Component && Replication.IsServer())
			m_Component.Rpc(m_Component.RpcDo_OperationComplete, itemsTransferred, itemsSkipped);
	}
	
	override void OnError(string errorMessage)
	{
		if (m_Component && Replication.IsServer())
			m_Component.Rpc(m_Component.RpcDo_OperationError, errorMessage);
	}
}

//------------------------------------------------------------------------------------------------
//! Component that handles all container transfer operations with progress tracking.
//! Replaces container transfer functionality from OVT_PlayerCommsComponent.
class OVT_ContainerTransferComponent : OVT_BaseServerProgressComponent
{
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
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_TransferStorage(RplId fromId, RplId toId, bool deleteEmpty)
	{
		// Validate we're on server
		if (!Replication.IsServer()) return;
		
		// Start the operation
		StartOperation("Transferring items");
		
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
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_CollectContainers(vector pos, RplId vehicleId, float radius)
	{
		// Validate we're on server
		if (!Replication.IsServer()) return;
		
		IEntity vehicle = GetEntityFromRplId(vehicleId);
		if (!vehicle) 
		{
			Rpc(RpcDo_OperationError, "Vehicle not found");
			return;
		}
		
		// Start the operation
		StartOperation("Collecting containers");
		
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
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_TransferToWarehouse(RplId fromId)
	{
		// Validate we're on server
		if (!Replication.IsServer()) return;
		
		// Start the operation
		StartOperation("Transferring to warehouse");
		
		// This uses the existing warehouse transfer logic which is instant
		OVT_Global.TransferToWarehouse(fromId);
		
		// Send completion immediately as warehouse transfers are instant
		Rpc(RpcDo_OperationComplete, 1, 0);
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
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_UndeployFOB(RplId fobId, RplId vehicleId)
	{
		// Validate we're on server
		if (!Replication.IsServer()) return;
		
		IEntity fob = GetEntityFromRplId(fobId);
		IEntity vehicle = GetEntityFromRplId(vehicleId);
		
		if (!fob || !vehicle)
		{
			Rpc(RpcDo_OperationError, "Invalid FOB or vehicle");
			return;
		}
		
		// Start the operation
		StartOperation("Undeploying FOB");
		
		// Use specialized FOB collection config
		OVT_StorageOperationConfig config = new OVT_StorageOperationConfig(
			true,       // skipWeaponsOnGround
			true,       // deleteEmptyContainers
			100,        // itemsPerBatch
			300,        // batchDelayMs
			75.0,       // searchRadius
			3           // maxBatchesPerFrame
		);
		
		// Use inventory manager with our callback
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
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LootBattlefield(RplId vehicleId, float searchRadius)
	{
		// Validate we're on server
		if (!Replication.IsServer()) return;
		
		IEntity vehicle = GetEntityFromRplId(vehicleId);
		if (!vehicle) 
		{
			Rpc(RpcDo_OperationError, "Vehicle not found");
			return;
		}
		
		// Start the operation
		StartOperation("Looting battlefield");
		
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
}