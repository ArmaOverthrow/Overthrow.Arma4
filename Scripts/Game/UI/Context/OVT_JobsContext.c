class OVT_JobsContext : OVT_UIContext
{
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Layout for job items", params: "layout")]
	ResourceName m_JobLayout;
	
	OVT_JobManagerComponent m_JobManager;
	OVT_Job m_SelectedJob;
	OVT_JobConfig m_Selected
		
	override void PostInit()
	{		
		m_JobManager = OVT_Global.GetJobs();
	}
	
	override void OnShow()
	{	
		Widget show = m_wRoot.FindAnyWidget("ShowOnMap");
		ButtonActionComponent btn = ButtonActionComponent.Cast(show.FindHandler(ButtonActionComponent));
		
		btn.GetOnAction().Insert(ShowOnMap);
		
		show = m_wRoot.FindAnyWidget("Accept");
		btn = ButtonActionComponent.Cast(show.FindHandler(ButtonActionComponent));
		
		btn.GetOnAction().Insert(Accept);
		
		show = m_wRoot.FindAnyWidget("Decline");
		btn = ButtonActionComponent.Cast(show.FindHandler(ButtonActionComponent));
		
		btn.GetOnAction().Insert(Decline);
					
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
		
		foreach(int i,OVT_Job job : m_JobManager.m_aJobs)
		{
			OVT_JobConfig config = m_JobManager.GetConfig(job.jobIndex);
			if(job.owner != "" && job.owner != m_sPlayerID) continue;
			
			Widget w = workspace.CreateWidgets(m_JobLayout, container);
			
			OVT_JobListEntryHandler handler = OVT_JobListEntryHandler.Cast(w.FindHandler(OVT_JobListEntryHandler));
			
			handler.Populate(job, config);
			
			handler.m_OnClicked.Insert(OnJobClicked);
			if(!m_SelectedJob)
			{
				OnJobClicked(handler);
			}
		}
	}
	
	protected void Accept()
	{
		OVT_Global.GetJobs().AcceptJob(m_SelectedJob, m_iPlayerID);
		Refresh();
		SelectJob(m_SelectedJob);
	}
	
	protected void Decline()
	{
	
	}
	
	protected void ShowOnMap()
	{
		m_JobManager.m_vCurrentWaypoint = m_SelectedJob.location;
				
		CloseLayout();
		ShowHint("#OVT-Job_AddedWaypoint");
	}
	
	protected void OnJobClicked(SCR_ButtonBaseComponent btn)
	{				
		OVT_JobListEntryHandler handler = OVT_JobListEntryHandler.Cast(btn);
		SelectJob(handler.m_Job);
	}
	
	protected void SelectJob(OVT_Job job)
	{		
		m_SelectedJob = job;
		m_Selected = OVT_Global.GetJobs().GetConfig(job.jobIndex);
		
		if(job.accepted)
		{
			m_wRoot.FindAnyWidget("ShowOnMap").SetVisible(true);			
			m_wRoot.FindAnyWidget("Accept").SetVisible(false);
			m_wRoot.FindAnyWidget("Decline").SetVisible(false);
		}else{
			m_wRoot.FindAnyWidget("ShowOnMap").SetVisible(false);			
			m_wRoot.FindAnyWidget("Accept").SetVisible(true);
			m_wRoot.FindAnyWidget("Decline").SetVisible(true);
		}
		
		TextWidget title = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedJobName"));
		title.SetText(m_Selected.m_sTitle);
		
		TextWidget location = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedLocation"));
		if(job.townId == -1)
		{
			OVT_BaseData base = OVT_Global.GetOccupyingFaction().m_Bases[job.baseId];
			OVT_TownData town = OVT_Global.GetTowns().GetNearestTown(base.location);
			location.SetText("#OVT-BaseNear " + OVT_Global.GetTowns().GetTownName(town.id));
		}else{
			location.SetText(OVT_Global.GetTowns().GetTownName(job.townId));
		}
		
		TextWidget details = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedDetails"));
		details.SetText("$" + m_Selected.m_iReward.ToString());
		
		TextWidget desc = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedDescription"));
		desc.SetText(m_Selected.m_sDescription);
	}
}