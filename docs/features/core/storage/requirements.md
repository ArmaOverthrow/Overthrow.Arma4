# Storage — Requirements

The base game inventory systems are not designed for our use. They track spawned entities with full state when 90% of the time Overthrow is only concerned with type + quantity. This means that a full ammobox filled with loot or imported items can cause network spikes, kicking players or causing server FPS issues on save etc.

A warehouse in Overthrow already solves this problem, it stores only pure data rather than entities. This core feature will extract, correct and build upon those ideas to make them usable anywhere. However warehouse code hasnt been touched for a while and should be checked for accuracy and completeness as we migrate and extract it

The component:
A new component is developed that acts as a warehouse-style inventory. This component gets added to the warehouse (replacing its current storage mechanisms), as well as placed ammoboxes, trucks and civilian cars (offensive/illegal vehicles do not get it). The component includes a capacity setting we set for smaller vehicles only, warehouses, ammoboxes and trucks will have unlimited storage (-1)

The component includes server-only methods that can "convert" items between our storage and the base game inventory. We dont remove or alter the base game inventory systems at all (that is still how you access items as your character), we just "move" them between with a conversion process that spawns/despawns entities. To avoid network spikes we make sure to batch all spawning/despawning with a progress bar where possible

New action is added to vehicles/ammoboxes with the component: "Open Storage (1123 items)" - opens a new UI similar to warehouses, categorized and has "Transfer 1", "transfer 10", "transfer all" which moves items out of storage into normal inventory to be accessed, spawning them as fresh entities. This action should be right after the vanilla "Open" action in the list for accessibility and discovery

Another action on ammoboxes/vehicles "Transfer all to storage" converts all the items in the inventory to our storage, taking into account the following:
- Half-used ammunition clips are IGNORED. only a full ammoclip is converted to storage, used clips stay in normal storage
- All attachments and ammo removed from entities first so they go in separately

Existing load/unload scripts on ammoboxes change to use the new storage only, however "Unload" should first call "Transfer all to storage" on the source, converting any normal inventory to storage before the transfer. Change the action names to mention "Storage" as well so its clear what is being loaded/unloaded. This covers the most common use case of using the "Loot" action to clean up a battle or doing it manually, while forcing the players to use the new storage system and avoid the problems with the base game inventory. Any confusion about where their items went should be solved by adding the "(2874 items)" to the Open action name so they can see where the stuff is.

Importing at the port is converted to use storage only along with taking items from a warehouse

The existing "Loot" action on trucks is out of date and should be updated along with this feature (may as well roll it in as its inventory related). The filtering it does atm is not good and it should be migrated to a Progress component so it doesnt spike the network and shows you progress. As for filtering take everything that isnt the soldiers base clothes (shirt, pants, boots). Do take helmets, do take bags, vests, etc. Only filter out the base clothes now. To compensate we will need to raise the inventory cap a little so trucks can easily fit about 20-30 soldiers worth of loot. Loot keeps using the vanilla inventory systems and is only converted upon transfer to an ammobox

Undeploy FOB action also needs to be converted to storage, it should collect everything from both base inventory and storage from the nearby ammoboxes, spent clips can be deleted

Lastly, an officer only action is added to ammoboxes that clears the inventory (base game one, not storage) so any junk can be cleared ie half-spent ammo clips or unwanted items.

Pitfalls to watch for:
The base game already has a "Storage" system meant for supplies, we should attempt to disable or hide this to avoid confusion. Also keep in mind the plans for the "logistics" epic which adds resources, but that should be a separate storage again as it has different capacity and usage requirements and we are happy to allow unlimited capacity for this system where it makes sense (warehouses, trucks, ammoboxes)
