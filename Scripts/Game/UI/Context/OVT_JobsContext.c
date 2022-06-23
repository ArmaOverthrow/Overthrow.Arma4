class OVT_JobsContext : OVT_UIContext
{
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Layout for job items", params: "layout")]
	ResourceName m_JobLayout;
	
	OVT_JobManagerComponent m_JobManager;
	OVT_JobListEntryHandler m_Selected;
		
	override void PostInit()
	{		
		m_JobManager = OVT_Global.GetJobs();
	}
	
	override void OnShow()
	{				
		Refresh();		
	}
	
	protected void Refresh()
	{
		Widget container = m_wRoot.FindAnyWidget("BrowserLayout");
		
		Widget child = container.GetChildren();
		while(child)
		{
			container.RemoveChild(child);
			child = container.GetChildren();
		}

		WorkspaceWidget workspace = GetGame().GetWorkspace(); 
		
		if(m_JobManager.m_aJobs.Count() == 0)
		{
			TextWidget title = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedJobName"));
			title.SetText("");
			
			TextWidget location = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedLocation"));
			location.SetText("");
			
			TextWidget details = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedDetails"));
			details.SetText("");
			
			TextWidget desc = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedDescription"));
			desc.SetText("#OVT-Jobs_NoJobs");
			
			Widget w = m_wRoot.FindAnyWidget("ShowOnMap");
			w.SetVisible(false);
			
			return;
		}
		
		Widget show = m_wRoot.FindAnyWidget("ShowOnMap");
		SCR_NavigationButtonComponent btn = SCR_NavigationButtonComponent.Cast(show.FindHandler(SCR_NavigationButtonComponent));
		
		btn.m_OnClicked.Insert(ShowOnMap);
		
		foreach(int i,OVT_Job job : m_JobManager.m_aJobs)
		{
			OVT_JobConfig config = m_JobManager.GetConfig(job.jobIndex);
			
			Widget w = workspace.CreateWidgets(m_JobLayout, container);
			
			OVT_JobListEntryHandler handler = OVT_JobListEntryHandler.Cast(w.FindHandler(OVT_JobListEntryHandler));
			
			handler.Populate(job, config);
			
			handler.m_OnClicked.Insert(OnJobClicked);
			if(!m_Selected)
			{
				OnJobClicked(handler);
			}
		}
	}
	
	protected void ShowOnMap()
	{
		m_JobManager.m_vCurrentWaypoint = m_Selected.m_Job.location;
				
		CloseLayout();
		ShowHint("#OVT-Job_AddedWaypoint");
	}
	
	protected void OnJobClicked(SCR_ButtonBaseComponent btn)
	{		
		OVT_JobListEntryHandler handler = OVT_JobListEntryHandler.Cast(btn);
		m_Selected = handler;
		
		TextWidget title = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedJobName"));
		title.SetText(handler.m_JobConfig.m_sTitle);
		
		TextWidget location = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedLocation"));
		location.SetText(OVT_Global.GetTowns().GetTownName(handler.m_Job.townId));
		
		TextWidget details = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedDetails"));
		details.SetText("$" + handler.m_JobConfig.m_iReward.ToString());
		
		TextWidget desc = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedDescription"));
		desc.SetText(handler.m_JobConfig.m_sDescription);
	}
}