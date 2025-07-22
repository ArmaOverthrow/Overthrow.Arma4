# TSE Enhanced Main Menu - Faction Mechanics Guide

## Overview
This guide explains how the **OF Resources** and **Resistance Threat** parameters displayed in the enhanced Main Menu affect gameplay in the Overthrow mod. Understanding these mechanics is crucial for strategic planning and successful resistance operations.

---

## 💰 OF Resources (Occupying Faction Resources)

### What They Are
- **Currency**: The occupying faction's "money" to spend on military operations
- **Dynamic**: Changes every 6 hours based on threat level and player count
- **Strategic**: Determines how aggressive the AI can be
- **Real-time**: Now visible to all players via the enhanced Main Menu

### Resource Generation Mechanics

#### Base Generation (Every 6 Hours)
```c
// Occurs at 0, 6, 12, 18 o'clock
int newResources = baseResourcesPerTick + (resourcesPerTick * threatFactor);

// Threat factor calculation
float threatFactor = m_iThreat / 1000;
if(threatFactor > 4) threatFactor = 4; // Capped at 4x multiplier
```

#### Player Count Scaling
```c
// Resources scale with number of players online
if(numPlayersOnline > 32) newResources *= 6;
else if(numPlayersOnline > 24) newResources *= 5;
else if(numPlayersOnline > 16) newResources *= 4;
else if(numPlayersOnline > 8) newResources *= 3;
else if(numPlayersOnline > 4) newResources *= 2;
```

### Resource Spending Priorities

#### 1. Base Upgrades (80% of gained resources)
- **Defense Positions**: Guards and defensive units
- **Patrol Groups**: Mobile security forces
- **Tower Guards**: Radio tower protection
- **Vehicle Spawns**: Military vehicles and equipment
- **Base Infrastructure**: Buildings and fortifications

#### 2. Special Operations (when resources > maxQRF)
- **Targeted Attacks**: Known resistance locations
- **Strategic Strikes**: FOBs and camps
- **Intelligence Operations**: Reconnaissance missions

#### 3. Counter Attacks (when resources > 2000)
- **Random Base Retakes**: 10% chance every hour
- **Surplus Spending**: Uses excess resources aggressively

### Gameplay Impact by Resource Level

| Resource Level | AI Behavior | Strategic Implication |
|----------------|-------------|----------------------|
| **2000+** | Very Aggressive | Prepare for counter-attacks |
| **1000-2000** | Moderately Aggressive | Normal operations, some risk |
| **500-1000** | Defensive | Good time for player attacks |
| **<500** | Very Defensive | Best time for major operations |
| **0** | Passive | Cannot spawn new units |

---

## ⚠️ Resistance Threat

### What It Is
- **Aggression Meter**: Measures occupying faction's desire to attack
- **Dynamic**: Increases with player kills, decreases over time
- **Strategic**: Influences resource generation and AI behavior
- **Real-time**: Now visible to all players via the enhanced Main Menu

### Threat Mechanics

#### Threat Increase
```c
// When players kill AI soldiers
void OnAIKilled(IEntity ai, IEntity instigator)
{
    m_iThreat += 5; // +5 threat per kill
}
```

#### Threat Decrease
```c
// Every 15 minutes (0, 15, 30, 45 minutes)
int threatReduce = Math.Ceil(m_iThreat * threatReductionFactor);
m_iThreat -= threatReduce;
```

### Threat Impact on Resources

#### Resource Multiplier Calculation
```c
float threatFactor = m_iThreat / 1000;
if(threatFactor > 4) threatFactor = 4; // Capped at 4x

int newResources = baseResourcesPerTick + (resourcesPerTick * threatFactor);
```

#### Threat Level Effects

| Threat Level | Resource Multiplier | AI Behavior |
|--------------|-------------------|-------------|
| **1000+** | 4x Resources | Maximum aggression |
| **750-1000** | 3x Resources | High aggression |
| **500-750** | 2x Resources | Moderate aggression |
| **250-500** | 1.5x Resources | Low aggression |
| **<250** | 1x Resources | Minimal aggression |

### QRF (Quick Reaction Force) Triggers

#### Town QRF Conditions
- **High Threat** + **Low Town Support** (<25% for resistance towns)
- **High Threat** + **High Town Support** (>75% for occupied towns)

#### Base QRF Conditions
- **Player Attacks** on occupied bases
- **Threat Level** influences response intensity

---

## 🎮 Strategic Implications for Players

### Resource Management Strategy

#### Monitor Resources
- **High Resources (2000+)**: Prepare for counter-attacks
- **Medium Resources (500-2000)**: Normal operations
- **Low Resources (<500)**: Excellent time for attacks

#### Timing Attacks
- **Attack When Resources Low**: Easier success, less resistance
- **Avoid High Resource Periods**: AI can afford heavy responses
- **Consider Player Count**: More players = more AI resources

### Threat Management Strategy

#### Stealth Operations
- **Avoid Killing AI**: Keep threat low for stealth missions
- **Guerrilla Tactics**: Hit-and-run to avoid building threat
- **Recovery Periods**: Let threat decrease between operations

#### All-Out Assaults
- **Accept High Threat**: For major operations
- **Coordinate Attacks**: Use threat info for timing
- **Resource Denial**: Attack bases to drain AI resources

### Real-time Intelligence Benefits

#### Danger Signs to Watch
- **Resources > 2000**: Counter-attack likely
- **Threat > 1000**: Heavy AI response expected
- **Both High**: Maximum AI aggression phase

#### Opportunity Windows
- **Resources < 500**: AI is weak, good for attacks
- **Threat < 200**: AI is passive, safe for operations
- **Both Low**: Best time for major resistance operations

---

## 🔍 Strategic Planning Guide

### Coordinated Operations

#### Planning Phase
1. **Monitor Resources**: Wait for low resource periods
2. **Check Threat**: Ensure threat is manageable
3. **Coordinate Team**: Plan attacks during opportunity windows
4. **Prepare Defenses**: Expect AI response based on threat level

#### Execution Phase
1. **Resource Denial**: Attack bases to drain AI resources
2. **Threat Management**: Balance aggression with stealth
3. **Timing**: Use real-time data for optimal attack timing
4. **Adaptation**: Adjust strategy based on changing values

### Advanced Tactics

#### Resource Warfare
- **Base Attacks**: Drain AI resources through base captures
- **Patrol Elimination**: Reduce AI spending on defense
- **Infrastructure Destruction**: Force AI to spend on repairs

#### Threat Manipulation
- **Stealth Operations**: Keep threat low for extended periods
- **Decoy Attacks**: Distract AI while main operation proceeds
- **Threat Cycling**: Alternate between high and low threat periods

---

## 📊 Monitoring and Analysis

### Key Metrics to Track

#### Resource Trends
- **Spike Detection**: Sudden increases indicate AI operations
- **Depletion Patterns**: Track when AI runs low on resources
- **Player Impact**: Monitor how player actions affect resources

#### Threat Patterns
- **Build-up Phases**: Track threat accumulation over time
- **Decay Rates**: Understand how quickly threat decreases
- **Player Influence**: Measure impact of player actions on threat

### Strategic Decision Making

#### When to Attack
- **Low Resources + Low Threat**: Optimal conditions
- **Low Resources + High Threat**: Good for resource denial
- **High Resources + Low Threat**: Risky but possible
- **High Resources + High Threat**: Avoid unless necessary

#### When to Defend
- **High Resources + High Threat**: Maximum defensive posture
- **High Resources + Low Threat**: Moderate defense
- **Low Resources + Any Threat**: Minimal defense needed

---

## 🛠️ Technical Implementation

### Enhanced Main Menu Features

#### Real-time Updates
- **5-Second Refresh**: Values update every 5 seconds
- **Color Coding**: Visual threat level indicators
- **Client/Server Sync**: Accurate data across all players

#### Display Information
- **OF Resources**: Current occupying faction resources
- **Resistance Threat**: Current threat level against resistance
- **Color Indicators**: Red (high), Orange (medium), White (low)

### Modded Components

#### TSE_OccupyingFactionManager
- **Enhanced RPC**: Synchronizes faction data to clients
- **Real-time Updates**: Broadcasts changes immediately
- **JIP Support**: New players receive current data

#### TSE_MainMenuContext
- **Enhanced Display**: Shows faction data in Main Menu
- **Automatic Updates**: Refreshes values periodically
- **Error Handling**: Robust data retrieval and display

---

## 📝 Conclusion

The enhanced Main Menu provides players with crucial strategic intelligence that was previously hidden. By understanding how OF Resources and Resistance Threat affect gameplay, players can:

1. **Plan Operations**: Time attacks for optimal conditions
2. **Manage Risk**: Avoid high-threat periods when unprepared
3. **Coordinate Teams**: Share real-time intelligence
4. **Adapt Strategy**: Respond to changing AI conditions
5. **Maximize Success**: Use data-driven decision making

This real-time information transforms the resistance experience from reactive to strategic, giving players the tools they need to effectively fight the occupying faction.

---

*This guide is based on analysis of the Overthrow mod codebase and the TSE Enhanced Main Menu implementation.* 