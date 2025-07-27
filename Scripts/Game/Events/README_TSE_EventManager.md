# TSE Event Manager System

## Overview
Централизованная система управления всеми ивентами в игре. Event Manager контролирует создание, частоту, одновременность и жизненный цикл всех ивентов.

## Architecture

### Core Components

#### TSE_EventManagerComponent
- **Location**: `Scripts/Game/GameMode/Managers/Modded/TSE_EventManagerComponent.c`
- **Purpose**: Главный менеджер всех ивентов
- **Features**:
  - Управление одновременными ивентами
  - Расчет шансов спавна
  - Контроль интервалов между ивентами
  - Глобальные ограничения (cooldown)
  - Debug logging

#### Event Configurations

##### TSE_BaseEventConfig
- **Location**: `Scripts/Game/Events/Config/TSE_BaseEventConfig.c`
- **Purpose**: Базовая конфигурация для всех ивентов
- **Properties**:
  - `m_sEventName` - Название ивента
  - `m_iSpawnChance` - Шанс спавна (0-100%)
  - `m_iMinIntervalHours` / `m_iMaxIntervalHours` - Интервалы между ивентами
  - `m_iDurationHours` - Длительность ивента
  - `m_iPriority` - Приоритет ивента
  - `m_bEnabled` - Включен/выключен
  - `m_bAllowSimultaneous` - Разрешить одновременно с другими
  - Time constraints для ограничения по игровому времени

##### TSE_ConvoyEventConfig
- **Location**: `Scripts/Game/Events/Config/TSE_ConvoyEventConfig.c`
- **Purpose**: Конфигурация для convoy events
- **Extends**: TSE_BaseEventConfig
- **Additional Properties**:
  - `m_CargoConfig` - Конфигурация груза
  - `m_iSuccessReward` - Награда за успех
  - `m_fDestinationTolerance` - Дистанция до цели
  - `m_fMarkerUpdateInterval` - Интервал обновления маркера

##### TSE_SmuglersEventConfig
- **Location**: `Scripts/Game/Events/Config/TSE_SmuglersEventConfig.c`
- **Purpose**: Конфигурация для smugglers events
- **Extends**: TSE_BaseEventConfig
- **Additional Properties**:
  - `m_CrateConfig` - Конфигурация ящика
  - `m_fCleanupRadius` - Радиус очистки техники
  - `m_fMonitoringInterval` - Интервал мониторинга

## Event System Flow

1. **Initialization**
   - Event Manager инициализируется в геймоде
   - Находит существующие event components
   - Ждет задержку перед началом работы

2. **Event Checking**
   - Каждые N секунд проверяет возможность создания новых ивентов
   - Проверяет лимиты одновременных ивентов
   - Применяет глобальный cooldown

3. **Event Spawning**
   - Рассчитывает шансы для каждого типа ивента
   - Проверяет временные ограничения
   - Вызывает `StartEventFromManager()` у соответствующего компонента

4. **Event Monitoring**
   - Отслеживает активные ивенты
   - Убирает завершенные ивенты из списка активных

## Configuration

### Game Mode Setup
Event Manager добавлен в `Prefabs/GameMode/OVT_OverthrowGameMode.et`:

```
TSE_EventManagerComponent {
  m_ManagerConfig TSE_EventManagerConfig {
    m_fEventCheckInterval 1800      // 30 minutes
    m_iMaxSimultaneousEvents 2      // Max 2 events
    m_iMinGlobalCooldown 4          // 4 hours between any events
    m_bEventSystemEnabled 1         // Enabled
    m_iInitialDelayMinutes 8        // 8 minute initial delay
    m_bDebugLogging 1              // Debug logs
  }
  m_ConvoyConfig TSE_ConvoyEventConfig { ... }
  m_SmuglersConfig TSE_SmuglersEventConfig { ... }
}
```

### Event-Specific Settings

#### Convoy Events
- **Spawn Chance**: 60%
- **Interval**: 18-36 hours
- **Duration**: 8 hours
- **Priority**: 2 (high)
- **Simultaneous**: No (только один convoy)

#### Smugglers Events
- **Spawn Chance**: 40%
- **Interval**: 12-30 hours
- **Duration**: 12 hours
- **Priority**: 1 (normal)
- **Simultaneous**: Yes (можно несколько одновременно)

## Adding New Events

### 1. Create Event Configuration Class
```cpp
[BaseContainerProps()]
class TSE_MyNewEventConfig : TSE_BaseEventConfig
{
    [Attribute("", desc: "My custom property")]
    string m_sCustomProperty;
    
    void TSE_MyNewEventConfig()
    {
        m_sEventName = "My New Event";
        m_iSpawnChance = 30;
        // ... other defaults
    }
}
```

### 2. Create Event Component
```cpp
class TSE_MyNewEventManagerComponent : ScriptComponent
{
    // Add these methods for Event Manager integration:
    void StartEventFromManager(TSE_MyNewEventConfig config) { ... }
    bool IsEventActive() { return m_bEventActive; }
}
```

### 3. Update Event Manager
Add new event support to `TSE_EventManagerComponent`:
- Add config property
- Add TryStartEvent call  
- Add StartEvent case
- Add cleanup case

### 4. Update Game Mode
Add new event to `OVT_OverthrowGameMode.et`

## Backward Compatibility

Existing event components (TSE_ConvoyEventManagerComponent, TSE_SmuglersEventManagerComponent) сохраняют свою функциональность для backward compatibility. Если Event Manager не найден, они работают в legacy mode и запускаются самостоятельно.

## Debug Features

- **Logging**: Подробные логи всех операций (включается через `m_bDebugLogging`)
- **Public Interface**: Методы для проверки состояния системы:
  - `GetActiveEventCount()` - количество активных ивентов
  - `GetActiveEventTypes()` - типы активных ивентов  
  - `CanStartEvent(type)` - можно ли запустить ивент типа

## Configuration Files

Примеры конфигураций:
- `Configs/Events/TSE_EventManagerConfig.conf`
- `Configs/Events/TSE_ConvoyEventConfig.conf`
- `Configs/Events/TSE_SmuglersEventConfig.conf`

Эти файлы можно использовать как шаблоны для создания собственных конфигураций ивентов. 