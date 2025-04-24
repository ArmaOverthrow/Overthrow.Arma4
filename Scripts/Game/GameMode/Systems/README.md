# Overthrow Game Systems

This directory contains various gameplay systems that power the Overthrow game mode. These systems implement specific mechanics and features that work together to create the dynamic gameplay experience.

## Core Systems

### OVT_TownModifierSystem
Manages modifiers that affect towns, including support and stability values. This system applies, updates, and removes modifiers based on game events and player actions.

## Jobs System

Located in the `Jobs` subdirectory:

The Jobs system handles the creation, management, and completion of missions and tasks for players. It uses a modular structure with conditions and stages to create dynamic missions.

### Base Components
- **OVT_JobCondition**: Base class for conditions that determine if a job can be generated or completed
- **OVT_JobStage**: Base class for job stages that define the sequence of objectives in a job

### Conditions
Located in `Jobs/Conditions/`:

Conditions determine when and where jobs can be generated, accessed, or completed:
- Town-related conditions (support levels, dealers, shops)
- Player-related conditions (proximity, range)
- Randomization conditions

### Stages
Located in `Jobs/Stages/`:

Stages define the sequence of objectives that make up a job:
- Location finding (houses, dealers, shops)
- Entity spawning (civilians, enemy groups)
- Wait conditions (deaths, player acceptance, proximity)
- Support change triggers

## Modifiers System

Located in the `Modifiers` subdirectory:

The Modifiers system handles temporary and permanent effects on gameplay elements such as towns, implementing changes to support and stability values.

### Base Components
- **OVT_Modifier**: Base class for all modifiers
- **OVT_ModifierConfig**: Configuration class for modifier settings
- **OVT_StabilityModifier**: Base class for modifiers affecting town stability
- **OVT_SupportModifier**: Base class for modifiers affecting resistance support
- **OVT_TownStabilityModifierSystem**: Manages stability modifiers for towns
- **OVT_TownSupportModifierSystem**: Manages support modifiers for towns

### Support Modifiers
Located in `Modifiers/Support/`:
- **OVT_RecentDeathSupportModifier**: Applies support changes when a civilian or faction member dies

### Stability Modifiers
Located in `Modifiers/Stability/`:
- **OVT_RecentDeathStabilityModifier**: Applies stability changes when a civilian or faction member dies
- **OVT_RandomStabilityModifier**: Generates random fluctuations in stability to simulate dynamic town conditions

## Skill Effects

Located in the `SkillEffects` subdirectory:

The Skill Effects system implements the gameplay effects of player skills and abilities:
- **OVT_GivePermissionSkillEffect**: Grants special permissions based on skill level
- **OVT_StaminaSkillEffect**: Modifies player stamina attributes
- **OVT_StealthSkillEffect**: Enhances player stealth capabilities
- **OVT_SupportSkillEffect**: Improves player influence on town support
- **OVT_TradeDiscountSkillEffect**: Provides economic benefits when trading

---

For more information about Overthrow's development, visit our [GitHub repository](https://github.com/ArmaOverthrow/Overthrow.Arma4) or join our [Discord](https://discord.gg/j6CvmFfZ95). 