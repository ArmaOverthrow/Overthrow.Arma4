[ComponentEditorProps(category: "Overthrow/Managers/Modded", description: "TSE Event Manager - manages all game events")]
class TSE_EventManagerComponentClass : OVT_ComponentClass {}

// Event instance tracking
class TSE_ActiveEvent
{
    string m_sEventType;
    EntityID m_EventComponentID;
    float m_fStartTime;
    ref TSE_BaseEventConfig m_Config;
    
    void TSE_ActiveEvent(string eventType, EntityID componentID, float startTime, TSE_BaseEventConfig config)
    {
        m_sEventType = eventType;
        m_EventComponentID = componentID;
        m_fStartTime = startTime;
        m_Config = config;
    }
}

class TSE_EventManagerComponent : OVT_Component
{
    [Attribute("", UIWidgets.Object, desc: "Event Manager Configuration")]
    ref TSE_EventManagerConfig m_ManagerConfig;
    
    [Attribute("", UIWidgets.Object, desc: "Convoy Event Configuration")]
    ref TSE_ConvoyEventConfig m_ConvoyConfig;
    
    [Attribute("", UIWidgets.Object, desc: "Smugglers Event Configuration")]
    ref TSE_SmuglersEventConfig m_SmuglersConfig;
    
    // Event system state
    protected ref array<ref TSE_ActiveEvent> m_ActiveEvents;
    protected float m_fLastEventTime;
    protected float m_fLastCheckTime;
    protected bool m_bInitialized;
    
    // Component references
    protected TSE_ConvoyEventManagerComponent m_ConvoyEventComponent;
    protected TSE_SmuglersEventManagerComponent m_SmuglersEventComponent;
    
    override void OnPostInit(IEntity owner)
    {
        super.OnPostInit(owner);
        
        if (!Replication.IsServer())
            return;
            
        // Initialize default configs if not set
        if (!m_ManagerConfig)
            m_ManagerConfig = new TSE_EventManagerConfig();
        if (!m_ConvoyConfig)
            m_ConvoyConfig = new TSE_ConvoyEventConfig();
        if (!m_SmuglersConfig)
            m_SmuglersConfig = new TSE_SmuglersEventConfig();
            
        m_ActiveEvents = new array<ref TSE_ActiveEvent>();
        m_fLastEventTime = 0;
        m_fLastCheckTime = 0;
        m_bInitialized = false;
        
        LogEventManager("Event Manager initialized");
        
        // Wait for game mode to be ready
        GetGame().GetCallqueue().CallLater(WaitForGameModeReady, 1000, false);
    }
    
    void WaitForGameModeReady()
    {
        OVT_OverthrowGameMode gameMode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
        if (!gameMode || !gameMode.IsInitialized())
        {
            GetGame().GetCallqueue().CallLater(WaitForGameModeReady, 1000, false);
            return;
        }
        
        // Find event components
        m_ConvoyEventComponent = TSE_ConvoyEventManagerComponent.Cast(gameMode.FindComponent(TSE_ConvoyEventManagerComponent));
        m_SmuglersEventComponent = TSE_SmuglersEventManagerComponent.Cast(gameMode.FindComponent(TSE_SmuglersEventManagerComponent));
        
        if (!m_ConvoyEventComponent)
            LogEventManager("WARNING: ConvoyEventManagerComponent not found!");
        if (!m_SmuglersEventComponent)
            LogEventManager("WARNING: SmuglersEventManagerComponent not found!");
            
        m_bInitialized = true;
        
        // Start event management after initial delay
        int delayMs = m_ManagerConfig.m_iInitialDelayMinutes * 60 * 1000;
        GetGame().GetCallqueue().CallLater(StartEventManagement, delayMs, false);
        
        LogEventManager("Event Manager ready, starting in " + m_ManagerConfig.m_iInitialDelayMinutes + " minutes");
    }
    
    void StartEventManagement()
    {
        if (!m_ManagerConfig.m_bEventSystemEnabled)
        {
            LogEventManager("Event system disabled in config");
            return;
        }
        
        LogEventManager("Event management started");
        
        // Start periodic event checking
        float checkIntervalMs = m_ManagerConfig.m_fEventCheckInterval * 1000;
        GetGame().GetCallqueue().CallLater(CheckForNewEvents, checkIntervalMs, true);
    }
    
    void CheckForNewEvents()
    {
        if (!m_bInitialized || !m_ManagerConfig.m_bEventSystemEnabled)
            return;
            
        float currentTime = GetGame().GetWorld().GetWorldTime();
        
        // Clean up finished events
        CleanupFinishedEvents();
        
        // Check global cooldown
        if (currentTime - m_fLastEventTime < m_ManagerConfig.m_iMinGlobalCooldown * 3600)
        {
            LogEventManager("Global cooldown active, skipping event check");
            return;
        }
        
        // Check if we can start new events
        if (m_ActiveEvents.Count() >= m_ManagerConfig.m_iMaxSimultaneousEvents)
        {
            LogEventManager("Maximum simultaneous events reached (" + m_ActiveEvents.Count() + ")");
            return;
        }
        
        // Try to start events based on priority and conditions
        TryStartEvent("convoy", m_ConvoyConfig);
        TryStartEvent("smugglers", m_SmuglersConfig);
        
        m_fLastCheckTime = currentTime;
    }
    
    void TryStartEvent(string eventType, TSE_BaseEventConfig config)
    {
        if (!config || !config.m_bEnabled)
            return;
            
        // Check if event is already running (for non-simultaneous events)
        if (!config.m_bAllowSimultaneous && IsEventTypeActive(eventType))
        {
            LogEventManager(eventType + " event already active and doesn't allow simultaneous instances");
            return;
        }
        
        // Check game time constraints
        float gameTimeHours = GetGameTimeHours();
        if (gameTimeHours < config.m_iMinGameTimeHours || gameTimeHours > config.m_iMaxGameTimeHours)
        {
            LogEventManager(eventType + " event outside time constraints");
            return;
        }
        
        // Roll for spawn chance
        int roll = Math.RandomInt(0, 100);
        if (roll >= config.m_iSpawnChance)
        {
            LogEventManager(eventType + " event failed spawn chance roll: " + roll + "/" + config.m_iSpawnChance);
            return;
        }
        
        // Check interval constraints
        float timeSinceLastEvent = GetTimeSinceLastEventOfType(eventType);
        float minInterval = config.m_iMinIntervalHours * 3600;
        if (timeSinceLastEvent < minInterval)
        {
            LogEventManager(eventType + " event on cooldown for " + (minInterval - timeSinceLastEvent) / 3600 + " more hours");
            return;
        }
        
        // Start the event
        StartEvent(eventType, config);
    }
    
    void StartEvent(string eventType, TSE_BaseEventConfig config)
    {
        LogEventManager("Starting " + eventType + " event");
        
        float currentTime = GetGame().GetWorld().GetWorldTime();
        EntityID componentID = EntityID.INVALID;
        
        // Start the appropriate event component
        if (eventType == "convoy" && m_ConvoyEventComponent)
        {
            m_ConvoyEventComponent.StartEventFromManager(TSE_ConvoyEventConfig.Cast(config));
            componentID = m_ConvoyEventComponent.GetOwner().GetID();
        }
        else if (eventType == "smugglers" && m_SmuglersEventComponent)
        {
            m_SmuglersEventComponent.StartEventFromManager(TSE_SmuglersEventConfig.Cast(config));
            componentID = m_SmuglersEventComponent.GetOwner().GetID();
        }
        else
        {
            LogEventManager("ERROR: Unknown event type or component not found: " + eventType);
            return;
        }
        
        // Track the active event
        TSE_ActiveEvent activeEvent = new TSE_ActiveEvent(eventType, componentID, currentTime, config);
        m_ActiveEvents.Insert(activeEvent);
        
        m_fLastEventTime = currentTime;
        
        LogEventManager("Successfully started " + eventType + " event");
    }
    
    void CleanupFinishedEvents()
    {
        for (int i = m_ActiveEvents.Count() - 1; i >= 0; i--)
        {
            TSE_ActiveEvent activeEvent = m_ActiveEvents[i];
            if (!activeEvent)
            {
                m_ActiveEvents.Remove(i);
                continue;
            }
            
            // Check if event component still reports as active
            bool stillActive = false;
            
            if (activeEvent.m_sEventType == "convoy" && m_ConvoyEventComponent)
                stillActive = m_ConvoyEventComponent.IsEventActive();
            else if (activeEvent.m_sEventType == "smugglers" && m_SmuglersEventComponent)
                stillActive = m_SmuglersEventComponent.IsEventActive();
                
            if (!stillActive)
            {
                LogEventManager("Event finished: " + activeEvent.m_sEventType);
                m_ActiveEvents.Remove(i);
            }
        }
    }
    
    bool IsEventTypeActive(string eventType)
    {
        foreach (TSE_ActiveEvent activeEvent : m_ActiveEvents)
        {
            if (activeEvent && activeEvent.m_sEventType == eventType)
                return true;
        }
        return false;
    }
    
    float GetTimeSinceLastEventOfType(string eventType)
    {
        // For now, just return a large value to allow events
        // TODO: Implement proper event history tracking
        return 999999;
    }
    
    float GetGameTimeHours()
    {
        ChimeraWorld world = GetGame().GetWorld();
        if (!world)
            return 0;
            
        TimeAndWeatherManagerEntity timeManager = world.GetTimeAndWeatherManager();
        if (!timeManager)
            return 0;
            
        TimeContainer time = timeManager.GetTime();
        return time.m_iHours + (time.m_iMinutes / 60.0);
    }
    
    void LogEventManager(string message)
    {
        if (m_ManagerConfig && m_ManagerConfig.m_bDebugLogging)
            Print("[TSE_EventManager] " + message);
    }
    
    // Public interface methods
    int GetActiveEventCount()
    {
        return m_ActiveEvents.Count();
    }
    
    array<string> GetActiveEventTypes()
    {
        array<string> types = new array<string>();
        foreach (TSE_ActiveEvent activeEvent : m_ActiveEvents)
        {
            if (activeEvent)
                types.Insert(activeEvent.m_sEventType);
        }
        return types;
    }
    
    bool CanStartEvent(string eventType)
    {
        if (!m_bInitialized || !m_ManagerConfig.m_bEventSystemEnabled)
            return false;
            
        if (m_ActiveEvents.Count() >= m_ManagerConfig.m_iMaxSimultaneousEvents)
            return false;
            
        TSE_BaseEventConfig config;
        if (eventType == "convoy")
            config = m_ConvoyConfig;
        else if (eventType == "smugglers")
            config = m_SmuglersConfig;
        else
            return false;
            
        if (!config || !config.m_bEnabled)
            return false;
            
        if (!config.m_bAllowSimultaneous && IsEventTypeActive(eventType))
            return false;
            
        return true;
    }
} 