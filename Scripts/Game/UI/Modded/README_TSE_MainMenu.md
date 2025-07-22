# TSE Enhanced Main Menu - Faction Resources and Threat Display

## Overview
This mod enhances the existing Main Menu by adding two additional information strings:
- **OF Resources**: Shows the current resources of the Occupying Faction
- **Resistance Threat**: Shows the current threat level against the Resistance

## Files Created (Proper Arma Reforger Modding Structure)

### 1. `Scripts/Game/UI/Context/Modded/TSE_MainMenuContext.c`
- **Proper Modding**: Uses `modded class OVT_MainMenuContext`
- **Direct Enhancement**: Extends the original Main Menu context functionality
- **Real-time Updates**: Updates faction information every 5 seconds
- **Color Coding**: Visual threat level indicators (red/orange/normal)

### 2. `Scripts/Game/Components/Player/Modded/TSE_UIManagerComponent.c`
- **Proper Modding**: Uses `modded class OVT_UIManagerComponent`
- **Automatic Integration**: Enhances the UI manager initialization
- **Logging**: Provides initialization confirmation

### 3. `Scripts/Game/UI/Modded/TSE_MainMenuIntegration.c`
- **Integration Helper**: Provides initialization functions
- **TSE Prefix**: Uses proper mod prefix for identification

## Arma Reforger Modding Structure

### ✅ Proper Modding Syntax
- **`modded` keyword**: Used before class definitions
- **TSE_ prefix**: All modded classes use TSE_ prefix
- **Modded folders**: Placed in `Modded` subfolders near original files
- **Inheritance**: Properly extends original classes

### 📁 File Structure
```
Scripts/Game/UI/Context/Modded/TSE_MainMenuContext.c
Scripts/Game/Components/Player/Modded/TSE_UIManagerComponent.c
Scripts/Game/UI/Modded/TSE_MainMenuIntegration.c
```

## Localization
Added new localization keys to `Language/localization_Overthrow.en-us.conf`:
- `OVT-OFResources`: "OF Resources: %1"
- `OVT-ResistanceThreat`: "Resistance Threat: %1"

## How It Works
1. **Modded Context**: The `modded class OVT_MainMenuContext` automatically enhances the original
2. **Direct Integration**: Enhances the existing town info display without new layouts
3. **Real-time Updates**: Every 5 seconds, fetches current faction data and updates display
4. **Color Coding**: Text color changes based on threat levels:
   - **Red tint**: High threat (>1000)
   - **Orange tint**: Medium threat (500-1000)
   - **Normal**: Low threat (<500)

## Features
- **Automatic Updates**: Faction information updates every 5 seconds when menu is active
- **Color Coding**: Visual threat level indicators
- **Seamless Integration**: Works with existing Main Menu functionality
- **Localized Text**: Uses proper localization with parameter substitution
- **Performance Optimized**: Only updates when the menu is active

## Usage
When players open the Main Menu, they will see the enhanced town information that includes:
- Original town data (Population, Stability, Supporters)
- **NEW**: OF Resources (current occupying faction resources)
- **NEW**: Resistance Threat (current threat level against resistance)

The information updates automatically and provides visual feedback through color coding.

## Installation
1. Ensure all modded files are in their proper `Modded` subfolders
2. The modded classes will automatically enhance the original functionality
3. No additional configuration required - the modding system handles the integration

## Arma Reforger Modding Best Practices
- ✅ Uses `modded` keyword for class definitions
- ✅ Places files in `Modded` subfolders
- ✅ Uses unique prefix (TSE_) for all modded classes
- ✅ Properly extends original classes without breaking functionality
- ✅ Maintains compatibility with base mod 