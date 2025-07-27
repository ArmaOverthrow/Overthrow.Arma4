[BaseContainerProps()]
class TSE_EventManagerConfig
{
    [Attribute(defvalue: "3600", desc: "Event check interval (seconds) - how often manager checks for new events")]
    float m_fEventCheckInterval;
    
    [Attribute(defvalue: "2", desc: "Maximum simultaneous events")]
    int m_iMaxSimultaneousEvents;
    
    [Attribute(defvalue: "6", desc: "Minimum hours between any events")]
    int m_iMinGlobalCooldown;
    
    [Attribute(defvalue: "true", desc: "Enable event system")]
    bool m_bEventSystemEnabled;
    
    [Attribute(defvalue: "480", desc: "Initial delay before first event check (minutes)")]
    int m_iInitialDelayMinutes;
    
    [Attribute(defvalue: "true", desc: "Enable debug logging")]
    bool m_bDebugLogging;
    
    void TSE_EventManagerConfig()
    {
        // Default values are set in attribute declarations
    }
} 