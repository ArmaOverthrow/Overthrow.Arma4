//------------------------------------------------------------------------------------------------
//! Money on the vanilla fuel pump: charges the refuelling player per litre actually delivered, and
//! refuses through the vanilla seam when they cannot pay.
//!
//! WHY THIS CLASS EXISTS AT ALL. Reforger already ships a complete, working fuel economy that
//! Overthrow never wired up - world pumps hold 50,000 L and dispense for free through one looped
//! user action driving one server-side transfer routine. This adds a price to it and nothing else.
//! Every behaviour a player can see - refuel speed, the crewman speed bonus, the audio, the
//! "someone is refuelling your vehicle" notifications, tank-full handling, the fill-percentage
//! readout - belongs to vanilla and stays untouched: all three hooks below guard first, delegate to
//! super, and only then add.
//!
//! ================== WHERE THE MONEY IS BORN (and why there is no RPC) ==================
//!
//!   player holds "Refuel" on a vehicle          (the action lives on the RECEIVING vehicle)
//!         v
//!   SCR_BaseUseSupportStationAction.PerformContinuousAction        [client + server]
//!         v
//!   PerformAction                                                  [SERVER ONLY, m_bIsMaster]
//!         +-- CanBePerformedScript -> GetClosestValidSupportStation
//!         |            v
//!         |       IsValid(...)                        <-- (1) THE CAN'T-PAY GATE
//!         v
//!   OnExecutedServer(owner, USER, action)              <-- (2) stash the paying user
//!         v
//!   OnFuelAddedToVehicleServer(litresDelivered, node)  <-- (3) TAKE THE MONEY
//!
//! IsValid runs on BOTH machines - on the client every 500 ms while the action is hovered (that is
//! what greys it out and picks the reason string) and again on the server immediately before every
//! 0.5 s execution, which is the authority. So the client answers "can I afford it?" from its own
//! already-replicated money and the server answers the same question authoritatively half a second
//! later. No message is sent between them and none is needed. THE DAY THIS FILE GAINS A REMOTE
//! PROCEDURE CALL, THE DESIGN HAS BEEN ABANDONED.
//!
//! ================== TWO ORDERING RULES THAT ARE NOT STYLE ==================
//!
//! 1. THE CHARGE RUNS BEFORE super IN OnFuelAddedToVehicleServer. A station with no
//!    SCR_FuelManagerComponent falls back to m_BackupMaxFlowCapacity and calls that method with a
//!    NULL node, where vanilla early-returns before doing anything. The fuel was still delivered.
//!    Charging first, from the litres argument alone and never from the node, is what keeps that
//!    path paid (implementation.md R2). Eden's pumps do have fuel managers, which is exactly why
//!    getting this backwards would go unnoticed.
//! 2. THE GATE RUNS AFTER super IN IsValid. Range, faction, destroyed, disabled, moving and
//!    no-fuel-to-give are all more useful things to tell a player than "you're broke", and letting
//!    super answer first means they win.
//!
//! Money integrity: the balance is read before every take, because TakePlayerMoneyPersistentId
//! clamps at zero and would otherwise report an overdraw as a successful transaction. A player is
//! never charged for litres that did not arrive - the charge happens only after the transfer - and
//! never charged twice for the same litres.
//!
//! ================== WHY THE "YOU RAN OUT" TOAST HAS TWO TRIGGERS ==================
//!
//! The obvious trigger - "the take had to be clamped to the balance" - is in practice a RACE, not
//! the normal way a player runs dry, and relying on it alone would have left the ordinary case
//! silent. The gate is deliberately conservative: it demands ceil(maxFlow x price) up front while
//! the charge only ever bills for litres that actually arrived, and the ledger's carry is always
//! under a dollar, so a tick that passed the gate can be shown never to overdraw. What really
//! happens is that the player is refused BEFORE the tick that would have broken them - which stops
//! the fuel correctly but says nothing.
//!
//! So the flag is raised on either of:
//!   1. CLAMP - the balance moved between the gate and the charge (another debit landing
//!      concurrently: a purchase, a transfer, a fine). Rare, and mostly a multiplayer phenomenon.
//!   2. RUN DRY - after this tick's take, what is left can no longer fund another tick, i.e.
//!      exactly the condition the NEXT tick's gate will refuse on. This is the ordinary case and
//!      the one Primary Goal 2 / DoD F4 are about.
//! Both are evaluated against the same tick-flow basis the gate uses, so the toast fires precisely
//! when the refuel is about to stop for lack of money and never when it stops for any other reason
//! - releasing the action early while still solvent raises nothing at all.
//!
//! Degradation (implementation.md D9/R9): this class loads everywhere the mod loads, including
//! vanilla Conflict, the Game Master, autotest worlds and Workbench. No economy manager, no config,
//! a price of 0, a free source or a user that is not a player, and every hook falls through to pure
//! vanilla behaviour.
//------------------------------------------------------------------------------------------------
modded class SCR_FuelSupportStationComponent : SCR_BaseSupportStationComponent
{
	//! Localization tag of the one-shot "your money ran out mid-refuel" notification. Its preset lives
	//! in Configs/overthrowBroadcastMessages.conf.
	protected const string NOTIFICATION_CANNOT_AFFORD_FUEL = "CannotAffordFuel";

	//! The character currently being served, stashed by OnExecutedServer purely so the charge inside
	//! OnFuelAddedToVehicleServer knows whose account to debit - vanilla passes only litres and a node
	//! down that far. Set immediately before super and cleared immediately after, so it is non-null
	//! only for the duration of one server-side execution. That window is also this file's implicit
	//! server guard: OnExecutedServer is reached only from PerformAction under m_bIsMaster, so a
	//! charge can never run anywhere else - there is nobody stashed.
	protected IEntity m_PendingChargeUser;

	//! Runtime player id of the same character, resolved once up front so the refusal notification
	//! does not depend on the character still being alive and possessed after super returns.
	protected int m_iPendingNotifyPlayerId = -1;

	//! This execution's action duration in seconds (0.5 s for the vanilla refuel loop), stashed for
	//! the same reason as the user: the charge has to size the NEXT tick the way the gate does, and
	//! vanilla does not hand the action down to OnFuelAddedToVehicleServer. Left at 0 outside an
	//! execution, which makes any stray call compute a zero tick and raise nothing.
	protected float m_fPendingActionDuration;

	//! Sub-dollar carry for this pump, keyed by persistent id. Created on first paid delivery - most
	//! stations in a session (every fuel truck, every jerrycan, every depot) never need one.
	//! Deliberately never settled and never persisted; see OVT_FuelChargeLedger's header.
	protected ref OVT_FuelChargeLedger m_Ledger;

	//! Raised by the charge when this player's refuel is about to stop for lack of money - by clamp
	//! or by running dry, see the class header - and consumed once by OnExecutedServer.
	//!
	//! Reset at the START of every execution as well as cleared on consumption, so it is strictly
	//! per-execution: it cannot leak from one player to the next at a shared pump, and a player who
	//! earns money and comes back is told again if they run dry again. It stays a one-shot within a
	//! session without any timer because the next tick's IsValid refuses outright - a player who
	//! cannot pay is greyed out before OnExecutedServer is ever reached again.
	protected bool m_bNotifyOutOfMoney;

	//------------------------------------------------------------------------------------------------
	//! Vanilla validity first, then affordability.
	//!
	//! Returns super's answer unchanged when it fails, so every reason vanilla can give - not in
	//! range, wrong faction, destroyed, disabled, moving, no fuel to give - reaches the player ahead
	//! of ours. Only a station that is otherwise ready to pour is allowed to refuse over money.
	//! \param[in] actionOwner The entity being refuelled (the action manager's owner).
	//! \param[in] actionUser The character performing the refuel.
	//! \param[in] action The refuel action driving this check.
	//! \param[in] actionPosition World position of the action point.
	//! \param[out] reasonInvalid Why the station refused, when it did.
	//! \param[out] supplyAmount Vanilla's supply cost/gain for the action.
	//! \return True when this station may serve this user right now.
	override bool IsValid(IEntity actionOwner, IEntity actionUser, SCR_BaseUseSupportStationAction action, vector actionPosition, out ESupportStationReasonInvalid reasonInvalid, out int supplyAmount)
	{
		if(!super.IsValid(actionOwner, actionUser, action, actionPosition, reasonInvalid, supplyAmount))
			return false;

		if(!CanAffordNextTick(actionUser, action))
		{
			reasonInvalid = ESupportStationReasonInvalid.OVT_CANNOT_AFFORD_FUEL;
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server-side execution of one refuel tick, wrapped so the charge knows who to bill.
	//!
	//! Stashes what the charge will need (who to bill, and how long a tick is), runs the whole of
	//! vanilla's transfer - which calls back into OnFuelAddedToVehicleServer, where the money actually
	//! moves - then tells the player, at most once, if that was their last affordable tick. SERVER
	//! ONLY: reached only from SCR_BaseUseSupportStationAction.PerformAction under m_bIsMaster, which
	//! is true on a dedicated server and on a listen host alike.
	//! \param[in] actionOwner The entity being refuelled.
	//! \param[in] actionUser The character performing the refuel.
	//! \param[in] action The refuel action driving this execution.
	override void OnExecutedServer(notnull IEntity actionOwner, notnull IEntity actionUser, notnull SCR_BaseUseSupportStationAction action)
	{
		m_PendingChargeUser = actionUser;
		m_iPendingNotifyPlayerId = SCR_PossessingManagerComponent.GetPlayerIdFromControlledEntity(actionUser);
		m_fPendingActionDuration = Math.AbsFloat(action.GetActionDuration());
		m_bNotifyOutOfMoney = false;

		super.OnExecutedServer(actionOwner, actionUser, action);

		if(m_bNotifyOutOfMoney)
		{
			NotifyOutOfMoney();
			m_bNotifyOutOfMoney = false;
		}

		m_PendingChargeUser = null;
		m_iPendingNotifyPlayerId = -1;
		m_fPendingActionDuration = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Fuel has just been delivered - take the money for it, then let vanilla drain the provider node.
	//!
	//! CHARGE FIRST, super SECOND, and the charge reads only fuelAmount. Vanilla early-returns here
	//! when the node is null (the m_BackupMaxFlowCapacity path), and fuel delivered on that path must
	//! still be paid for (implementation.md R2).
	//! \param[in] fuelAmount Litres actually delivered by this tick, computed by vanilla as
	//!            maxFlowCapacityOut - fuelToAdded. Never negative.
	//! \param[in] nodeUsedForRefuel The provider node vanilla will drain, or null on the backup path.
	protected override void OnFuelAddedToVehicleServer(float fuelAmount, SCR_FuelNode nodeUsedForRefuel)
	{
		ChargeForDeliveredFuel(fuelAmount);

		super.OnFuelAddedToVehicleServer(fuelAmount, nodeUsedForRefuel);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the player can pay for one more full-rate tick from this station.
	//!
	//! Deliberately conservative: maxFlow is the STATION's output for the tick clamped to the
	//! provider node's remaining fuel, an upper bound on what will actually be delivered (the
	//! receiving vehicle's own flow-in cap may be lower). So a player can be refused holding a few
	//! dollars they cannot spend. That is expected behaviour, not a defect (implementation.md R10) -
	//! the alternative, charging in advance, would take money for fuel that never arrived.
	//!
	//! Every guard answers TRUE, because "we cannot tell" must never block a refuel that vanilla
	//! would have allowed. Ordered cheapest-first so a non-Overthrow world pays almost nothing for
	//! this: the price lookup alone rejects it.
	//!
	//! CLIENT/SERVER SPLIT. On the server (and on a listen host, where Replication.IsServer is also
	//! true) the authoritative wallet is read by persistent id. On a client the question is asked of
	//! the LOCAL player's own money, which is the only balance a client is guaranteed to have
	//! streamed - and the only one it can be asked about, since PerformContinuousAction refuses to
	//! run for any user but the local controlled entity on a non-master machine.
	//! \param[in] actionUser The character performing the refuel.
	//! \param[in] action The refuel action, whose duration sizes the tick.
	//! \return True when the refuel may proceed.
	protected bool CanAffordNextTick(IEntity actionUser, SCR_BaseUseSupportStationAction action)
	{
		if(!actionUser || !action) return true;

		float price = OVT_FuelUtils.GetFuelPricePerLitre();
		if(price <= 0) return true;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if(!config) return true;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(!economy) return true;

		if(OVT_FuelUtils.IsFreeFuelSource(this)) return true;

		string persId = OVT_FuelUtils.ResolvePersistentId(actionUser);
		if(persId == "") return true;

		// A tick that can deliver nothing costs nothing - vanilla's own maxFlowCapacityOut <= 0 safety
		// returns before any fuel moves, so there is nothing here to refuse.
		int tickCost = GetTickCost(price, Math.AbsFloat(action.GetActionDuration()));
		if(tickCost <= 0) return true;

		if(Replication.IsServer())
			return economy.PlayerHasMoney(persId, tickCost);

		if(!economy.LocalPlayerHasMoney(tickCost)) return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Take payment for fuel that has ALREADY been delivered. Server only, by construction.
	//!
	//! The exact cost is accrued against the payer's persistent id and only whole dollars are taken;
	//! the sub-dollar remainder waits at this pump (OVT_FuelChargeLedger, implementation.md D8).
	//!
	//! The balance is read before the take and the amount clamped to it. TakePlayerMoneyPersistentId
	//! floors the account at 0 on its own, so without this read an overdraw would look exactly like a
	//! successful charge and the player would silently receive fuel they never paid for. When the
	//! clamp bites, the payer's carry is dropped as well - they paid everything they had for the fuel
	//! that arrived, and carrying the shortfall forward would charge them for it a second time.
	//!
	//! It also decides whether the player needs telling that this was their last tick - both triggers
	//! are described in the class header. The run-dry test is deliberately the exact negation of the
	//! gate's own test (balance >= tickCost), so the toast fires on precisely the ticks after which
	//! the gate will refuse, and the two can never disagree about what "cannot afford" means.
	//! \param[in] litres Litres delivered by this tick.
	protected void ChargeForDeliveredFuel(float litres)
	{
		if(litres <= 0) return;
		if(!m_PendingChargeUser) return;

		float price = OVT_FuelUtils.GetFuelPricePerLitre();
		if(price <= 0) return;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if(!config) return;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(!economy) return;

		if(OVT_FuelUtils.IsFreeFuelSource(this)) return;

		string persId = OVT_FuelUtils.ResolvePersistentId(m_PendingChargeUser);
		if(persId == "") return;

		if(!m_Ledger) m_Ledger = new OVT_FuelChargeLedger();

		int owed = m_Ledger.Accrue(persId, litres, price);
		int balance = economy.GetPlayerMoney(persId);

		// Trigger 1 - the balance moved between the gate and here. Rare; see the class header.
		if(owed > balance)
		{
			owed = balance;
			m_Ledger.Clear(persId);
			m_bNotifyOutOfMoney = true;
		}

		if(owed > 0)
			economy.TakePlayerMoneyPersistentId(persId, owed);

		if(m_bNotifyOutOfMoney) return;

		// Trigger 2 - what is left cannot fund another tick, so the next gate will refuse. This is the
		// ordinary way a refuel runs dry and, until it was added, the way it ended in silence.
		int nextTickCost = GetTickCost(price, m_fPendingActionDuration);
		if(nextTickCost <= 0) return;

		if(balance - owed < nextTickCost)
			m_bNotifyOutOfMoney = true;
	}

	//------------------------------------------------------------------------------------------------
	//! What one tick from this station costs right now.
	//!
	//! THE SINGLE SOURCE FOR BOTH SIDES OF THE "CAN THEY AFFORD IT" QUESTION: the gate asks it about
	//! the tick that is about to happen, the charge asks it about the tick after the one that just
	//! did. Sharing the call is the point - a gate that refused on one number while the toast fired
	//! on another would tell a player their money ran out while the pump kept pouring, or the
	//! reverse. They differ only in where the duration comes from, which is why it is a parameter:
	//! the gate reads it live off the action (it also runs on clients, which have no execution in
	//! flight), the charge reads the value stashed from the same action in the same execution.
	//!
	//! Called by the charge a hair BEFORE vanilla drains the provider node, so at a pump down to its
	//! last couple of litres the answer is a fraction high. That window is one tick at the end of a
	//! 50,000 L pump's life, where vanilla is about to report NO_FUEL_TO_GIVE anyway, and erring high
	//! only warns the player a tick early rather than a tick late.
	//! \param[in] price Dollars per litre, already resolved.
	//! \param[in] duration The action's tick length in seconds, always positive.
	//! \return Whole dollars needed for one tick. 0 when the station can deliver nothing, which is a
	//!         fuel problem and not a money one - say nothing and let vanilla explain it.
	protected int GetTickCost(float price, float duration)
	{
		float maxFlow;
		SCR_FuelNode providerNode;
		GetFuelNodeInfo(maxFlow, providerNode, duration);

		if(maxFlow <= 0) return 0;

		return OVT_FuelPricing.EstimateTickCost(maxFlow, price);
	}

	//------------------------------------------------------------------------------------------------
	//! Tell the player once that their money ran out mid-refuel.
	//!
	//! Targeted at the paying player only - a pump is a public place and the rest of the server has
	//! no business knowing. Fires at most once per execution, and the next tick's IsValid refuses
	//! outright, so it cannot repeat every 0.5 s.
	//!
	//! One acknowledged imprecision: if the receiving tank fills on the very tick that empties the
	//! wallet, the player is told they ran out of money when what actually ended the refuel was the
	//! full tank. Both statements are true, the money one is the more useful, and distinguishing them
	//! would mean reaching into state vanilla computes after this hook has already returned.
	protected void NotifyOutOfMoney()
	{
		if(m_iPendingNotifyPlayerId < 1) return;

		OVT_NotificationManagerComponent notify = OVT_Global.GetNotify();
		if(!notify) return;

		notify.SendTextNotification(NOTIFICATION_CANNOT_AFFORD_FUEL, m_iPendingNotifyPlayerId);
	}
}
