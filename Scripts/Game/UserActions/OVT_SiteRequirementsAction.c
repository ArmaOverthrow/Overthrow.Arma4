//------------------------------------------------------------------------------------------------
//! "Requirements" - shows what a construction site still needs, in an information dialog.
//!
//! IT READS THE REPLICATED PILE CONTENTS, so it is correct on every machine with no round trip; the
//! server re-derives all of it before it consumes anything.
//!
//! Sort Priority 1, so it sits above the build action: reading what a thing costs comes before
//! paying for it.
//------------------------------------------------------------------------------------------------
class OVT_SiteRequirementsAction : ScriptedUserAction
{
	//! The Overthrow dialog presets; SITE_REQUIREMENTS is an OK-only information dialog.
	protected const ResourceName DIALOG_PRESETS = "{272B6C4030554E27}Configs/UI/Dialogs/DialogPresets_Campaign.conf";

	//! The open dialog, held so its handler can unsubscribe itself. The action outlives it.
	protected SCR_ConfigurableDialogUi m_Dialog;

	//------------------------------------------------------------------------------------------------
	//! \param[in] pOwnerEntity The construction site.
	//! \param[in] pUserEntity The acting character.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (m_Dialog)
			return;

		array<ref OVT_ResourceAmount> need = new array<ref OVT_ResourceAmount>();
		array<ref OVT_ResourceAmount> have = new array<ref OVT_ResourceAmount>();
		if (!OVT_SiteRequirementsReader.Read(pOwnerEntity, need, have))
			return;

		SCR_ConfigurableDialogUi dialog = SCR_ConfigurableDialogUi.CreateFromPreset(DIALOG_PRESETS, "SITE_REQUIREMENTS");
		if (!dialog)
			return;

		dialog.SetTitle(ResolveTitle());
		dialog.SetMessage(OVT_SiteRequirementsReader.FormatReadout(need, have));

		m_Dialog = dialog;
		m_Dialog.m_OnConfirm.Insert(ForgetDialog);
		m_Dialog.m_OnCancel.Insert(ForgetDialog);
	}

	//------------------------------------------------------------------------------------------------
	//! Hidden on a ruin and on anything that is not a site the client can read.
	//! \param[in] user The character looking at the action.
	//! \return True when the action may be drawn.
	override bool CanBeShownScript(IEntity user)
	{
		if (!OVT_StructureDamage.IsUsable(GetOwner()))
			return false;

		return OVT_SiteRequirementsReader.GetSite(GetOwner()) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[out] outName The label to draw.
	//! \return Always true.
	override bool GetActionNameScript(out string outName)
	{
		outName = "#OVT-Resource_SiteRequirements";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A dialog on the clicking machine and nothing else.
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The dialog title: the building's replicated name, or a generic one.
	protected string ResolveTitle()
	{
		OVT_ConstructionSiteComponent site = OVT_SiteRequirementsReader.GetSite(GetOwner());
		if (!site)
			return "#OVT-Resource_ConstructionSite";

		string name = site.GetBuildableName();
		if (name == "")
			return "#OVT-Resource_ConstructionSite";

		return name;
	}

	//------------------------------------------------------------------------------------------------
	//! Releases the dialog so the action can open another one.
	protected void ForgetDialog()
	{
		if (!m_Dialog)
			return;

		m_Dialog.m_OnConfirm.Remove(ForgetDialog);
		m_Dialog.m_OnCancel.Remove(ForgetDialog);
		m_Dialog = null;
	}
}

//------------------------------------------------------------------------------------------------
//! What both site actions need to know, in one place: the site, its scaled requirements, and what
//! the crate piles around it hold.
//!
//! CLIENT-SAFE THROUGHOUT. The requirement figures come from the same
//! OVT_ResourceRequirements.ScaleForDifficulty() call the server consumes by, and the availability
//! from the piles' replicated contents, so the label a player reads and the amount the server takes
//! are the same numbers.
//------------------------------------------------------------------------------------------------
class OVT_SiteRequirementsReader
{
	//------------------------------------------------------------------------------------------------
	//! \param[in] owner The candidate site entity.
	//! \return Its construction site component, or null.
	static OVT_ConstructionSiteComponent GetSite(IEntity owner)
	{
		return OVT_ComponentFinder<OVT_ConstructionSiteComponent>.Find(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Fills a site's scaled requirements and what is available around it.
	//! \param[in] owner The site entity.
	//! \param[out] need Receives the scaled requirements. Cleared by the scaler.
	//! \param[out] have Receives what the nearby piles hold. Cleared by the sweep.
	//! \return True when the site and its buildable both resolved.
	static bool Read(IEntity owner, notnull array<ref OVT_ResourceAmount> need, notnull array<ref OVT_ResourceAmount> have)
	{
		need.Clear();
		have.Clear();

		OVT_ConstructionSiteComponent site = GetSite(owner);
		if (!site)
			return false;

		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (!resistance)
			return false;

		OVT_Buildable buildable = resistance.GetBuildableAt(site.GetBuildableIndex());
		if (!buildable || !buildable.m_aResourceRequirements)
			return false;

		OVT_ResourceRequirements.ScaleForDifficulty(buildable.m_aResourceRequirements, need);
		OVT_ResourceRequirements.NearbyAvailability(owner.GetOrigin(), need, have);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The requirements readout with translated resource names.
	//! \param[in] need The scaled requirements.
	//! \param[in] have What is available nearby.
	//! \return A newline-separated readout, or "" when there is nothing to list.
	static string FormatReadout(notnull array<ref OVT_ResourceAmount> need, notnull array<ref OVT_ResourceAmount> have)
	{
		OVT_ResourceManagerComponent resources = OVT_Global.GetResources();
		if (!resources)
			return "";

		array<string> titles = new array<string>();
		OVT_ResourceUtils.ResolveResourceTitles(need, titles);

		return OVT_ResourceRules.FormatReadout(need, have, resources.GetDefs(), titles, WidgetManager.Translate("#OVT-Resource_Short"));
	}
}
