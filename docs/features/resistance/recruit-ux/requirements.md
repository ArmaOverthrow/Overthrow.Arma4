# Recruit Ux — Requirements

We will make some improvements to the recruit systems

Headline feature: Inactive Recruits
We add the ability to remove a recruit from your group, while keeping them "owned". It'll be an action on the recruit character itself (short held/timed action to avoid accidental removal), but also added to the recruit screen. An inactive recruit leaves the slave group but remains owned by the player and a member of the resistance. You can no longer command them, open inventory etc but they keep the inventory they have now and are persisted/respawned on load like any other recruit. They still count towards the 16 maximum and show in the recruits screen in an "Inactive" section at the bottom, with current group members at the top in their own section. When inactive they can be added back to the group via either a character action or the recruit screen. They might need to be put in their own group alone to avoid weird formation problems, it would be ideal if nearby ones were added to a group though so maybe when you make them inactive it searches for other inactives nearby and groups them up

Additional improvements:
- A new map icon layer (see map/map-layers)  like the player location which shows all your recruits both inactive and active. A set of icons (user-provided and added to the main map iconset) shows a base icon similar to the player icon (inactive has lower opacity, active at full opacity) and a second small icon tag showing:
   - Unarmed (no second icon)
   - Armed + has ammo (bullets)
   - Armed + out of ammo (cross through bullets)
- Add more info to the recruit screen showing armed/unarmed, ammo status, wounded, and anything else that might be useful, can reuse the icons above
-Loadout swap: A character action (for active recruits only) that swaps your loadout with theirs. Everything included, clothes, the lot. Would be an amazing QOL hack to get recruits loaded up exactly how you want quickly
- During /plan-feature suggest any more quick wins that would improve the recruit system

## Phase 9 addition (user, 2026-08-14, post-build)

Once you can build a recruitment tent you should be able to buy recruits from it with a saved loadout applied. It should cost what those items would cost plus a hefty margin as a convenience fee.

Confirmed design calls (user, same day): price = sum of each loadout item's shop catalog price × a convenience-fee multiplier, default **1.5×**, exposed as a server config value; **own saved loadouts only** (no officer templates); gear is **spawned and charged for** (purchase semantics, like shop imports) — deliberately not the equipment-box conservation path (BUG-042 territory).

