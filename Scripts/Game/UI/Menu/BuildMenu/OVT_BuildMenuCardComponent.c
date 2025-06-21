class OVT_BuildMenuCardComponent : SCR_ScriptedWidgetComponent
{
	protected OVT_Buildable m_Buildable;
	protected OVT_BuildContext m_Context;
	protected bool m_bCanBuild;
	protected string m_sRestrictionReason;
	
	void Init(OVT_Buildable buildable, OVT_BuildContext context, bool canBuild = true, string reason = "")
	{
		m_Buildable = buildable;
		m_Context = context;
		m_bCanBuild = canBuild;
		m_sRestrictionReason = reason;
		
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		
		TextWidget text = TextWidget.Cast(m_wRoot.FindAnyWidget("EntityName"));
		text.SetText(buildable.m_sTitle);
		
		TextWidget cost = TextWidget.Cast(m_wRoot.FindAnyWidget("Cost"));
		cost.SetText("$" + config.GetBuildableCost(buildable));
		
		ImageWidget img = ImageWidget.Cast(m_wRoot.FindAnyWidget("Image"));
		img.LoadImageTexture(0, buildable.m_tPreview);
		
		TextWidget desc = TextWidget.Cast(m_wRoot.FindAnyWidget("EntityDescription"));
		if (!m_bCanBuild && m_sRestrictionReason != "")
		{
			// Show restriction reason in description
			desc.SetText(buildable.m_sDescription + "\n\n" + m_sRestrictionReason);
		}
		else
		{
			desc.SetText(buildable.m_sDescription);
		}
		
		// Visual feedback for non-buildable items
		if (!m_bCanBuild)
		{
			m_wRoot.SetOpacity(0.5);
			m_wRoot.SetColor(Color.FromInt(0xFFFF4444)); // Red tint
		}
		else
		{
			m_wRoot.SetOpacity(1.0);
			m_wRoot.SetColor(Color.White);
		}
	}
	
	override bool OnClick(Widget w, int x, int y, int button)
	{
		super.OnClick(w, x, y, button);
		if (button != 0)
			return false;
		
		if(!m_Context) return false;
		
		if (!m_bCanBuild)
		{
			// Show restriction reason instead of attempting to build
			m_Context.ShowHint(m_sRestrictionReason);
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.ERROR);
			return true;
		}
		
		m_Context.StartBuild(m_Buildable);
		
		return true;
	}
}