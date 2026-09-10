//------------------------------------------------------------------------------------------------
//! One live row on the Options screen: the option it edits and the control that draws it.
//!
//! Exactly one of the three component handles is set. The type field says which, so a re-populate
//! never has to guess.
//------------------------------------------------------------------------------------------------
class OVT_OptionRowBinding
{
	string m_sId;
	OVT_EOptionType m_eType;
	Widget m_wRow;
	SCR_CheckboxComponent m_Checkbox;
	SCR_SliderComponent m_Slider;
	SCR_SpinBoxComponent m_SpinBox;
}

//------------------------------------------------------------------------------------------------
//! The Options screen, generated from the option registry.
//!
//! The screen holds no list of options. It walks the three scopes and builds a row for each id from
//! the registry, so a new option appears here with no edit to this file.
//!
//! The widget names Sections, CloseButton, Label, ControlSlot and Description are a contract with
//! the layout files. A rename with no matching change here draws a placeholder that no compiler sees.
//------------------------------------------------------------------------------------------------
class OVT_OptionsContext : OVT_UIContext
{
	[Attribute(defvalue: "{6B8B000000000012}UI/Layouts/Menu/OptionsMenu/OptionsSectionTitle.layout", uiwidget: UIWidgets.ResourceNamePicker, desc: "Section title layout", params: "layout")]
	ResourceName m_SectionTitleLayout;

	[Attribute(defvalue: "{6B8B000000000011}UI/Layouts/Menu/OptionsMenu/OptionsRow.layout", uiwidget: UIWidgets.ResourceNamePicker, desc: "Option row layout", params: "layout")]
	ResourceName m_RowLayout;

	[Attribute(defvalue: "{6B8B000000000013}UI/Layouts/Menu/OptionsMenu/OptionsSlider.layout", uiwidget: UIWidgets.ResourceNamePicker, desc: "SLIDER control layout", params: "layout")]
	ResourceName m_SliderLayout;

	[Attribute(defvalue: "{5D5055E10FD00549}UI/layouts/WidgetLibrary/ToolBoxes/WLib_Checkbox.layout", uiwidget: UIWidgets.ResourceNamePicker, desc: "TOGGLE control layout", params: "layout")]
	ResourceName m_CheckboxLayout;

	[Attribute(defvalue: "{C9DF0E6590F6C388}UI/layouts/WidgetLibrary/SpinBox/WLib_SpinBox.layout", uiwidget: UIWidgets.ResourceNamePicker, desc: "CHOICE control layout", params: "layout")]
	ResourceName m_SpinBoxLayout;

	protected const string WIDGET_SECTIONS = "Sections";
	protected const string WIDGET_CLOSE = "CloseButton";
	protected const string WIDGET_ROW_LABEL = "Label";
	protected const string WIDGET_ROW_CONTROL = "ControlSlot";
	protected const string WIDGET_ROW_DESCRIPTION = "Description";

	//! Format a SLIDER falls back to when its definition names no format key. Without it the row
	//! would keep the widget library's percent format.
	protected const string SLIDER_PLAIN_FORMAT = "%1";

	protected ref array<ref OVT_OptionRowBinding> m_aBindings = new array<ref OVT_OptionRowBinding>();

	//! The manager this screen subscribed to. Cached so the close removes the same instance.
	protected OVT_OptionsManagerComponent m_Options;

	protected Widget m_wSections;
	protected Widget m_wFirstControl;
	protected SCR_InputButtonComponent m_CloseAction;

	//! True while the form writes values into its own widgets. Every change handler returns early
	//! while it is set (D14).
	protected bool m_bPopulating;

	//------------------------------------------------------------------------------------------------
	override void OnShow()
	{
		m_bPopulating = false;
		m_wFirstControl = null;

		// Close is wired first so a broken layout, or a machine with no manager, still leaves a
		// screen a gamepad can get out of.
		Widget closeButton = m_wRoot.FindAnyWidget(WIDGET_CLOSE);
		if (closeButton)
		{
			m_CloseAction = SCR_InputButtonComponent.Cast(closeButton.FindHandler(SCR_InputButtonComponent));
			if (m_CloseAction)
				m_CloseAction.m_OnActivated.Insert(CloseLayout);
		}

		m_wSections = m_wRoot.FindAnyWidget(WIDGET_SECTIONS);
		if (!m_wSections)
		{
			Print("[Overthrow] OVT_OptionsContext: widget '" + WIDGET_SECTIONS + "' not found in OptionsMenu.layout, the screen can draw no options", LogLevel.ERROR);
			FocusFirstControl(closeButton);
			return;
		}

		ClearSections();

		m_Options = OVT_Global.GetOptions();
		if (!m_Options)
		{
			Print("[Overthrow] OVT_OptionsContext: no options manager on this machine, the screen opens with no rows", LogLevel.ERROR);
			FocusFirstControl(closeButton);
			return;
		}

		m_Options.m_OnOptionChanged.Insert(OnOptionChanged);

		m_bPopulating = true;

		int localPlayerId = SCR_PlayerController.GetLocalPlayerId();

		BuildSection(OVT_EOptionScope.LOCAL, "#OVT-Options_Section_Local", localPlayerId);
		BuildSection(OVT_EOptionScope.FACTION, "#OVT-Options_Section_Faction", localPlayerId);
		BuildSection(OVT_EOptionScope.WORLD, "#OVT-Options_Section_World", localPlayerId);

		m_bPopulating = false;

		FocusFirstControl(closeButton);
	}

	//------------------------------------------------------------------------------------------------
	override void OnClose()
	{
		if (m_Options)
		{
			if (m_Options.m_OnOptionChanged)
				m_Options.m_OnOptionChanged.Remove(OnOptionChanged);

			GetGame().GetCallqueue().Remove(RefreshRow);

			// The only flush point. One whole-record write beats one for each toggle, which the
			// engine's save throttle would drop (R6).
			m_Options.FlushLocalOptions();
		}

		if (m_CloseAction)
			m_CloseAction.m_OnActivated.Remove(CloseLayout);

		foreach (OVT_OptionRowBinding binding : m_aBindings)
		{
			if (binding.m_Checkbox)
				binding.m_Checkbox.m_OnChanged.Remove(OnToggleChanged);

			if (binding.m_Slider)
				binding.m_Slider.GetOnChangedFinal().Remove(OnSliderChanged);

			if (binding.m_SpinBox)
				binding.m_SpinBox.m_OnChanged.Remove(OnChoiceChanged);
		}

		m_aBindings.Clear();

		ClearSections();

		m_CloseAction = null;
		m_wSections = null;
		m_wFirstControl = null;
		m_Options = null;
		m_bPopulating = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Builds the title and the rows of one scope, or nothing at all.
	//!
	//! The permission predicate takes the scope and not the id, so one call answers for the whole
	//! section. A section this player cannot edit, and a section with no options, both draw nothing
	//! (D9).
	//! \param[in] scope The scope to draw.
	//! \param[in] titleKey Localization key of the section title.
	//! \param[in] localPlayerId The local player's runtime id.
	protected void BuildSection(OVT_EOptionScope scope, string titleKey, int localPlayerId)
	{
		if (!OVT_OptionsRequestComponent.PlayerMayEditScope(localPlayerId, scope))
			return;

		array<string> ids = {};
		m_Options.GetIdsForScope(scope, ids);

		if (ids.IsEmpty())
			return;

		CreateSectionTitle(titleKey);

		foreach (string id : ids)
		{
			CreateRow(id);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] titleKey Localization key of the section title.
	protected void CreateSectionTitle(string titleKey)
	{
		Widget title = GetGame().GetWorkspace().CreateWidgets(m_SectionTitleLayout, m_wSections);
		if (!title)
			return;

		TextWidget text = TextWidget.Cast(title);
		if (text)
			text.SetText(titleKey);
	}

	//------------------------------------------------------------------------------------------------
	//! Builds one row and the control that matches the option's type.
	//! \param[in] id The option id.
	protected void CreateRow(string id)
	{
		OVT_OptionDefinition def = m_Options.GetDefinition(id);
		if (!def)
			return;

		Widget row = GetGame().GetWorkspace().CreateWidgets(m_RowLayout, m_wSections);
		if (!row)
			return;

		TextWidget label = TextWidget.Cast(row.FindAnyWidget(WIDGET_ROW_LABEL));
		if (label)
			label.SetText(def.m_sLabelKey);

		TextWidget description = TextWidget.Cast(row.FindAnyWidget(WIDGET_ROW_DESCRIPTION));
		if (description)
		{
			if (def.m_sDescriptionKey == "")
				description.SetVisible(false);
			else
				description.SetText(def.m_sDescriptionKey);
		}

		Widget slot = row.FindAnyWidget(WIDGET_ROW_CONTROL);
		if (!slot)
		{
			Print("[Overthrow] OVT_OptionsContext: widget '" + WIDGET_ROW_CONTROL + "' not found in OptionsRow.layout, option '" + id + "' has no control", LogLevel.ERROR);
			return;
		}

		OVT_OptionRowBinding binding = new OVT_OptionRowBinding();
		binding.m_sId = id;
		binding.m_eType = def.m_eType;
		binding.m_wRow = row;

		Widget control;

		if (def.m_eType == OVT_EOptionType.TOGGLE)
			control = BuildToggle(binding, def, slot);
		else if (def.m_eType == OVT_EOptionType.SLIDER)
			control = BuildSlider(binding, def, slot);
		else if (def.m_eType == OVT_EOptionType.CHOICE)
			control = BuildChoice(binding, def, slot);

		if (!control)
		{
			row.RemoveFromHierarchy();
			return;
		}

		m_aBindings.Insert(binding);

		if (!m_wFirstControl)
			m_wFirstControl = control;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] binding The row's binding, filled in on success.
	//! \param[in] def The option definition.
	//! \param[in] slot The row's control container.
	//! \return The control widget, or null when the control did not build.
	protected Widget BuildToggle(OVT_OptionRowBinding binding, OVT_OptionDefinition def, Widget slot)
	{
		Widget control = GetGame().GetWorkspace().CreateWidgets(m_CheckboxLayout, slot);
		if (!control)
			return null;

		SCR_CheckboxComponent checkbox = SCR_CheckboxComponent.Cast(control.FindHandler(SCR_CheckboxComponent));
		if (!checkbox)
		{
			control.RemoveFromHierarchy();
			return null;
		}

		// The row draws its own label, so the widget library's placeholder must go.
		checkbox.UseLabel(false);

		// playSound false is what suppresses the change event through the vanilla argument chain.
		checkbox.SetChecked(m_Options.GetBool(def.m_sId), false, false);

		checkbox.m_OnChanged.Insert(OnToggleChanged);

		binding.m_Checkbox = checkbox;

		return control;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] binding The row's binding, filled in on success.
	//! \param[in] def The option definition.
	//! \param[in] slot The row's control container.
	//! \return The control widget, or null when the control did not build.
	protected Widget BuildSlider(OVT_OptionRowBinding binding, OVT_OptionDefinition def, Widget slot)
	{
		Widget control = GetGame().GetWorkspace().CreateWidgets(m_SliderLayout, slot);
		if (!control)
			return null;

		SCR_SliderComponent slider = SCR_SliderComponent.Cast(control.FindHandler(SCR_SliderComponent));
		if (!slider || !control.FindAnyWidget("Slider"))
		{
			control.RemoveFromHierarchy();
			return null;
		}

		string format = def.m_sFormatKey;
		if (format == "")
			format = SLIDER_PLAIN_FORMAT;

		slider.SetSliderSettings(def.m_fMin, def.m_fMax, def.m_fStep, format);

		float multiplier = def.m_fShownMultiplier;
		if (multiplier == 0)
			multiplier = 1;

		slider.SetShownValueMultiplier(multiplier);

		// SetValue raises m_OnChanged and never m_OnChangedFinal, so the write hook below stays
		// silent here and a drag sends one request rather than one for each pixel.
		slider.SetValue(m_Options.GetFloat(def.m_sId));

		slider.GetOnChangedFinal().Insert(OnSliderChanged);

		binding.m_Slider = slider;

		return control;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] binding The row's binding, filled in on success.
	//! \param[in] def The option definition.
	//! \param[in] slot The row's control container.
	//! \return The control widget, or null when the control did not build.
	protected Widget BuildChoice(OVT_OptionRowBinding binding, OVT_OptionDefinition def, Widget slot)
	{
		Widget control = GetGame().GetWorkspace().CreateWidgets(m_SpinBoxLayout, slot);
		if (!control)
			return null;

		SCR_SpinBoxComponent spinbox = SCR_SpinBoxComponent.Cast(control.FindHandler(SCR_SpinBoxComponent));
		if (!spinbox)
		{
			control.RemoveFromHierarchy();
			return null;
		}

		if (!def.m_aChoiceKeys || !def.m_aChoiceLabelKeys || def.m_aChoiceLabelKeys.IsEmpty())
		{
			control.RemoveFromHierarchy();
			return null;
		}

		spinbox.UseLabel(false);
		spinbox.ClearAll();

		// AddItem's second argument reaches SetInitialState as invokeOnChanged, so it stays false.
		foreach (string labelKey : def.m_aChoiceLabelKeys)
		{
			spinbox.AddItem(labelKey, false);
		}

		int index = def.m_aChoiceKeys.Find(m_Options.GetChoice(def.m_sId));
		if (index < 0)
			index = 0;

		spinbox.SetCurrentItem(index, false, false, false);

		spinbox.m_OnChanged.Insert(OnChoiceChanged);

		binding.m_SpinBox = spinbox;

		return control;
	}

	//------------------------------------------------------------------------------------------------
	//! SCR_CheckboxComponent.m_OnChanged handler.
	//! \param[in] checkbox The checkbox that changed.
	//! \param[in] state The new checked state.
	protected void OnToggleChanged(SCR_CheckboxComponent checkbox, bool state)
	{
		if (m_bPopulating)
			return;

		foreach (OVT_OptionRowBinding binding : m_aBindings)
		{
			if (binding.m_Checkbox != checkbox)
				continue;

			WriteOption(binding.m_sId, OVT_OptionsRegistry.EncodeBool(state));
			return;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! SCR_SliderComponent.GetOnChangedFinal() handler. It fires when the player releases the slider.
	//! \param[in] slider The slider that changed.
	//! \param[in] value The new value, in the option's own units.
	protected void OnSliderChanged(SCR_SliderComponent slider, float value)
	{
		if (m_bPopulating)
			return;

		foreach (OVT_OptionRowBinding binding : m_aBindings)
		{
			if (binding.m_Slider != slider)
				continue;

			WriteOption(binding.m_sId, OVT_OptionsRegistry.EncodeFloat(value));
			return;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! SCR_SpinBoxComponent.m_OnChanged handler. The stored value is the key, never the index (D2).
	//! \param[in] spinbox The picker that changed.
	//! \param[in] index The selected entry.
	protected void OnChoiceChanged(SCR_SpinBoxComponent spinbox, int index)
	{
		if (m_bPopulating)
			return;

		foreach (OVT_OptionRowBinding binding : m_aBindings)
		{
			if (binding.m_SpinBox != spinbox)
				continue;

			OVT_OptionDefinition def = m_Options.GetDefinition(binding.m_sId);
			if (!def || !def.m_aChoiceKeys || !def.m_aChoiceKeys.IsIndexValid(index))
				return;

			WriteOption(binding.m_sId, def.m_aChoiceKeys[index]);
			return;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Sends one value on the path its scope demands. A LOCAL value stays on this machine, and every
	//! other scope goes to the server, which validates the write again from scratch.
	//! \param[in] id The option id.
	//! \param[in] value The candidate value, in the option's string encoding.
	protected void WriteOption(string id, string value)
	{
		if (!m_Options)
			return;

		OVT_OptionDefinition def = m_Options.GetDefinition(id);
		if (!def)
			return;

		if (def.m_eScope == OVT_EOptionScope.LOCAL)
		{
			m_Options.SetLocalOption(id, value);
			return;
		}

		OVT_OptionsRequestComponent request = OVT_ControllerComponent<OVT_OptionsRequestComponent>.Get();
		if (!request)
		{
			Print("[Overthrow] OVT_OptionsContext: no options request component, the write to '" + id + "' never left this machine", LogLevel.WARNING);
			return;
		}

		request.SetOption(id, value);
	}

	//------------------------------------------------------------------------------------------------
	//! OVT_OptionsManagerComponent.m_OnOptionChanged handler. It queues a redraw of the one row that
	//! shows the id, from this machine's mirror.
	//! ⚠ Deferred one frame. On a listen host or in single player the broadcast fires inside the
	//! widget's own click, before the vanilla toolbox moves its selection. A synchronous redraw then
	//! makes the click toggle the new value off again.
	//! \param[in] id The option that changed.
	//! \param[in] value The stored value. The row reads the mirror instead, so it always shows what
	//!            the registry holds.
	protected void OnOptionChanged(string id, string value)
	{
		if (!m_bIsActive)
			return;

		GetGame().GetCallqueue().CallLater(RefreshRow, 0, false, id);
	}

	//------------------------------------------------------------------------------------------------
	//! Redraws the one row that shows the id. Runs one frame after the change arrived.
	//! \param[in] id The option that changed.
	protected void RefreshRow(string id)
	{
		if (!m_bIsActive)
			return;

		if (!m_wRoot)
			return;

		if (!m_Options)
			return;

		foreach (OVT_OptionRowBinding binding : m_aBindings)
		{
			if (binding.m_sId != id)
				continue;

			m_bPopulating = true;
			PopulateControl(binding);
			m_bPopulating = false;
			return;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the registry's current value into one control. The caller holds m_bPopulating.
	//! \param[in] binding The row to redraw.
	protected void PopulateControl(OVT_OptionRowBinding binding)
	{
		if (binding.m_eType == OVT_EOptionType.TOGGLE && binding.m_Checkbox)
		{
			binding.m_Checkbox.SetChecked(m_Options.GetBool(binding.m_sId), false, false);
			return;
		}

		if (binding.m_eType == OVT_EOptionType.SLIDER && binding.m_Slider)
		{
			binding.m_Slider.SetValue(m_Options.GetFloat(binding.m_sId));
			return;
		}

		if (binding.m_eType != OVT_EOptionType.CHOICE || !binding.m_SpinBox)
			return;

		OVT_OptionDefinition def = m_Options.GetDefinition(binding.m_sId);
		if (!def || !def.m_aChoiceKeys)
			return;

		int index = def.m_aChoiceKeys.Find(m_Options.GetChoice(binding.m_sId));
		if (index < 0)
			index = 0;

		binding.m_SpinBox.SetCurrentItem(index, false, false, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the gamepad cursor on the first control, or on Close when the screen drew no rows. A
	//! screen with nothing focused strands a player who has no mouse.
	//! \param[in] fallback The widget to focus when there is no row. May be null.
	protected void FocusFirstControl(Widget fallback)
	{
		if (m_wFirstControl)
		{
			GetGame().GetWorkspace().SetFocusedWidget(m_wFirstControl);
			return;
		}

		if (fallback)
			GetGame().GetWorkspace().SetFocusedWidget(fallback);
	}

	//------------------------------------------------------------------------------------------------
	//! Removes every section title and row from the list.
	protected void ClearSections()
	{
		if (!m_wSections)
			return;

		Widget child = m_wSections.GetChildren();
		while (child)
		{
			m_wSections.RemoveChild(child);
			child = m_wSections.GetChildren();
		}
	}
}
