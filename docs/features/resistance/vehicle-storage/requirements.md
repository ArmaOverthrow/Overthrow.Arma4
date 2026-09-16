# Vehicle Storage — Requirements

A new component, `OVT_VehicleStorageComponent`, turns any building into a vehicle store. A stored vehicle is a data record, not an entity. This is the same model the item storage uses. A busy server keeps its parked vehicles as records and pays no physics or replication cost for them. The trade-off is less exposure to the war. A later feature makes vehicle stores a target the enemy can disable.

## 1. Placement rules

The component goes on any building prefab. It adds to the capabilities the building already has. The building must also carry an `OVT_ParkingComponent`. The parking spots define where a player stores and retrieves a vehicle. A store has no capacity cap.

A public component with no parking component logs one warning on init and stays inert. A private component with no parking component stays inert and logs nothing. Many house prefabs have no parking spots.

A store accepts a vehicle only if it has a parking spot of the parking type of that vehicle. The economy maps each vehicle prefab to a parking type today. The store uses that map.

## 2. Store a vehicle

The driver opens the vehicle menu while in the driver seat. This is the same menu that holds the warehouse buttons. A "Store vehicle" button shows when a usable store is in range and the driver has access to it. In range means the vehicle is within 10 m of a parking spot of its own type on that building.

The server removes every occupant from the vehicle before it stores it. Recruits count as occupants. The server writes the record, then deletes the entity.

The store refuses a mobile FOB and a deployed FOB. The store accepts an unlocked vehicle from a player who does not own it. The record keeps the original owner.

## 3. The stored record

One record per stored vehicle. The record holds:

| Field | Source |
| --- | --- |
| Vehicle prefab | The prefab of the entity |
| Original owner | `OVT_PlayerOwnerComponent` |
| Locked flag | `OVT_PlayerOwnerComponent` |
| Stored by, stored at | The request |
| Fuel, per tank | The fuel managers |
| Item ledger | `OVT_StorageComponent` |

At store time the server moves every item in the vanilla cargo into the `OVT_StorageComponent` ledger of the vehicle. Each item becomes a ledger line. This is the same path the warehouse uses when a player empties a vehicle into it. The record then holds that ledger. Retrieval puts the ledger back on the `OVT_StorageComponent` of the fresh vehicle. The items stay ledger lines and do not return to the vanilla cargo.

Storage repairs the vehicle. A retrieved vehicle comes back at full health, so the record holds no damage.

Not kept: the state of single items in the cargo (magazine fill, weapon attachments), and the world position. The warehouse accepts the same loss today.

The store must never depend on a vanilla persistence record that outlives its entity. Those records die within minutes (BUG-086).

## 4. Retrieve a vehicle

A player standing within range of any parking spot of the building opens the Overthrow main menu. The store screen opens instead of the main menu. The existing main menu override component measures range from the building origin. Extend it, or add a new finder, so every parking spot counts.

The screen always opens and always lists the stored vehicles. The retrieve button disables when a blocker sits on the target spot. The reason shows next to the button. A player or a recruit on the spot counts as a blocker. The helicopter spot needs a real obstruction test. The parking component skips it for that type today.

Retrieval searches the spots of the correct type, nearest to the player first, and spawns on the first free one. Many free spots allow fast mass retrieval. The fresh vehicle gets the owner, lock state, fuel and item ledger from the record. It spawns at full health. It registers with the vehicle manager as a normal player vehicle.

## 5. The store screen

Use the transfer screen as the guide (plain list, selected item detail panel). Do not use the shop idiom. One list entry is one stored vehicle. No stacking. Each entry shows the vehicle name and the name of the original owner.

Tabs use the vanilla vehicle labels: car, truck, APC, helicopter, airplane. An "all" tab comes first. The detail panel shows the vehicle preview image, the fuel and the item count.

A locked vehicle shows for everyone. The list grays it out for a player who is not the owner. That player can select it but cannot retrieve it. At a garage the screen has two tabs: "Stored" and "Buy". See section 9.

## 6. Helicopters

A helicopter parking spot accepts and returns helicopters only. A helipad refuses a car. A building with only a helicopter spot stores only helicopters. This is the same detection procurement uses today.

The helipad buildable gets a parking component with one helicopter spot. No prefab has one today.

## 7. Access control

Only the owner can retrieve a locked vehicle. The component has a "private" flag. A private store allows storage and retrieval by the building owner only. Houses get the component with private set. The houses a player owns become private vehicle stores.

A warehouse store follows the `isPrivate` flag of the warehouse. The flag and the access check exist today. A player switch for it does not. The store respects the flag now and needs no code comment.

A ruined building blocks storage and retrieval until a player repairs it. The records stay. This is the same rule the warehouse follows. When a house changes owner, the records stay. The new owner can retrieve them. A locked vehicle stays locked to its original owner.

## 8. Officer switches

An officer can set two switches on any non-private store: "officer-only storage" and "officer-only retrieval". Both persist. The switches live in the store screen header. Only an officer sees them.

An officer can turn "officer-only retrieval" on only when the store is empty. An officer can turn it off at any time. Every store is public both ways by default. A private store has no switches.

## 9. Garages and procurement

The garage buildable gets the component. Procurement no longer spawns the vehicle. A purchase adds a record to the store of the garage. The record is a fresh vehicle: full fuel, full health, empty ledger. The purchase spawns no entity.

The garage screen holds two tabs. "Stored" is the store screen. "Buy" is the procurement list. A purchase moves the player to the "Stored" tab with the new vehicle selected. The garage refuses a purchase when it has no spot of the parking type of that vehicle.

## 10. Admins

An admin can retrieve any stored vehicle from any store, private stores included. This covers abandoned locked vehicles and stolen vehicles. An admin can open the lock on any vehicle in the world. The unlock action is owner-only today. This is a new admin action on the vehicle. An admin cannot store a vehicle into a store the admin has no normal access to.

## 11. Persistence

The records persist with the game mode save through an Overthrow serializer. No stored vehicle exists as an entity in the save. A save and load round trip keeps every record field.

A stored record uses none of the offline vehicle logic. Reservation, respawn on login and rebuild from a position record apply to live vehicles only and stay untouched.

## 12. Map

A stored vehicle has no map marker of its own. The marker of the building stands for its contents.

## Out of scope

- Enemy attacks on vehicle stores. A later feature.
- A player switch for warehouse privacy. A later feature.
- Storage of a mobile FOB.

## Decisions

- Cargo (user, 2026-09-08): vanilla cargo items move into the item ledger of the vehicle at store time and become ledger lines.
- Record fidelity (user, 2026-09-08): the record keeps prefab, owner, lock, fuel and the item ledger. Storage repairs the vehicle. Single-item state in the cargo is not kept. A vanilla persistence record is not an option because a released record dies within minutes (BUG-086).

- Store range (user, 2026-09-08): 10 m to the nearest spot of the correct type.
- Detail panel (user, 2026-09-08): preview image, fuel and item count.
