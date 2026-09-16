class OVT_JobsContext : OVT_UIContext
{
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Layout for job items", params: "layout")]
	ResourceName m_JobLayout;

	OVT_JobManagerComponent m_JobManager;
	OVT_Job m_SelectedJob;
	OVT_JobConfig m_Selected

	//! Lowercased text typed into the search box. Empty means no text filter is active.
	protected string m_sSearchFilter = "";

	override void PostInit()
	{
		m_JobManager = OVT_Global.GetJobs();
	}

	override void OnShow()
	{
		Widget closeButton = m_wRoot.FindAnyWidget("CloseButton");
		if (closeButton)
		{
			SCR_InputButtonComponent action = SCR_InputButtonComponent.Cast(closeButton.FindHandler(SCR_InputButtonComponent));
			if (action)
				action.m_OnActivated.Insert(CloseLayout);
		}

		Widget ww = m_wRoot.FindAnyWidget("ShowOnMap");
		SCR_InputButtonComponent btn = SCR_InputButtonComponent.Cast(ww.FindHandler(SCR_InputButtonComponent));
		btn.m_OnActivated.Insert(ShowOnMap);

		ww = m_wRoot.FindAnyWidget("Accept");
		btn = SCR_InputButtonComponent.Cast(ww.FindHandler(SCR_InputButtonComponent));
		btn.m_OnActivated.Insert(Accept);

		ww = m_wRoot.FindAnyWidget("Decline");
		btn = SCR_InputButtonComponent.Cast(ww.FindHandler(SCR_InputButtonComponent));
		btn.m_OnActivated.Insert(Decline);

		BuildSearchBox();

		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	//! Wires the search box's live-change event. SCR_EditBoxComponent.m_OnChanged fires on every
	//! keystroke, on-screen keyboard included, so a controller player typing with the pad keyboard
	//! gets the same live filtering as a mouse-and-keyboard player.
	protected void BuildSearchBox()
	{
		SCR_EditBoxComponent editBox = SCR_EditBoxComponent.GetEditBoxComponent("SearchBox", m_wRoot);
		if(!editBox)
			return;

		editBox.m_OnChanged.Insert(OnSearchChanged);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSearchChanged(SCR_EditBoxComponent comp, string value)
	{
		value.ToLower();
		m_sSearchFilter = value;
		m_SelectedJob = null;
		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	//! A town job's effective town is its own townId. A base job has no townId of its own, so its
	//! effective town is the town nearest the base - the same value the detail panel and the job
	//! card both already show as the job's location. Some jobs (OVT_JobManagerComponent's "not tied
	//! to town/base" case) have neither: townId == -1 AND baseId == -1. That is not an error, so
	//! this returns -1 rather than indexing m_Bases with an invalid id. Callers must treat -1 as
	//! "no town" and skip any GetTownName(-1) lookup, which would itself be out of range.
	protected int GetEffectiveTownId(OVT_Job job)
	{
		if(job.townId != -1)
			return job.townId;

		if(job.baseId == -1)
			return -1;

		OVT_BaseData base = OVT_Global.GetOccupyingFaction().m_Bases[job.baseId];
		if(!base)
			return -1;

		OVT_TownData town = OVT_Global.GetTowns().GetNearestTown(base.location);
		return OVT_Global.GetTowns().GetTownID(town);
	}

	//------------------------------------------------------------------------------------------------
	//! Clears the detail panel and hides its action buttons for the "filters hid every job" case.
	//! Kept separate from the true-empty-job-list branch in Refresh, which has its own long-standing
	//! (and unrelated) behaviour that this change does not touch.
	protected void ShowNoMatchesPanel()
	{
		TextWidget title = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedJobName"));
		if(title)
			title.SetText("");

		TextWidget location = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedLocation"));
		if(location)
			location.SetText("");

		TextWidget details = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedDetails"));
		if(details)
			details.SetText("");

		TextWidget desc = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedDescription"));
		if(desc)
			desc.SetText("#OVT-Jobs_NoMatches");

		Widget w = m_wRoot.FindAnyWidget("ShowOnMap");
		if(w)
			w.SetVisible(false);

		w = m_wRoot.FindAnyWidget("Accept");
		if(w)
			w.SetVisible(false);

		w = m_wRoot.FindAnyWidget("Decline");
		if(w)
			w.SetVisible(false);
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

		int shownCount = 0;
		foreach(int i,OVT_Job job : m_JobManager.m_aJobs)
		{
			OVT_JobConfig config = m_JobManager.GetConfig(job.jobIndex);
			if(job.owner != "" && job.owner != m_sPlayerID) continue;
			if(job.declined.Contains(m_sPlayerID)) continue;
			if(m_sSearchFilter != "")
			{
				string title = WidgetManager.Translate(config.m_sTitle);
				title.ToLower();

				string townName = "";
				int effectiveTownId = GetEffectiveTownId(job);
				if(effectiveTownId != -1)
					townName = OVT_Global.GetTowns().GetTownName(effectiveTownId);
				townName.ToLower();

				if(title.IndexOf(m_sSearchFilter) == -1 && townName.IndexOf(m_sSearchFilter) == -1) continue;
			}

			Widget w = workspace.CreateWidgets(m_JobLayout, container);

			OVT_JobListEntryHandler handler = OVT_JobListEntryHandler.Cast(w.FindHandler(OVT_JobListEntryHandler));

			handler.Populate(job, config);

			handler.m_OnClicked.Insert(OnJobClicked);
			if(!m_SelectedJob)
			{
				OnJobClicked(handler);
			}
			shownCount++;
		}

		if(shownCount == 0)
			ShowNoMatchesPanel();
	}
	
	protected void Accept()
	{
		OVT_Global.GetJobs().AcceptJob(m_SelectedJob, m_iPlayerID);
		Refresh();
		SelectJob(m_SelectedJob);
	}
	
	protected void Decline()
	{
		OVT_Global.GetJobs().DeclineJob(m_SelectedJob, m_iPlayerID);
		m_SelectedJob = null;
		Refresh();
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
			int effectiveTownId = GetEffectiveTownId(job);
			if(effectiveTownId == -1)
			{
				location.SetText("");
			}else{
				location.SetText("#OVT-BaseNear " + OVT_Global.GetTowns().GetTownName(effectiveTownId));
			}
		}else{
			location.SetText(OVT_Global.GetTowns().GetTownName(job.townId));
		}
		
		TextWidget details = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedDetails"));
		details.SetText("$" + m_Selected.m_iReward.ToString() + " + " + m_Selected.m_iRewardXP.ToString() + " XP");
		
		TextWidget desc = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedDescription"));
		desc.SetText(m_Selected.m_sDescription);
	}
}