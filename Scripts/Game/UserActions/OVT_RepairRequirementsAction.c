//------------------------------------------------------------------------------------------------
//! "Repair Requirements" - shows what a RUINED structure needs before it can be put back, in an
//! information dialog. The ruin's counterpart to OVT_SiteRequirementsAction on a construction site.
//!
//! IT READS THE REPLICATED PILE CONTENTS, so it is correct on every machine with no round trip; the
//! server re-derives all of it before it consumes anything.
//!
//! Sort Priority 1, so it sits above the repair action: reading what a thing costs comes before
//! paying for it.
//------------------------------------------------------------------------------------------------
class OVT_RepairRequirementsAction : ScriptedUserAction
{
	//! The Overthrow dialog presets; SITE_REQUIREMENTS is an OK-only information dialog.
	protected const ResourceName DIALOG_PRESETS = "{272B6C4030554E27}Configs/UI/Dialogs/DialogPresets_Campaign.conf";

	//! The open dialog, held so its handler can unsubscribe itself. The action outlives it.
	protected SCR_ConfigurableDialogUi m_Dialog;

	//------------------------------------------------------------------------------------------------
	//! \param[in] pOwnerEntity The ruined structure.
	//! \param[in] pUserEntity The acting character.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (m_Dialog)
			return;

		array<ref OVT_ResourceAmount> need = new array<ref OVT_ResourceAmount>();
		array<ref OVT_ResourceAmount> have = new array<ref OVT_ResourceAmount>();
		if (!OVT_RepairRequirementsReader.Read(pOwnerEntity, need, have))
			return;

		SCR_ConfigurableDialogUi dialog = SCR_ConfigurableDialogUi.CreateFromPreset(DIALOG_PRESETS, "SITE_REQUIREMENTS");
		if (!dialog)
			return;

		dialog.SetTitle(ResolveTitle());
		dialog.SetMessage(OVT_RepairRequirementsReader.FormatReadout(need, have));

		m_Dialog = dialog;
		m_Dialog.m_OnConfirm.Insert(ForgetDialog);
		m_Dialog.m_OnCancel.Insert(ForgetDialog);
	}

	//------------------------------------------------------------------------------------------------
	//! The inverse of every other action on these prefabs: shown ONLY on a ruin, and only when this
	//! buildable actually costs resources - a structure that repairs for money alone has nothing to
	//! list, and an empty dialog is worse than no action.
	//! \param[in] user The character looking at the action.
	//! \return True while the structure is ruined and has a resource requirement.
	override bool CanBeShownScript(IEntity user)
	{
		IEntity root = OVT_RepairRequirementsReader.ResolveRoot(GetOwner());
		if (!root)
			return false;

		if (!OVT_StructureDamage.IsRuined(root))
			return false;

		array<ref OVT_ResourceAmount> need = new array<ref OVT_ResourceAmount>();
		array<ref OVT_ResourceAmount> have = new array<ref OVT_ResourceAmount>();
		if (!OVT_RepairRequirementsReader.Read(root, need, have))
			return false;

		return !need.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! \param[out] outName The label to draw.
	//! \return Always true.
	override bool GetActionNameScript(out string outName)
	{
		outName = "#OVT-Resource_RepairRequirements";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A dialog on the clicking machine and nothing else.
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The dialog title: the buildable's title, or a generic one.
	protected string ResolveTitle()
	{
		OVT_Buildable buildable = OVT_RepairRequirementsReader.ResolveBuildable(GetOwner());
		if (!buildable || buildable.m_sTitle == "")
			return "#OVT-RepairStructure";

		return buildable.m_sTitle;
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
//! What the repair action and its readout both need to know: the buildable behind a ruin, the share
//! of its construction requirement a repair costs, and what the crate piles around it hold.
//!
//! CLIENT-SAFE THROUGHOUT, and deliberately the same shape as OVT_SiteRequirementsReader - the
//! requirement figures come from the same OVT_ResourceRequirements.ScaleForDifficulty() call the
//! server consumes by, narrowed by the same OVT_ResourceRules.RepairRequirement(), so the label a
//! player reads and the amount the server takes are the same numbers.
//------------------------------------------------------------------------------------------------
class OVT_RepairRequirementsReader
{
	//------------------------------------------------------------------------------------------------
	//! The entity that carries the buildable identity. The ramp mounts its actions on a bare root
	//! whose phase lives on a CHILD, so every gate goes up to the root the same way the repair action
	//! does.
	//! \param[in] entity The action's owner.
	//! \return Its root parent, or itself.
	static IEntity ResolveRoot(IEntity entity)
	{
		if (!entity)
			return null;

		IEntity root = entity.GetRootParent();
		if (!root)
			return entity;

		return root;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] owner The ruined structure, or any child of it.
	//! \return The buildables-config entry this structure was built from, or null.
	static OVT_Buildable ResolveBuildable(IEntity owner)
	{
		IEntity root = ResolveRoot(owner);
		if (!root)
			return null;

		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (!resistance)
			return null;

		return resistance.FindBuildableForEntity(root);
	}

	//------------------------------------------------------------------------------------------------
	//! Fills a ruin's repair requirements and what is available around it.
	//!
	//! An empty `need` with a true return is the ORDINARY case, not a failure: most buildables cost
	//! money alone, and those repair exactly as core/damage shipped them.
	//! \param[in] owner The ruined structure.
	//! \param[out] need Receives the repair requirements. Cleared first.
	//! \param[out] have Receives what the nearby piles hold. Cleared first.
	//! \return True when the buildable and the difficulty both resolved.
	static bool Read(IEntity owner, notnull array<ref OVT_ResourceAmount> need, notnull array<ref OVT_ResourceAmount> have)
	{
		need.Clear();
		have.Clear();

		IEntity root = ResolveRoot(owner);
		if (!root)
			return false;

		OVT_Buildable buildable = ResolveBuildable(root);
		if (!buildable)
			return false;

		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if (!difficulty)
			return false;

		if (!buildable.m_aResourceRequirements || buildable.m_aResourceRequirements.IsEmpty())
			return true;

		array<ref OVT_ResourceAmount> built = new array<ref OVT_ResourceAmount>();
		OVT_ResourceRequirements.ScaleForDifficulty(buildable.m_aResourceRequirements, built);

		foreach (OVT_ResourceAmount amount : built)
		{
			if (!amount)
				continue;

			int qty = OVT_ResourceRules.RepairRequirement(amount.m_iQuantity, difficulty.repairCostMultiplier);
			if (qty <= 0)
				continue;

			OVT_ResourceAmount line = new OVT_ResourceAmount();
			line.m_sId = amount.m_sId;
			line.m_iQuantity = qty;
			need.Insert(line);
		}

		OVT_ResourceRequirements.NearbyAvailability(root.GetOrigin(), need, have);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the piles around a ruin cover its repair requirement.
	//! \param[in] owner The ruined structure.
	//! \param[out] shortId The FIRST resource that is short; "" when nothing is.
	//! \return True when the repair may go ahead as far as resources are concerned.
	static bool IsSatisfied(IEntity owner, out string shortId)
	{
		shortId = "";

		array<ref OVT_ResourceAmount> need = new array<ref OVT_ResourceAmount>();
		array<ref OVT_ResourceAmount> have = new array<ref OVT_ResourceAmount>();

		// A ruin whose buildable cannot be resolved is not blocked HERE - the repair action's own
		// price gate already refuses an unclaimed structure, and refusing twice would hide the real
		// reason behind a resource message.
		if (!Read(owner, need, have))
			return true;

		if (need.IsEmpty())
			return true;

		return OVT_ResourceRules.IsSatisfied(need, have, shortId);
	}

	//------------------------------------------------------------------------------------------------
	//! The refusal message naming the resource a repair is short of.
	//! \param[in] shortId The resource id from IsSatisfied(), possibly empty.
	//! \return A translated-at-display reason string.
	static string ShortReason(string shortId)
	{
		if (shortId == "")
			return "#OVT-Resource_RepairNeedsMaterials";

		array<ref OVT_ResourceAmount> one = new array<ref OVT_ResourceAmount>();
		OVT_ResourceAmount amount = new OVT_ResourceAmount();
		amount.m_sId = shortId;
		one.Insert(amount);

		array<string> titles = new array<string>();
		OVT_ResourceUtils.ResolveResourceTitles(one, titles);

		if (titles.IsEmpty() || titles[0] == "")
			return "#OVT-Resource_RepairNeedsMaterials";

		return WidgetManager.Translate("#OVT-Resource_RepairShortOf") + " " + WidgetManager.Translate(titles[0]);
	}

	//------------------------------------------------------------------------------------------------
	//! The requirements readout with translated resource names.
	//! \param[in] need The repair requirements.
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
