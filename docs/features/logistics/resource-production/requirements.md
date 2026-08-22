# Resource Production — Requirements

Now that the resource system is in place along with the ui core of this epic this feature should be pretty simple to make.

A new component and prefab is created to add to maps as "Resource production" locations (Sawmills, Steel mills, Cement plants etc). 

The component just needs to define:
- A name (localized)
- What it produces
- How much it produces per (in-game) hour
- A base cost to buy it (multiplied by the real estate multiplier difficulty setting)

The same prefab then gains a resource storage component and drips the resource into it at the defined rate. An ActionsManager on the prefab adds actions to buy the site (with price), change public/private, open the storage etc

Ownership of these sites work similar to warehouses but they cannot be rented. The owning player can decide to make it public or private, with a public one allowing anyone in the resistance to access the resource storage with a truck. Private is the default. We dont use the real estate menu to buy them, just an action on the prefab will suffice.

When unowned, the site works the same as the port and allows buying that one resource from the Storage (not unlimited, only what it has accumulated) for 80% of the current import price.

When bought, the storage is cleared. You do not buy the current stock level (exploit vector)

These sites are added to the map as icon locations the same as bases etc and should be visible at the same zoom level as towns/bases (they are important economic objectives). Color is black when unowned, green when owned and public (or you own it), green and half transparent when owned, private and you dont own it. The info panel for these sites shows the current stock level, owner and public/private status. Icons for the sites should be derived from the resources config for the resource it produces.

A privately owned site does not sell the resource (this may be added later in economy 2.0), only unowned sites sell the resource.

The location, name, buy price, resource and output rate will be authored on a per-site basis on the map

An owned site does not have costs, it just continues to output at the same rate and the player can use those resources for building, sell them to other players (manually) and/or export them for profit. These sites are not targets for the enemy (yet). These sites are all raw production only, turning base resources into more complex products via chains will come later in a different feature
