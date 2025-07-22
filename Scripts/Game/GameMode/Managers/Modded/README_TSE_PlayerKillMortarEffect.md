# TSE Player Kill Mortar Effect

## Overview
This mod adds a dramatic mortar barrage effect that spawns when a player kills another player. The effect creates an immersive combat experience by adding visual feedback for player vs player combat.

## Features

### 🎯 Player Kill Detection
- **Automatic Detection**: Listens for player vs player kills
- **Smart Filtering**: Only triggers on actual player kills (not AI kills)
- **Server-Side**: Only runs on server to prevent client-side manipulation

### 💥 Mortar Barrage Effect
- **Prefab**: Uses `{5D48E2F7DB0C3714}PrefabsEditable/EffectsModules/Mortar/EffectModule_Zoned_MortarBarrage_Small.et`
- **Delayed Spawn**: 3-second delay before effect appears
- **Random Position**: Spawns within 50m radius of killer's position
- **Auto-Cleanup**: Automatically deletes after 30 seconds

### ⚙️ Configurable Settings
- **Spawn Delay**: Time before mortar barrage appears (default: 3 seconds)
- **Spawn Radius**: Random offset radius for spawn position (default: 50m)
- **Enable/Disable**: Toggle the feature on/off
- **Effect Prefab**: Customizable mortar barrage effect

## Installation

### 1. Add to Game Mode
Add the prefab to your game mode entity:
```
Prefabs/GameMode/Modded/TSE_PlayerKillMortarEffect.et
```

### 2. Component Configuration
The component can be configured with these attributes:

| Attribute | Default | Description |
|-----------|---------|-------------|
| `m_MortarBarragePrefab` | `{5D48E2F7DB0C3714}PrefabsEditable/EffectsModules/Mortar/EffectModule_Zoned_MortarBarrage_Small.et` | Mortar effect prefab |
| `m_fSpawnDelay` | `3.0` | Delay in seconds before spawning |
| `m_fSpawnRadius` | `50.0` | Random offset radius in meters |
| `m_bEnabled` | `true` | Enable/disable the feature |

## How It Works

### Event Flow
1. **Player Kill**: Player A kills Player B
2. **Event Detection**: `OnPlayerKilled` event is triggered
3. **Validation**: Checks that both killer and victim are players
4. **Scheduling**: Schedules mortar barrage spawn with delay
5. **Spawn**: Creates mortar effect at killer's location + random offset
6. **Cleanup**: Automatically deletes effect after 30 seconds

### Code Structure
```c
// Main event handler
protected void OnPlayerKilled(notnull SCR_InstigatorContextData instigatorContextData)
{
    // Validate both killer and victim are players
    // Schedule mortar barrage spawn
}

// Spawn the effect
protected void SpawnMortarBarrage(vector spawnPosition)
{
    // Add random offset
    // Spawn mortar barrage prefab
    // Schedule cleanup
}
```

## Console Output

### Success Messages
```
TSE_PlayerKillMortarEffect: Subscribed to player kill events
TSE_PlayerKillMortarEffect: Player 5 killed player 3, spawning mortar barrage
TSE_PlayerKillMortarEffect: Spawned mortar barrage at position [1234.5, 0, 678.9]
TSE_PlayerKillMortarEffect: Deleted mortar barrage effect
```

### Error Messages
```
TSE_PlayerKillMortarEffect: ERROR - Could not get game mode!
TSE_PlayerKillMortarEffect: ERROR - No mortar barrage prefab set!
TSE_PlayerKillMortarEffect: ERROR - Failed to spawn mortar barrage effect!
```

## Customization

### Different Effects
You can change the mortar barrage effect by modifying the `m_MortarBarragePrefab` attribute to use different effect prefabs:

- **Small Barrage**: `{5D48E2F7DB0C3714}PrefabsEditable/EffectsModules/Mortar/EffectModule_Zoned_MortarBarrage_Small.et`
- **Medium Barrage**: `{5D48E2F7DB0C3714}PrefabsEditable/EffectsModules/Mortar/EffectModule_Zoned_MortarBarrage_Medium.et`
- **Large Barrage**: `{5D48E2F7DB0C3714}PrefabsEditable/EffectsModules/Mortar/EffectModule_Zoned_MortarBarrage_Large.et`

### Timing Adjustments
- **Immediate**: Set `m_fSpawnDelay` to `0.0`
- **Longer Delay**: Increase `m_fSpawnDelay` for more dramatic effect
- **Larger Area**: Increase `m_fSpawnRadius` for wider spawn area

## Performance Considerations

### Server Impact
- **Minimal**: Only runs on player kill events
- **Efficient**: Uses delayed spawning to spread load
- **Cleanup**: Automatic deletion prevents entity buildup

### Network Impact
- **Low**: Effect spawning is server-side only
- **Optimized**: Uses existing game event system
- **Controlled**: Limited spawn frequency (only on player kills)

## Troubleshooting

### Effect Not Spawning
1. **Check Console**: Look for error messages
2. **Verify Prefab**: Ensure mortar barrage prefab exists
3. **Check Enable**: Verify `m_bEnabled` is set to `true`
4. **Server Only**: Confirm it's running on server

### Performance Issues
1. **Reduce Frequency**: Increase spawn delay
2. **Smaller Radius**: Reduce spawn radius
3. **Shorter Duration**: Modify cleanup timer
4. **Disable**: Set `m_bEnabled` to `false`

## Integration with Other Mods

### Compatible Systems
- **Overthrow Game Mode**: Uses existing player kill events
- **Damage System**: Works with any damage source
- **Faction System**: Independent of faction affiliations
- **Respawn System**: No interference with respawn mechanics

### Potential Conflicts
- **Other Kill Effects**: May conflict with similar mods
- **Performance Mods**: Monitor for performance impact
- **Event Systems**: Uses standard game events

## Future Enhancements

### Possible Features
- **Faction-Based**: Different effects for different factions
- **Weapon-Specific**: Different effects based on weapon used
- **Location-Based**: Different effects in different areas
- **Player-Specific**: Custom effects for specific players

### Advanced Options
- **Sound Effects**: Add audio feedback
- **Particle Systems**: Enhanced visual effects
- **Notification System**: Player notifications
- **Statistics Tracking**: Kill effect statistics

---

*This mod enhances the player vs player combat experience by adding dramatic visual feedback for kills.* 