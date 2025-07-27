[BaseContainerProps()]
class TSE_SmuglersEventConfig : TSE_BaseEventConfig
{
    [Attribute("", UIWidgets.Object, desc: "Smuggler crate content configuration")]
    ref TSE_SmuglerCrateContents m_CrateConfig;
    
    [Attribute(defvalue: "100", desc: "Vehicle cleanup radius around spawn markers (meters)")]
    float m_fCleanupRadius;
    
    [Attribute(defvalue: "900", desc: "Event monitoring interval (seconds)")]
    float m_fMonitoringInterval;
    
    void TSE_SmuglersEventConfig()
    {
        m_sEventName = "Smugglers Event";
        m_iSpawnChance = 40;
        m_iMinIntervalHours = 12;
        m_iMaxIntervalHours = 30;
        m_iDurationHours = 12;
        m_iPriority = 1;
        m_bAllowSimultaneous = true;
    }
} 