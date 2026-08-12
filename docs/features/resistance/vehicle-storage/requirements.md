# Vehicle Storage — Requirements

A new OVT component that can be placed on any building, turning it into a "vehicle storage" on top of any existing capabilities it has

Requirements:
An OVT_ParkingComponent must also be on the same building, a warning should be thrown on init if none exists and the component's capabilities disabled. This defines where the vehicle storage can be used to both store and retrieve, it won't work without one.

Storage:
Players can drive up and "store" the vehicle they are in if the vehicle is close enough to a parking space, using the vehicle menu (same as warehouses). Any occupants are removed when this happens. The vehicle's entity/rplid is then stored along with any metadata needed to display/retrieve it. Only the driver can store a vehicle, while sitting in the driver seat.

All vehicle storages can store unlimited vehicles, we dont cap it.

Persistence:
Stored vehicles are persisted and loaded as expected with no loss of state or inventory.  Use base-game serializers wherever possible to ensure nothing is lost and future changes to them in the base game are inherited free. If "deleting" the entity is too difficult to work with vanilla persistence serialization (very likely considering our previous work on vehicle persistence) and the entity must remain in the world, hide it and turn off its physics etc, and get it to selfspawn/hide on game start when loading the save (similar to how our vehicle restoration systems work already)

The key here is that this system allows busy servers to reduce the performance impact of vehicles that arent in use. Currently they are parked and left in the world with full physics sim etc. Aim to reduce this footpint as much as possible for stored vehicles. The trade-off here is reduced exposure to the war, but other systems later will make these storages a target for the enemy who can disable them.

Helicopters:
If a parking space attached to the building has a "helicopter" type then helicopters can also be stored, but you cannot drive a vehicle on to a helipad and store it, that will only accept/retrieve helicopters. So if a building ONLY has a helicopter parking space, then it only stores helicopters. This is similar to how procurement works already which detects the parking space types available. The "helipad" buildable then also becomes a consumer of this component to allow heli storage at them.

Retrieval UI:
When standing near a parking space defined by the parking component, the overthrow main menu is overridden with a screen to restore stored vehicles. You should be able to use the existing main menu override component to achieve this, but it needs to work for all the parking spaces defined so extend those systems if required or find the best solution. Disable retrieve button if something is blocking the space including a player/recruit and tell the player why, but still open the menu and show what vehicles are stored. 

Retrieval menu should try to include an image of the vehicle selected and some information about it's state. Stored vehicles are not stacked in the list, 1 entry = 1 stored vehicle. Dont use the shop menu idiom, use the import menu as a guide (plain text list with selected info panel).  Categorize this screen similar to the shop, using base-game categories where possible. The vehicles original owner name should be shown in the list next to the vehicle's name, but anyone can retrieve it with access to that vehicle storage. 

If there are multiple parking spaces try to find an empty one of the correct type, starting at the nearest (where the player is standing) and spawn it there, allowing fast mass retrieval if there are lots of spaces nearby.

Privacy and access control:
"Locked" state of the vehicle is observed here. A locked vehicle can only be retrieved by its owner. Locked vehicles still show in the retrieval menu and be selectable for everyone but are grayed out for non-owners in the list and are not retrievable.

Component has a "private" flag that only allows storage/retrieval by the building's owner. Houses are then given the component with private = true turning the player's owned houses  into private vehicle storage as well. Warehouses should already have a public/private switch iirc and the vehicle storage should respect that when it's a warehouse (if this isnt the case just mention it in code comment and it will be added later when that mechanic is)

Officers can set any non-private vehicle store into an "Officer only" store (switched, persisted), restricting storage OR retrieval to officers only (two separate switched for storage/retrieval).. Officer-only retrieval can only be switched ON if the storage is empty, to avoid locking players out of a previously-public storage. It can be switched off at any time. By default all storages are public both ways apart from when the component is specifically "private" (ie houses)

Admins can retrieve any individual stored vehicles from any storage including private ones (in case they need to clean up locked vehicles abandoned by players or retrieve a stolen vehicle). An admin should also be able to unlock any vehicle in the world if they cant already. Admins may not store a vehicle in a storage they otherwise wouldnt have access to (ie houses)

Garages:
The "Garage" buildable also becomes a consumer, and the procurement screen is updated to detect a vehicle storage component and add a "Buy into storage" button on top of the existing one, pressing this buys the vehicle and adds it into the storage instantly (you should be able to spawn it off screen then serialize it in and remove it to ensure it has the proper initial state)


