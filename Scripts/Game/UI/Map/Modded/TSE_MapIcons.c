[ComponentEditorProps(category: "GameScripted/UI", description: "TSE Map Icons - extends OVT_MapIcons with convoy marker support")]
modded class OVT_MapIcons : SCR_MapUIBaseComponent
{
    // Convoy marker tracking
    protected int m_iConvoyMarkerIndex = -1;
    protected Widget m_ConvoyMarkerWidget = null;
    
    override void OnMapOpen(MapConfiguration config)
    {
        super.OnMapOpen(config);
        
        // Add convoy marker if active
        AddConvoyMarker();
    }
    
    void AddConvoyMarker()
    {
        // Check if convoy marker is visible
        if (!TSE_ConvoyEventManagerComponent.IsConvoyMarkerVisible())
        {
            // Remove convoy marker if it exists but convoy is not visible
            RemoveConvoyMarker();
            return;
        }
            
        vector convoyPos = TSE_ConvoyEventManagerComponent.GetActiveConvoyPosition();
        if (convoyPos == vector.Zero)
        {
            RemoveConvoyMarker();
            return;
        }
        
        // If convoy marker already exists, just update position
        if (m_iConvoyMarkerIndex != -1 && m_iConvoyMarkerIndex < m_Centers.Count())
        {
            m_Centers[m_iConvoyMarkerIndex] = convoyPos;
            return;
        }
            
        // Create new convoy marker widget
        Widget w = GetGame().GetWorkspace().CreateWidgets(m_Layout, m_RootWidget);
        if (!w) return;
        
        ImageWidget image = ImageWidget.Cast(w.FindAnyWidget("Image"));
        if (image)
        {
            // Use a distinct convoy icon
            image.LoadImageFromSet(0, m_Imageset, "waypoint");
            image.SetColor(Color.Red); // Make it red to stand out
        }
        
        // Add to tracking arrays and remember index
        m_Centers.Insert(convoyPos);
        m_Ranges.Insert(0); // Always visible (like radio towers)
        m_Widgets.Insert(w);
        
        m_iConvoyMarkerIndex = m_Centers.Count() - 1;
        m_ConvoyMarkerWidget = w;
        
        Print("[TSE_MapIcons] Added convoy marker at: " + convoyPos + " (index: " + m_iConvoyMarkerIndex + ")");
    }
    
    void RemoveConvoyMarker()
    {
        if (m_iConvoyMarkerIndex == -1 || m_iConvoyMarkerIndex >= m_Centers.Count())
            return;
            
        // Remove widget from hierarchy
        if (m_ConvoyMarkerWidget)
        {
            m_ConvoyMarkerWidget.RemoveFromHierarchy();
        }
        
        // Remove from arrays (this is tricky as it shifts indices)
        // For simplicity, we'll just mark it as invalid and let OnMapClose clean up
        m_iConvoyMarkerIndex = -1;
        m_ConvoyMarkerWidget = null;
        
        Print("[TSE_MapIcons] Removed convoy marker");
    }
    
    override void Update(float timeSlice)
    {
        super.Update(timeSlice);
        
        // Update convoy marker position if it exists and is still active
        UpdateConvoyMarker();
    }
    
    void UpdateConvoyMarker()
    {
        // Check if convoy is still visible
        if (!TSE_ConvoyEventManagerComponent.IsConvoyMarkerVisible())
        {
            RemoveConvoyMarker();
            return;
        }
            
        vector newConvoyPos = TSE_ConvoyEventManagerComponent.GetActiveConvoyPosition();
        if (newConvoyPos == vector.Zero)
        {
            RemoveConvoyMarker();
            return;
        }
        
        // Update position if we have a valid convoy marker
        if (m_iConvoyMarkerIndex != -1 && m_iConvoyMarkerIndex < m_Centers.Count())
        {
            m_Centers[m_iConvoyMarkerIndex] = newConvoyPos;
        }
        else
        {
            // Convoy marker lost, try to re-add it
            AddConvoyMarker();
        }
    }
    
    override void OnMapClose(MapConfiguration config)
    {
        // Clean up convoy marker tracking
        m_iConvoyMarkerIndex = -1;
        m_ConvoyMarkerWidget = null;
        
        super.OnMapClose(config);
    }
} 