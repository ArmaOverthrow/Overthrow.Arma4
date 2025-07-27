[BaseContainerProps()]
class TSE_BaseEventConfig
{
    [Attribute(defvalue: "Unknown Event", desc: "Event display name")]
    string m_sEventName;
    
    [Attribute(defvalue: "50", desc: "Spawn chance percentage (0-100)")]
    int m_iSpawnChance;
    
    [Attribute(defvalue: "24", desc: "Minimum interval between events (hours)")]
    int m_iMinIntervalHours;
    
    [Attribute(defvalue: "48", desc: "Maximum interval between events (hours)")]
    int m_iMaxIntervalHours;
    
    [Attribute(defvalue: "12", desc: "Event duration (hours)")]
    int m_iDurationHours;
    
    [Attribute(defvalue: "1", desc: "Event priority (higher = more important)")]
    int m_iPriority;
    
    [Attribute(defvalue: "true", desc: "Is event enabled")]
    bool m_bEnabled;
    
    [Attribute(defvalue: "false", desc: "Can this event run simultaneously with others")]
    bool m_bAllowSimultaneous;
    
    [Attribute(defvalue: "0", desc: "Minimum game time hours before this event can start")]
    int m_iMinGameTimeHours;
    
    [Attribute(defvalue: "999999", desc: "Maximum game time hours after which this event stops spawning")]
    int m_iMaxGameTimeHours;
} 