class OVT_BuildMenuCardComponent : SCR_ScriptedWidgetComponent
{
	protected OVT_Buildable m_Buildable;
	protected OVT_BuildContext m_Context;
	
	void Init(OVT_Buildable buildable, OVT_BuildContext context)
	{
		m_Buildable = buildable;
		m_Context = context;
		
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		
		TextWidget text = TextWidget.Cast(m_wRoot.FindAnyWidget("EntityName"));
		text.SetText(buildable.m_sTitle);
		
		TextWidget cost = TextWidget.Cast(m_wRoot.FindAnyWidget("Cost"));
		cost.SetText("$" + config.GetBuildableCost(buildable));
		
		ImageWidget img = ImageWidget.Cast(m_wRoot.FindAnyWidget("Image"));
		img.LoadImageTexture(0, buildable.m_tPreview);
	}
	
	override bool OnClick(Widget w, int x, int y, int button)
	{
		super.OnClick(w, x, y, button);
		if (button != 0)
			return false;
		
		m_Context.StartBuild(m_Buildable);
		
		return true;
	}
}