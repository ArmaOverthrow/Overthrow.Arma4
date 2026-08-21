//------------------------------------------------------------------------------------------------
//! Buying resources at the port moves MONEY and RESOURCES, and moves them by the same numbers the
//! screen quoted.
//!
//! WHAT IS AT STAKE. The port is the only source of resources in the game: nothing is produced,
//! salvaged or found. If the import path debits the wallet without filling the truck - or fills the
//! truck at a price nobody was charged - the whole logistics loop is either free or impossible, and
//! neither shows up as an error. The two halves are asserted TOGETHER for that reason: a case that
//! only checked the ledger would pass on a free purchase.
//!
//! THE LIVE PRICE, NOT THE BASE PRICE. The expected debit is qty x OVT_ResourceManagerComponent.
//! GetPrice(), which is the drifted, difficulty-multiplied number the port screen shows. Reading the
//! config base here would make the case agree with a server that charged the wrong thing.
//!
//! IT DRIVES THE REAL WIRE. RequestTransferBegin / RequestTransferLine / RequestTransferCommit is
//! exactly what OVT_ResourceTransferContext and OVT_PortContext send; on a listen host the whole fan
//! runs synchronously inside the ask, so the assertions can be made on the next step with no polling.
//! Every server gate the fan passes - the caller's identity, the two-ended port proximity rule, the
//! use radius, the importable flag, the illegal gate, whole-cart atomicity and the money check - is
//! exercised on the way through, which is why this belongs in a started campaign and cannot be faked
//! one tier down.
//!
//! WHY THE PLAYER IS MOVED. AtAPort() requires BOTH the caller and the holder within the port radius,
//! and the test world's one port is ~150 m from where the campaign start puts the player. The
//! character is teleported to the port and put back before the case returns; the case that reads the
//! player's position (Warehouse_BuiltRegistersLikeAPurchasedOne) sorts after this one and would
//! notice if it were not.
//!
//! RESTORES WHAT IT SPENDS. The money is refunded through OVT_EconomyManagerComponent.AddPlayerMoney
//! and the truck is deleted, so the next case starts from the state this one found. The truck is
//! spawned rather than bought, so it belongs to no vehicle registry and deleting it untracks nothing.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_CampaignSuite, timeoutS: 60)]
class OVT_TEST_Campaign_ResourcePort_PurchaseMovesMoneyAndResources : SCR_AutotestCaseBase
{
	//! The transport truck: the store the Wheeled_Truck_Base delta puts on every truck, at the
	//! transport override's capacity.
	static const ResourceName TRANSPORT_TRUCK_PREFAB = "{F1FBD0972FA5FE09}Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et";

	//! Units bought. Small enough to fit any truck and to be affordable at any drifted price on the
	//! TestWorld preset's starting cash.
	static const int PURCHASE_QUANTITY = 5;

	//! Where the caller stands relative to the port, and where the truck is put. Both well inside
	//! OVT_ResourceRequestComponent's 30 m port radius and its 30 m use radius.
	static const vector CALLER_OFFSET = "2 0 2";
	static const vector TRUCK_OFFSET = "6 0 6";

	//! Metres the teleported character may still be from the stand position before the case gives up.
	static const float ARRIVAL_TOLERANCE = 12;

	//! Frame polls allowed for the teleport to land. A bound on the explanation, not a retry budget.
	static const int MAX_ARRIVAL_POLLS = 300;

	static const int PHASE_SETUP = 0;
	static const int PHASE_AWAIT_ARRIVAL = 1;
	static const int PHASE_PURCHASE = 2;

	protected int m_iPhase;
	protected int m_iArrivalPolls;

	protected int m_iPlayerId;
	protected string m_sPersId;
	protected vector m_vOriginalPlayerPos;
	protected vector m_vStandPos;
	protected IEntity m_Truck;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == PHASE_SETUP)
			return Setup();

		if (m_iPhase == PHASE_AWAIT_ARRIVAL)
			return AwaitArrival();

		return Purchase();
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the case is finished (a failure); false to advance.
	protected bool Setup()
	{
		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players)
		{
			SetFailure("OVT_Global.GetPlayers() is null - see OVT_TEST_Init_Globals_ManagersResolve");
			return true;
		}

		array<int> connected = {};
		GetGame().GetPlayerManager().GetPlayers(connected);
		if (connected.IsEmpty())
		{
			SetFailure("No player is connected - the autotest client normally spawns a local player (playerId 1), and a port purchase needs a caller with a wallet and a body");
			return true;
		}

		m_iPlayerId = connected[0];
		m_sPersId = players.GetPersistentIDFromPlayerID(m_iPlayerId);
		if (m_sPersId == "")
		{
			SetFailure("The player manager has no persistent ID for playerId %1, so SetupPlayer never ran and the player has no wallet to debit", m_iPlayerId.ToString());
			return true;
		}

		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(m_iPlayerId);
		if (!character)
		{
			SetFailure("Player %1 controls no entity, so neither the port proximity rule nor the use radius can be satisfied", m_iPlayerId.ToString());
			return true;
		}

		vector portPos;
		string diagnostic = ResolvePortPosition(portPos);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		m_vOriginalPlayerPos = character.GetOrigin();

		m_vStandPos = SnapToGround(portPos + CALLER_OFFSET);
		vector truckPos = SnapToGround(portPos + TRUCK_OFFSET);

		m_Truck = OVT_Global.SpawnEntityPrefab(TRANSPORT_TRUCK_PREFAB, truckPos);
		if (!m_Truck)
		{
			SetFailure("SpawnEntityPrefab() produced no transport truck from %1 at the port - the prefab or its .meta GUID no longer resolves", TRANSPORT_TRUCK_PREFAB);
			return true;
		}

		if (!OVT_ResourceUtils.GetStore(m_Truck))
		{
			SetFailure("The spawned transport truck has no OVT_ResourceStoreComponent, so the same-GUID Prefabs/Vehicles/Core/Wheeled_Truck_Base.et delta is not being applied and no truck in the game can carry resources");
			Cleanup();
			return true;
		}

		if (!SCR_Global.TeleportPlayer(m_iPlayerId, m_vStandPos))
		{
			SetFailure("SCR_Global.TeleportPlayer() refused to move the caller to the port at %1", m_vStandPos.ToString());
			Cleanup();
			return true;
		}

		m_iPhase = PHASE_AWAIT_ARRIVAL;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Precondition, not the assertion: the caller has to actually be standing at the port before the
	//! purchase means anything.
	//! \return True when the case is finished (a failure); false to keep waiting or advance.
	protected bool AwaitArrival()
	{
		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(m_iPlayerId);
		if (!character)
		{
			SetFailure("Player %1 lost its character while being moved to the port", m_iPlayerId.ToString());
			Cleanup();
			return true;
		}

		if (vector.Distance(character.GetOrigin(), m_vStandPos) <= ARRIVAL_TOLERANCE)
		{
			m_iPhase = PHASE_PURCHASE;
			return false;
		}

		m_iArrivalPolls += 1;
		if (m_iArrivalPolls > MAX_ARRIVAL_POLLS)
		{
			SetFailure(string.Format("The caller never arrived at the port: after %1 poll(s) the character is %2 m from %3, which is outside the %4 m tolerance. The purchase would be refused for distance and the case would report the wrong failure.",
				m_iArrivalPolls.ToString(), vector.Distance(character.GetOrigin(), m_vStandPos).ToString(), m_vStandPos.ToString(), ARRIVAL_TOLERANCE.ToString()));
			Cleanup();
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Always true - the case ends here either way.
	protected bool Purchase()
	{
		OVT_ResourceManagerComponent resources = OVT_Global.GetResources();
		if (!resources)
		{
			SetFailure("OVT_Global.GetResources() is null, so there is no price to charge and nothing to buy");
			Cleanup();
			return true;
		}

		OVT_ResourceDefs defs = resources.GetDefs();
		if (!defs || defs.Count() == 0)
		{
			SetFailure("The resource catalogue is empty, so the port has nothing to sell and this case would assert nothing");
			Cleanup();
			return true;
		}

		int resIndex = -1;
		for (int i = 0; i < defs.Count(); i++)
		{
			if (defs.IsImportable(i) && !defs.IsIllegal(i))
			{
				resIndex = i;
				break;
			}
		}

		if (resIndex == -1)
		{
			SetFailure("resources.conf lists no legal importable resource, so nothing can be bought at a port at all");
			Cleanup();
			return true;
		}

		string resourceId = defs.IdAt(resIndex);
		int unitPrice = resources.GetPrice(resIndex);
		if (unitPrice <= 0)
		{
			SetFailure("The live price of '%1' is %2. A resource that costs nothing makes the money half of this case meaningless.", resourceId, unitPrice.ToString());
			Cleanup();
			return true;
		}

		int expectedSpend = unitPrice * PURCHASE_QUANTITY;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!economy)
		{
			SetFailure("OVT_Global.GetEconomy() is null - see OVT_TEST_Init_Globals_ManagersResolve");
			Cleanup();
			return true;
		}

		int moneyBefore = economy.GetPlayerMoney(m_sPersId);
		if (moneyBefore < expectedSpend)
		{
			SetFailure("The player holds %1 and the purchase costs %2. The TestWorld preset starts them at 100000, so an earlier case has spent it and this one would be refused for funds.",
				moneyBefore.ToString(), expectedSpend.ToString());
			Cleanup();
			return true;
		}

		OVT_ResourceStoreComponent store = OVT_ResourceUtils.GetStore(m_Truck);
		if (!store || !store.GetLedger())
		{
			SetFailure("The truck's resource store lost its ledger between the setup step and the purchase");
			Cleanup();
			return true;
		}

		int heldBefore = store.GetLedger().Count(resourceId);

		OVT_ResourceRequestComponent request = OVT_ControllerComponent<OVT_ResourceRequestComponent>.Get();
		if (!request)
		{
			SetFailure("OVT_ControllerComponent<OVT_ResourceRequestComponent>.Get() returned null, so no resource request can be sent - see OVT_TEST_Init_Controller_ResourceRequestResolves");
			Cleanup();
			return true;
		}

		RplId truckId = OVT_ResourceUtils.GetHolderId(m_Truck);
		if (!truckId.IsValid())
		{
			SetFailure("The spawned truck has no valid RplId, so it cannot be named on the wire");
			Cleanup();
			return true;
		}

		// Both slots always carry a valid holder; PORT_IMPORT reads only the destination.
		int seq = request.RequestTransferBegin(truckId, truckId, EOVT_ResourceOp.PORT_IMPORT, 1);
		if (seq == OVT_ResourceRequestComponent.SEQ_NONE)
		{
			SetFailure("RequestTransferBegin() sent nothing. The component refuses when it is not on the LOCAL player's controller, which means this case is talking to somebody else's seam.");
			Cleanup();
			return true;
		}

		request.RequestTransferLine(seq, 0, resIndex, PURCHASE_QUANTITY);
		request.RequestTransferCommit(seq, 1);

		// A listen host runs the whole reply fan synchronously inside the ask, so both sides have
		// already moved by here.
		int moneyAfter = economy.GetPlayerMoney(m_sPersId);
		int heldAfter = store.GetLedger().Count(resourceId);

		if (heldAfter == heldBefore && moneyAfter == moneyBefore)
		{
			SetFailure(string.Format("The port purchase of %1 x '%2' did nothing: the truck still holds %3 and the wallet still reads %4. The checkout was refused - the caller or the truck is outside the port radius, the resource is not importable, or the server gate rejected the cart.",
				PURCHASE_QUANTITY.ToString(), resourceId, heldBefore.ToString(), moneyBefore.ToString()));
			Cleanup();
			return true;
		}

		if (heldAfter != heldBefore + PURCHASE_QUANTITY)
		{
			SetFailure("The truck came out of the purchase holding %1 of '%2', expected %3. Nothing clamps a resource cart - it is accepted whole or refused whole - so a partial figure means the commit path is moving something other than what it charged for.",
				heldAfter.ToString(), resourceId, (heldBefore + PURCHASE_QUANTITY).ToString());
			Cleanup();
			return true;
		}

		if (moneyAfter != moneyBefore - expectedSpend)
		{
			SetFailure(string.Format("The purchase moved %1 of '%2' onto the truck but took %3 from the wallet, expected the LIVE price of %4 each, i.e. %5. The price charged must be the price shown.",
				PURCHASE_QUANTITY.ToString(), resourceId, (moneyBefore - moneyAfter).ToString(), unitPrice.ToString(), expectedSpend.ToString()));
			Cleanup();
			return true;
		}

		string packed = store.GetPackedContents();
		if (packed == "")
		{
			SetFailure("The truck's ledger holds the purchase but its REPLICATED contents are empty, so PublishContents() never ran and no other machine can see the cargo");
			Cleanup();
			return true;
		}

		Print(string.Format("Port purchase moved money AND resources: %1 x '%2' at the live price of %3 each cost exactly %4 and arrived on the truck",
			PURCHASE_QUANTITY.ToString(), resourceId, unitPrice.ToString(), expectedSpend.ToString()));

		economy.AddPlayerMoney(m_iPlayerId, expectedSpend);
		Cleanup();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The port's world position.
	//! \param[out] pos Receives it; untouched on failure.
	//! \return An empty string on success, otherwise the sentence to fail with.
	protected string ResolvePortPosition(out vector pos)
	{
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!economy)
			return "OVT_Global.GetEconomy() is null, so there are no registered ports";

		array<RplId> ports = economy.GetAllPorts();
		if (!ports || ports.IsEmpty())
			return "No port is registered in this world. Worlds/MP/OVT_Campaign_Test_Layers/default.layer is supposed to place one - without it there is nowhere to buy resources.";

		RplComponent rpl = RplComponent.Cast(Replication.FindItem(ports[0]));
		if (!rpl || !rpl.GetEntity())
			return "The first registered port's RplId resolves to no entity";

		pos = rpl.GetEntity().GetOrigin();
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] pos A position whose height may be anything.
	//! \return The same position with its height on the terrain.
	protected vector SnapToGround(vector pos)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return pos;

		vector snapped = pos;
		snapped[1] = world.GetSurfaceY(snapped[0], snapped[2]);
		return snapped;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the world back: the truck goes, the player goes home. Safe to call from any step.
	protected void Cleanup()
	{
		if (m_Truck)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(m_Truck);
			m_Truck = null;
		}

		if (m_vOriginalPlayerPos != vector.Zero)
			SCR_Global.TeleportPlayer(m_iPlayerId, m_vOriginalPlayerPos);
	}
}
