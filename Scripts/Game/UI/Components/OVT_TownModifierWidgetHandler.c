//! Widget handler for displaying town modifier information
//! Handles both support and stability modifiers with proper styling
class OVT_TownModifierWidgetHandler : SCR_ScriptedWidgetComponent
{
	protected TextWidget m_wModifierText;
	protected PanelWidget m_wBackground;
	
	protected ref OVT_TownModifierData m_ModifierData;
	protected ref OVT_ModifierConfig m_ModifierConfig;
	protected int m_iEffectValue;
	
	//! Colors for positive and negative modifiers
	[Attribute("1 1 1 0.3", UIWidgets.ColorPicker)]
	ref Color m_PositiveColor;
	
	[Attribute("1 1 1 0.3", UIWidgets.ColorPicker)]
	ref Color m_NegativeColor;
	
	//! Initialize the modifier widget with data
	void Init(OVT_TownModifierData modifierData, OVT_ModifierConfig modifierConfig, int effectValue)
	{
		m_ModifierData = modifierData;
		m_ModifierConfig = modifierConfig;
		m_iEffectValue = effectValue;
		
		UpdateDisplay();
	}
	
	//! Handle widget attachment and cache references
	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);
		
		if (!w)
			return;
		
		// Cache widget references
		m_wModifierText = TextWidget.Cast(w.FindAnyWidget("ModifierText"));
		m_wBackground = PanelWidget.Cast(w.FindAnyWidget("ModifierBackground"));
		
		// Update display if we already have data
		if (m_ModifierConfig)
			UpdateDisplay();
	}
	
	//! Update the visual display of the modifier
	protected void UpdateDisplay()
	{
		if (!m_wModifierText || !m_wBackground || !m_ModifierConfig)
			return;
		
		// Set the text with effect value and modifier title
		string effectText;
		if (m_iEffectValue > 0)
			effectText = "+" + m_iEffectValue.ToString() + "%";
		else
			effectText = m_iEffectValue.ToString() + "%";
		
		string displayText = effectText + " " + m_ModifierConfig.title;
		m_wModifierText.SetText(displayText);
		
		// Set background color based on effect
		Color backgroundColor;
		if (m_iEffectValue < 0)
			backgroundColor = m_NegativeColor;
		else
			backgroundColor = m_PositiveColor;
		
		m_wBackground.SetColor(backgroundColor);
	}
	
	//! Get the modifier data
	OVT_TownModifierData GetModifierData()
	{
		return m_ModifierData;
	}
	
	//! Get the modifier config
	OVT_ModifierConfig GetModifierConfig()
	{
		return m_ModifierConfig;
	}
	
	//! Get the calculated effect value
	int GetEffectValue()
	{
		return m_iEffectValue;
	}
}