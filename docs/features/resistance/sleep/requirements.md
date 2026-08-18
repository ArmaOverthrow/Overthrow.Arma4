# Sleep — Requirements

This feature will be for SINGLE PLAYER games only. Not on listen-host or dedicated servers.

- A new action "Sleep" added to overrides on all the base-game bed prefabs
- One new placeable, a "cot" type bed that can be placed at camps, FOBs and bases. This is the only prefab that should need an RplComponent so it should not override but instead extend a base game cot prefab or just be a new prefab that uses the right model
- Action is not shown unless the bed is within range of: Real-estate owned by the player, a camp, a deployed FOB or captured base
- Action can be done at any time, not just at night
- When performed, time skips 8 hrs, all accounting should still occur. Donations/taxes are paid, rent is paid, the occupying faction gets/spends resources etc as if the time wasnt skipped
- If possible animate the screen to black and back again just for RP/flavour
- After the sleep action is performed you cannot perform it again for 12 in-game hours (cooldown). Action should still show with a countdown
