# High Command — Requirements

Building on top of the "virtualization" epic, High Command will be the resistance-side of virtualized groups.

- "Barracks" becomes a new buildable, but would be already existing in some bases (we override base game barracks building)
- Inside that building at a curated location (a desk or something) an action allows any player (not just officers) to buy entire groups from a set of group prefabs listed in the FIA faction config, including some vehicle groups. Location can work like the shop component already does, with just the actions manager put on a child prefab that finds its parent and ensures base is friendly etc
- If a warehouse is in range of the barracks the cost of buying the group can be reduced by using items from the warehouse (may need to inspect group character prefabs to build a list of required items, this can be done once at game load as groups are static). otherwise full price is charged same as equipped recruits (using the same difficulty-scaled multiplier)
- The groups show on the player's map as green NATO symbols. All high command groups show on all players maps, with ones they dont own at a lower opacity
- Groups are given waypoints by selecting them in the map, moving the cursor to a new location and then pressing a button. They only have one waypoint at a time
- Groups are not despawned when the player disconnects, they continue following their most recent waypoint
- New main menu item "Manage Groups" similar to recruit management shows status and allows dismissal
- Groups in range of a warehouse with the needed items automatically rearm with ammo when needed. group menu UI shows ammo/fuel status, map icon also shows ammo status when completely out
- Vehicle groups in range of a fuel source automatically refuel
- Groups are set as observers in the virtualization system, so they remain spawned at all times and can spawn enemy groups when in range
- We set a maximum of 16 groups per-player by default, server-configurable
- We extend the recruit system so the recruit menu splits the inactive recruits by group (they already merge) and add the ability to convert any of those groups into high-command ones. This allows creation of custom group compositions. They cannot be converted back. This removes those recruits from the players owned recruits and they no longer count towards the 16 recruit max
