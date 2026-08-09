# Spawn System Architecture Changes

## Summary
Fixed the start menu issue by moving it from the GameMode entity (which has no viewport) to the PlayerController entity (which does).

## Files Changed

### New Files
- `Scripts/Game/Components/Player/OVT_PlayerStartMenuHandlerComponent.c` - Handles showing the start menu on player join

### Modified Files
- `Scripts/Game/Respawn/Logic/OVT_SpawnLogic.c` - Removed blocking logic, now always spawns player normally
- `Scripts/Game/GameMode/OVT_OverthrowGameMode.c` - Removed all start menu UI handling (moved to player controller)
- `Scripts/Game/UI/Context/OVT_StartGameContext.c` - Removed TriggerPlayerSpawn call (no longer needed)

## How It Works Now

### Previous (Broken) Flow:
1. Player joins server
2. Spawn logic blocks and calls NotifyReadyForSpawn
3. Game mode tries to show UI context (but has no viewport)
4. Player stuck at main menu, can't see anything

### New (Fixed) Flow:
1. Player joins server
2. Spawn logic proceeds normally, spawns player character
3. `OVT_PlayerStartMenuHandlerComponent` on the PlayerController detects `OnRespawnReady`
4. Component shows start menu UI context (now has viewport through player)
5. Player sees menu, selects factions/difficulty
6. Clicks "Start Game"
7. Game starts normally

## What You Need to Do

### 1. Add Component to PlayerController Prefab
In Arma Reforger Workbench:
1. Open the Player Controller prefab (likely `Prefabs/Characters/Core/PlayerController.et` or your custom one)
2. Add the component: `OVT_PlayerStartMenuHandlerComponent`
3. Save the prefab

### 2. Test the Following Scenarios

#### Single Player New Game:
1. Start a new game in single player
2. You should spawn into the world
3. The start menu should appear immediately
4. Select factions and difficulty
5. Click "Start Game"
6. Game should proceed normally
7. Verify you can move and interact

#### Single Player with Save:
1. Load an existing save
2. Start menu should NOT appear
3. You should spawn at your home location
4. Game should work normally

#### Multiplayer (If Applicable):
1. Join a dedicated server
2. You should spawn
3. Start menu should appear
4. Select options and start
5. Game should proceed

## Debugging

If the start menu still doesn't appear, check the console logs for:
- `[Overthrow] OVT_PlayerStartMenuHandlerComponent initialized for player controller`
- `[Overthrow] OnRespawnReady called`
- `[Overthrow] Showing start menu for player`
- `[Overthrow] Calling ShowLayout on start game context`

If you don't see these logs, the component may not be attached to the player controller correctly.

## Technical Details

The key insight was that `GetGame().GetWorkspace().CreateWidgets()` needs to be called from an entity with a viewport. The GameMode entity has no viewport, so widgets created there are invisible. The PlayerController does have a viewport context, so UI contexts initialized with it as the owner work correctly.

This follows the same pattern as the base game's `SCR_PlayerDeployMenuHandlerComponent` which shows the deploy/respawn menu after spawning.
