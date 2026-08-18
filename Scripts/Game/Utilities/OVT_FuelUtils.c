//------------------------------------------------------------------------------------------------
//! The fuel-source rules: what a litre costs here, and whether "here" charges for it at all.
//!
//! Three very different callers need the SAME answers and must never disagree:
//!   - the modded SCR_FuelSupportStationComponent's affordability gate, running on the client every
//!     500 ms while a refuel action is hovered (it decides whether the action greys out);
//!   - that same gate running on the server immediately before every 0.5 s refuel tick, and the
//!     charge that follows it (it decides whether money moves);
//!   - resistance/high-command, later, asking "is there a fuel source here and what does it cost?"
//!     without knowing anything about vanilla support stations.
//! Putting the rules in one static class is what keeps the client's offer and the server's authority
//! from drifting apart - and there is no replication here at all, because every input (the difficulty
//! price, the station's prefab data, the player's own money) is already on both machines.
//!
//! FREE IS DECIDED STRUCTURALLY, NEVER BY A WHITELIST (implementation.md D5). A whitelist of fuel
//! prefabs would rot on the next Reforger update and could never cover a mod-added fuel vehicle.
//! Instead: a source the resistance built and already paid for carries OVT_FuelSourceComponent, a
//! source that drove here is on a vehicle, and a source you are holding has no range. Everything
//! else is a static pump and charges (D2).
//!
//! DEGRADES TO "FREE, ALWAYS" (implementation.md D9/R9, task 2.8). The modded classes that call this
//! load everywhere the mod loads - vanilla Conflict, the Game Master, autotest worlds, Workbench.
//! Every lookup here answers 0 / free when the config, the difficulty or the station is missing, so
//! the fuel stack behaves exactly as BI shipped it wherever Overthrow is not actually running.
//!
//! THE RANGE QUERIES GO THROUGH THE VANILLA REGISTRY, NOT A PARALLEL LIST. Every range-bearing
//! station adds and removes itself from SCR_SupportStationManagerComponent as it is created,
//! destroyed and repaired, so asking it is the only way to get an answer that is still true after a
//! pump has been blown up. The registry's own accessor is protected; the modded
//! SCR_SupportStationManagerComponent.OVT_GetSupportStationsOfType opens it and hands back a copy.
//! Held jerrycans are not registered (m_fRange -1) and therefore never appear in these results,
//! which is correct: they are not a place you can drive to.
//!
//! THE FILL HALF (amendment A1) ADDS A FOURTH CALLER WITH THE SAME PROBLEM. OVT_FillFuelAction draws
//! a price and a set of gates on the client; OVT_FuelRequestComponent re-derives every one of them on
//! the server before it moves fuel or takes money. FindBestFillSource, GetRefuelableCapacity and
//! GetProvidableFuel are shared by both for exactly the reason the pricing rules are: two
//! implementations of "which pump is this and how much has it got" would eventually offer a fill the
//! server refuses, or price one source and drain another. Everything here is read-only - the transfer
//! itself is server-side mutation and lives with the authority, not in this class.
//!
//! ONE FILL TOUCHES ONE MANAGER (amendment A2). GetOwnFuelManager is the scoping rule and its
//! doc comment is worth reading before changing anything in this area: a fill fills the tank whose
//! action you are holding and nothing else, which is exactly how vanilla scopes its own refuel.
//! GetPrimaryFuelTankId / FindFuelManagerByTankId are how that scope survives the trip to the server,
//! since a truck's cargo-tank part is a real entity but carries no RplComponent to name it with.
//------------------------------------------------------------------------------------------------
class OVT_FuelUtils
{
	//------------------------------------------------------------------------------------------------
	//! What one litre costs at a PAID source, at the current difficulty.
	//!
	//! Null-guarded on both steps for the same reason OVT_Global.GetDifficulty is: the config
	//! component does not exist outside an Overthrow game mode, and on a joining client m_Difficulty
	//! stays null until the config's RplLoad lands. Either miss resolves to 0, which is the kill
	//! switch - no gate, no charge, no price suffix - rather than a guessed price.
	//! \return Dollars per litre, never negative. 0 means fuel is free everywhere.
	static float GetFuelPricePerLitre()
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if(!config || !config.m_Difficulty) return 0;

		return OVT_FuelPricing.ResolvePrice(config.m_Difficulty.fuelPricePerLitre);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether refuelling FROM this station costs nothing.
	//!
	//! True when any one of three structural tests passes:
	//!  1. the station's owner (or its root parent) carries an OVT_FuelSourceComponent marked free -
	//!     the Fuel Depot, whose contents the resistance already hauled and paid for;
	//!  2. the station is on a vehicle - fuel trucks and any future fuel vehicle. Asked of the
	//!     component's prefab data (CanMoveWithPhysics is the public accessor for the m_bIsVehicle
	//!     flag that FuelSupportStation_Vehicle.ct sets), and backed up by a Vehicle cast on the root
	//!     parent so a station slotted into a vehicle that forgot the flag is still caught;
	//!  3. the station does not use range - held jerrycans (FuelSupportStation_Gadget.ct authors
	//!     m_fRange -1) and any self-only station. That fuel was bought at a pump already.
	//!
	//! A null or ownerless station reads as free: an unknown source must never be charged for.
	//! \param[in] station The station that would PROVIDE the fuel, not the entity being refuelled.
	//! \return True when this source dispenses free fuel.
	static bool IsFreeFuelSource(SCR_BaseSupportStationComponent station)
	{
		if(!station) return true;

		IEntity owner = station.GetOwner();
		if(!owner) return true;

		// 1. The Overthrow free-source marker, on the owner or on its root parent
		OVT_FuelSourceComponent marker = OVT_FuelSourceComponent.Cast(owner.FindComponent(OVT_FuelSourceComponent));
		if(!marker)
		{
			IEntity markerRoot = owner.GetRootParent();
			if(markerRoot && markerRoot != owner)
				marker = OVT_FuelSourceComponent.Cast(markerRoot.FindComponent(OVT_FuelSourceComponent));
		}

		if(marker && marker.IsFree()) return true;

		// 2. On a vehicle - prefab flag first, then the structural backup
		SCR_BaseSupportStationComponentClass classData = SCR_BaseSupportStationComponentClass.Cast(station.GetComponentData(owner));
		if(classData && classData.CanMoveWithPhysics()) return true;

		if(Vehicle.Cast(owner)) return true;

		IEntity root = owner.GetRootParent();
		if(root && Vehicle.Cast(root)) return true;

		// 3. No range at all - a held gadget, or a station that can only ever serve itself
		if(!station.UsesRange()) return true;
		if(station.GetRange() <= 0) return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! What one litre costs at THIS station.
	//!
	//! The one call a consumer should need: free sources answer 0 without ever consulting difficulty,
	//! so a caller cannot accidentally price a fuel truck.
	//! \param[in] station The station that would provide the fuel.
	//! \return Dollars per litre, never negative. 0 at a free source or when the price is disabled.
	static float GetFuelCostPerLitre(SCR_BaseSupportStationComponent station)
	{
		if(IsFreeFuelSource(station)) return 0;

		return GetFuelPricePerLitre();
	}

	//------------------------------------------------------------------------------------------------
	//! Every registered fuel source whose OWN range covers this position.
	//!
	//! "Covers" is the station's question, not the caller's: a pump with a 6 m range answers for a
	//! point 5 m away and not for one 20 m away, exactly as the vanilla refuel action would. This is
	//! the query high-command wants for "could a vehicle sitting HERE be refuelled without moving?".
	//! Distance is measured from the station's own centre (SCR_BaseSupportStationComponent.GetPosition,
	//! which is its owner's position plus the authored offset), so a pump's nozzle offset is honoured.
	//!
	//! Answers 0 with an empty list wherever Overthrow's game mode is not running, or before the
	//! support station manager exists - never an error, never a guess.
	//! \param[in] position World position to test, e.g. where a vehicle is parked.
	//! \param[out] stations Cleared, then filled with the covering fuel stations.
	//! \return How many stations were written to stations.
	static int FindFuelSourcesCovering(vector position, notnull out array<SCR_FuelSupportStationComponent> stations)
	{
		array<SCR_BaseSupportStationComponent> registered = {};
		if (!CollectFuelStations(registered))
		{
			stations.Clear();
			return 0;
		}

		stations.Clear();

		foreach (SCR_BaseSupportStationComponent station : registered)
		{
			SCR_FuelSupportStationComponent fuelStation = SCR_FuelSupportStationComponent.Cast(station);
			if (!fuelStation) continue;

			if (!fuelStation.GetOwner()) continue;

			float range = fuelStation.GetRange();
			if (range <= 0) continue;

			if (vector.DistanceSq(position, fuelStation.GetPosition()) > range * range) continue;

			stations.Insert(fuelStation);
		}

		return stations.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Every registered fuel source within a radius of this position, whatever their own range is.
	//!
	//! The caller's question, not the station's: "what fuel is NEAR here?" - for a high-command tick
	//! deciding where to send a vehicle, or a search that should find a pump the vehicle would have
	//! to drive the last few metres to. A station just outside its own range of the position is still
	//! returned, which is the whole difference from FindFuelSourcesCovering.
	//! \param[in] position World position to search around.
	//! \param[in] radius Search radius in metres. Zero or negative finds nothing.
	//! \param[out] stations Cleared, then filled with the fuel stations inside the radius.
	//! \return How many stations were written to stations.
	static int FindFuelSourcesNear(vector position, float radius, notnull out array<SCR_FuelSupportStationComponent> stations)
	{
		stations.Clear();

		if (radius <= 0) return 0;

		array<SCR_BaseSupportStationComponent> registered = {};
		if (!CollectFuelStations(registered)) return 0;

		foreach (SCR_BaseSupportStationComponent station : registered)
		{
			SCR_FuelSupportStationComponent fuelStation = SCR_FuelSupportStationComponent.Cast(station);
			if (!fuelStation) continue;

			if (!fuelStation.GetOwner()) continue;

			if (vector.DistanceSq(position, fuelStation.GetPosition()) > radius * radius) continue;

			stations.Insert(fuelStation);
		}

		return stations.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Snapshots every registered FUEL-type support station.
	//!
	//! The single place the vanilla registry is touched, so both range queries degrade identically
	//! when there is no game mode, no manager, or nothing registered yet.
	//! \param[out] stations Filled with the registered fuel-type stations.
	//! \return False when there is nothing to search, in which case stations is meaningless.
	protected static bool CollectFuelStations(notnull out array<SCR_BaseSupportStationComponent> stations)
	{
		SCR_SupportStationManagerComponent manager = SCR_SupportStationManagerComponent.GetInstance();
		if (!manager) return false;

		if (manager.OVT_GetSupportStationsOfType(ESupportStationType.FUEL, stations) <= 0) return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The single best fuel source for filling this entity, or null when nothing can serve it.
	//!
	//! THIS IS THE FAST-FILL ACTION'S SOURCE RULE, and it is asked on both machines: the client asks
	//! it to decide whether to show the action at all and what price to draw, and the server asks it
	//! again - from scratch, trusting nothing the client sent - before it moves a drop. One body, so
	//! the offer and the authority cannot disagree about which pump is being used.
	//!
	//! A CANDIDATE MUST PASS FOUR TESTS:
	//!  1. its own range covers the target's position (FindFuelSourcesCovering);
	//!  2. it is enabled - a disabled station is not a fuel source, and a destroyed one has already
	//!     removed itself from the registry;
	//!  3. IT IS NOT MOUNTED ON THE TARGET. Without this the Fuel Depot is its own best source and
	//!     fills itself out of itself, and a fuel truck tops up its own cargo tank from its own cargo
	//!     tank. This is the same defect m_bIgnoreSelf exists to prevent in the vanilla flow, applied
	//!     to a query that never goes through the vanilla action;
	//!  4. it has fuel to give. A station with no fuel manager of its own is the vanilla
	//!     "backup flow capacity" case - an unmetered source that is never drained - and passes.
	//!
	//! THE ORDER IS FREE FIRST, THEN PRIORITY, THEN DISTANCE, and the first term is deliberately NOT
	//! what vanilla does. GetClosestValidSupportStation ranks on priority alone, which means a Fuel
	//! Depot (HIGH) built near a commercial pump (VERY_HIGH) would be shadowed and the player charged
	//! for fuel they already own (implementation.md R13). Since this action prices the fill up front,
	//! it can simply prefer the source that costs nothing - which is what a player standing between a
	//! full fuel truck and a pump obviously wants. Priority and distance then break the remaining ties
	//! exactly as vanilla would.
	//! \param[in] target The entity to be filled - any vehicle with a fuel manager, or the Fuel Depot.
	//! \return The station to draw from, or null when none covers the target with fuel to give.
	static SCR_FuelSupportStationComponent FindBestFillSource(IEntity target)
	{
		if (!target) return null;

		array<SCR_FuelSupportStationComponent> covering = {};
		if (FindFuelSourcesCovering(target.GetOrigin(), covering) <= 0) return null;

		SCR_FuelSupportStationComponent best;
		bool bestFree;
		ESupportStationPriority bestPriority;
		float bestDistanceSq;

		foreach (SCR_FuelSupportStationComponent station : covering)
		{
			if (!station.IsEnabled()) continue;
			if (IsStationMountedOn(station, target)) continue;

			SCR_FuelManagerComponent stock = GetStationFuelManager(station);
			if (stock && GetProvidableFuel(stock) <= 0) continue;

			bool free = IsFreeFuelSource(station);
			ESupportStationPriority priority = station.GetSupportStationPriority();
			float distanceSq = vector.DistanceSq(target.GetOrigin(), station.GetPosition());

			if (!best || IsBetterFillSource(free, priority, distanceSq, bestFree, bestPriority, bestDistanceSq))
			{
				best = station;
				bestFree = free;
				bestPriority = priority;
				bestDistanceSq = distanceSq;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether ANY fuel source could serve this entity, empty or not.
	//!
	//! The same candidate tests as FindBestFillSource minus the "has fuel" one, and it exists for a
	//! single reason: to tell "there is no pump here" apart from "the pump here is dry". The fast-fill
	//! action HIDES itself for the first and SHOWS ITSELF WITH A REASON for the second, and collapsing
	//! them would make an empty Fuel Depot look like no depot at all.
	//! \param[in] target The entity that would be filled.
	//! \return True when a covering, enabled fuel station exists that is not part of the target.
	static bool HasFillSourceInRange(IEntity target)
	{
		if (!target) return false;

		array<SCR_FuelSupportStationComponent> covering = {};
		if (FindFuelSourcesCovering(target.GetOrigin(), covering) <= 0) return false;

		foreach (SCR_FuelSupportStationComponent station : covering)
		{
			if (!station.IsEnabled()) continue;
			if (IsStationMountedOn(station, target)) continue;

			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The fuel manager holding a station's OWN stock, or null when it has none.
	//!
	//! A station with no fuel manager is the vanilla m_BackupMaxFlowCapacity case: it pours without
	//! ever being drained. Callers must treat null as "unmetered", not as "empty" - reading it as
	//! empty would silently stop such a station working at all.
	//! \param[in] station The station to look at.
	//! \return Its fuel manager, or null.
	static SCR_FuelManagerComponent GetStationFuelManager(SCR_BaseSupportStationComponent station)
	{
		if (!station) return null;

		IEntity owner = station.GetOwner();
		if (!owner) return null;

		return SCR_FuelManagerComponent.Cast(owner.FindComponent(SCR_FuelManagerComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a station belongs to an entity - the self-exclusion test.
	//!
	//! Compares in every direction the hierarchy can nest, because a support station may sit on the
	//! entity itself (the Fuel Depot), on a child of it (a station slotted into a vehicle), or on a
	//! sibling under the same root. Anything sharing a root parent with the target is the target for
	//! this purpose: a fuel truck must never fill its own cargo tank out of its own cargo tank.
	//! \param[in] station The candidate source.
	//! \param[in] entity The entity being filled.
	//! \return True when the station is part of that entity.
	static bool IsStationMountedOn(SCR_BaseSupportStationComponent station, IEntity entity)
	{
		if (!station || !entity) return false;

		IEntity owner = station.GetOwner();
		if (!owner) return false;

		if (owner == entity) return true;

		IEntity stationRoot = owner.GetRootParent();
		if (stationRoot == entity) return true;

		IEntity targetRoot = entity.GetRootParent();
		if (targetRoot == owner) return true;

		if (stationRoot && targetRoot && stationRoot == targetRoot) return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The fuel manager that belongs to THIS entity and nothing else - never its slotted parts.
	//!
	//! THE SCOPING RULE OF THE WHOLE FAST FILL, and it is vanilla's rule, not an invention:
	//! SCR_RefuelAtSupportStationAction.Init resolves its manager with exactly this call on its own
	//! action owner. That is why vanilla's BaseRefuel on a truck's "fuel_cap" context tops up the
	//! ~360 L chassis tanks while Refuel_CargoTank.conf - authored on the cargo-tank CHILD entity's
	//! "supportStation_fuel" context - tops up the 4,500 L cargo tank, and neither touches the other.
	//!
	//! AMENDMENT A2 EXISTS BECAUSE THE FIRST BUILD DID NOT DO THIS. It asked
	//! SCR_FuelManagerComponent.GetAllFuelManagers, which walks the slot manager, so one press at a
	//! truck's fuel cap tried to fill the chassis AND the cargo tank - ~3,960 L - and emptied a freshly
	//! filled Fuel Depot down to 21%. Do not reintroduce a slot walk here. If a future caller genuinely
	//! wants every tank on a vehicle, it should say so at its own call site, loudly.
	//! \param[in] entity The action owner - a vehicle chassis, a cargo-tank part, or the Fuel Depot.
	//! \return Its own fuel manager, or null.
	static SCR_FuelManagerComponent GetOwnFuelManager(IEntity entity)
	{
		if (!entity) return null;

		return SCR_FuelManagerComponent.Cast(entity.FindComponent(SCR_FuelManagerComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! The fuel-tank id that NAMES a manager on the wire.
	//!
	//! m_iFuelTankID is authored per node and exists precisely "for pairing with the user action and
	//! hitzone" (SCR_FuelNode:51) - vanilla's own SCR_RefuelAtSupportStationAction scopes itself with a
	//! list of them. Ids are authored distinct across a vehicle's hierarchy (Ural-4320: chassis nodes 1
	//! and 2, cargo node 10), so any ONE of a manager's node ids identifies that manager, which is what
	//! lets a fill request name a tank the network cannot address directly - a cargo-tank part carries
	//! no RplComponent of its own.
	//! \param[in] manager The manager to name.
	//! \return The first scripted node's tank id, or -1 when there is nothing to name.
	static int GetPrimaryFuelTankId(SCR_FuelManagerComponent manager)
	{
		if (!manager) return -1;

		array<SCR_FuelNode> nodes = {};
		manager.GetScriptedFuelNodesList(nodes);

		foreach (SCR_FuelNode node : nodes)
		{
			if (!node) continue;

			return node.GetFuelTankID();
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Resolve a tank id back to a manager somewhere under a networked root. THE SERVER'S HALF OF THE
	//! FILL REQUEST, and the client uses it too so both machines agree on what an id means.
	//!
	//! Searches the root's OWN manager first, then the managers on its slotted parts, and answers the
	//! first one holding a node with that id. The search is deliberately bounded to this root: a
	//! request can therefore only ever name a tank on the entity the server already resolved, checked
	//! for range and found a fuel source for - it can never reach across to another vehicle.
	//! \param[in] root The networked entity the request named.
	//! \param[in] fuelTankId The tank id from the request.
	//! \return The manager that owns a node with that id, or null when the id names nothing here.
	static SCR_FuelManagerComponent FindFuelManagerByTankId(IEntity root, int fuelTankId)
	{
		if (!root || fuelTankId < 0) return null;

		array<SCR_FuelManagerComponent> managers = {};
		if (SCR_FuelManagerComponent.GetAllFuelManagers(root, managers) <= 0) return null;

		foreach (SCR_FuelManagerComponent manager : managers)
		{
			if (ManagerHasTankId(manager, fuelTankId))
				return manager;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a manager holds a node with this tank id.
	//! \param[in] manager The manager to search.
	//! \param[in] fuelTankId The id to look for.
	//! \return True when one of its scripted nodes carries that id.
	static bool ManagerHasTankId(SCR_FuelManagerComponent manager, int fuelTankId)
	{
		if (!manager) return false;

		array<SCR_FuelNode> nodes = {};
		manager.GetScriptedFuelNodesList(nodes);

		foreach (SCR_FuelNode node : nodes)
		{
			if (!node) continue;

			if (node.GetFuelTankID() == fuelTankId)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! How many litres one fuel manager could still take in.
	//!
	//! EVERY receiving node in that manager is counted, not just the one the vanilla refuel action
	//! would have picked. Filling only the first node found is part of what makes the vanilla trickle
	//! unusable on anything with more than one tank.
	//! \param[in] manager The fuel manager to measure.
	//! \return Litres of remaining capacity across all receiving nodes. 0 when full or absent.
	static float GetRefuelableCapacity(SCR_FuelManagerComponent manager)
	{
		if (!manager) return 0;

		array<SCR_FuelNode> nodes = {};
		manager.GetScriptedFuelNodesList(nodes, SCR_EFuelNodeTypeFlag.CAN_RECEIVE_FUEL);

		float capacity = 0;

		foreach (SCR_FuelNode node : nodes)
		{
			if (!node) continue;

			float room = node.GetMaxFuel() - node.GetFuel();
			if (room <= 0) continue;

			capacity = capacity + room;
		}

		return capacity;
	}

	//------------------------------------------------------------------------------------------------
	//! How many litres a fuel manager could give away.
	//!
	//! Only CAN_PROVIDE_FUEL nodes count, which is what stops a car's own tank reading as a source.
	//! \param[in] manager The fuel manager of a source.
	//! \return Litres available across all providing nodes. 0 when dry or absent.
	static float GetProvidableFuel(SCR_FuelManagerComponent manager)
	{
		if (!manager) return 0;

		array<SCR_FuelNode> nodes = {};
		manager.GetScriptedFuelNodesList(nodes, SCR_EFuelNodeTypeFlag.CAN_PROVIDE_FUEL);

		float available = 0;

		foreach (SCR_FuelNode node : nodes)
		{
			if (!node) continue;

			float fuel = node.GetFuel();
			if (fuel <= 0) continue;

			available = available + fuel;
		}

		return available;
	}

	//------------------------------------------------------------------------------------------------
	//! Ranks one fill-source candidate against the best so far: free beats paid, then higher priority,
	//! then closer.
	//!
	//! Split out of FindBestFillSource so the ordering is one readable rule rather than a nest of
	//! conditions inside a loop - and so the "free wins first" decision, which is the one place this
	//! deliberately departs from vanilla's ranking, is impossible to miss.
	//! \param[in] free Whether the candidate dispenses free fuel.
	//! \param[in] priority The candidate's authored support-station priority.
	//! \param[in] distanceSq Squared distance from the target to the candidate.
	//! \param[in] bestFree Whether the incumbent is free.
	//! \param[in] bestPriority The incumbent's priority.
	//! \param[in] bestDistanceSq The incumbent's squared distance.
	//! \return True when the candidate should replace the incumbent.
	protected static bool IsBetterFillSource(bool free, ESupportStationPriority priority, float distanceSq, bool bestFree, ESupportStationPriority bestPriority, float bestDistanceSq)
	{
		if (free != bestFree)
			return free;

		if (priority != bestPriority)
			return priority > bestPriority;

		return distanceSq < bestDistanceSq;
	}

	//------------------------------------------------------------------------------------------------
	//! Which player's account a refuel performed by this character belongs to.
	//!
	//! An empty string means "not a player", and every caller treats that as FREE rather than as an
	//! error (implementation.md D7): an AI, a recruit or a script has no account to charge and
	//! blocking it would break future AI logistics. high-command charges its OWNING player through
	//! this class instead, which is high-command's decision to make, not the station's.
	//!
	//! Resolution follows the codebase's own chain - OVT_PlayerManagerComponent.GetPersistentIDFromControlledEntity,
	//! which is SCR_PossessingManagerComponent.GetPlayerIdFromControlledEntity followed by
	//! GetPersistentIDFromPlayerID. That map is populated on CLIENTS as well as the server (SetupPlayer
	//! plus the manager's RplLoad for join-in-progress), which is what lets the client-side
	//! affordability gate answer the same question the server will.
	//! \param[in] actionUser The character that started the refuel action.
	//! \return The player's persistent id, or "" when the character is not a player.
	static string ResolvePersistentId(IEntity actionUser)
	{
		if(!actionUser) return "";

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return "";

		return players.GetPersistentIDFromControlledEntity(actionUser);
	}
}
