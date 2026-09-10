//------------------------------------------------------------------------------------------------
//! Who may edit an option and where its value lives.
enum OVT_EOptionScope
{
	LOCAL,
	FACTION,
	WORLD
}

//------------------------------------------------------------------------------------------------
//! The widget family an option renders as.
enum OVT_EOptionType
{
	TOGGLE,
	SLIDER,
	CHOICE
}

//------------------------------------------------------------------------------------------------
//! One option's declaration: id, scope, type, default and constraints. Holds no live value.
//!
//! `new` applies no `[Attribute()]` default, so every field is a plain zero or empty string until a
//! factory or a caller sets it. Use `MakeToggle`, `MakeSlider` or `MakeChoice` to build one in a
//! single call.
//------------------------------------------------------------------------------------------------
class OVT_OptionDefinition : Managed
{
	string m_sId;
	OVT_EOptionScope m_eScope;
	OVT_EOptionType m_eType;
	string m_sDefault;

	float m_fMin;
	float m_fMax;
	float m_fStep;

	float m_fShownMultiplier;
	string m_sFormatKey;

	ref array<string> m_aChoiceKeys;
	ref array<string> m_aChoiceLabelKeys;

	string m_sLabelKey;
	string m_sDescriptionKey;

	//------------------------------------------------------------------------------------------------
	//! Builds a `TOGGLE` definition.
	//! \param[in] id The namespaced option id.
	//! \param[in] scope Who may edit the option.
	//! \param[in] defaultOn The default state.
	//! \param[in] labelKey The row's label localization key.
	//! \param[in] descKey The row's description localization key.
	//! \return The definition.
	static OVT_OptionDefinition MakeToggle(string id, OVT_EOptionScope scope, bool defaultOn, string labelKey, string descKey)
	{
		OVT_OptionDefinition def = new OVT_OptionDefinition();
		def.m_sId = id;
		def.m_eScope = scope;
		def.m_eType = OVT_EOptionType.TOGGLE;

		if (defaultOn)
			def.m_sDefault = "1";
		else
			def.m_sDefault = "0";

		def.m_fMin = 0;
		def.m_fMax = 0;
		def.m_fStep = 0;
		def.m_fShownMultiplier = 1;
		def.m_sFormatKey = "";
		def.m_aChoiceKeys = new array<string>();
		def.m_aChoiceLabelKeys = new array<string>();
		def.m_sLabelKey = labelKey;
		def.m_sDescriptionKey = descKey;

		return def;
	}

	//------------------------------------------------------------------------------------------------
	//! Builds a `SLIDER` definition.
	//! \param[in] id The namespaced option id.
	//! \param[in] scope Who may edit the option.
	//! \param[in] defaultValue The default, in the option's own units.
	//! \param[in] min The lowest allowed value.
	//! \param[in] max The highest allowed value.
	//! \param[in] step The snap step, measured from `min`.
	//! \param[in] labelKey The row's label localization key.
	//! \param[in] descKey The row's description localization key.
	//! \return The definition.
	static OVT_OptionDefinition MakeSlider(string id, OVT_EOptionScope scope, float defaultValue, float min, float max, float step, string labelKey, string descKey)
	{
		OVT_OptionDefinition def = new OVT_OptionDefinition();
		def.m_sId = id;
		def.m_eScope = scope;
		def.m_eType = OVT_EOptionType.SLIDER;
		def.m_sDefault = defaultValue.ToString();
		def.m_fMin = min;
		def.m_fMax = max;
		def.m_fStep = step;
		def.m_fShownMultiplier = 1;
		def.m_sFormatKey = "";
		def.m_aChoiceKeys = new array<string>();
		def.m_aChoiceLabelKeys = new array<string>();
		def.m_sLabelKey = labelKey;
		def.m_sDescriptionKey = descKey;

		return def;
	}

	//------------------------------------------------------------------------------------------------
	//! Builds a `CHOICE` definition.
	//! \param[in] id The namespaced option id.
	//! \param[in] scope Who may edit the option.
	//! \param[in] defaultKey The default choice key. Must appear in `keys`.
	//! \param[in] keys The stored choice keys, in display order.
	//! \param[in] labelKeys One localization key for each entry in `keys`.
	//! \param[in] labelKey The row's label localization key.
	//! \param[in] descKey The row's description localization key.
	//! \return The definition.
	static OVT_OptionDefinition MakeChoice(string id, OVT_EOptionScope scope, string defaultKey, array<string> keys, array<string> labelKeys, string labelKey, string descKey)
	{
		OVT_OptionDefinition def = new OVT_OptionDefinition();
		def.m_sId = id;
		def.m_eScope = scope;
		def.m_eType = OVT_EOptionType.CHOICE;
		def.m_sDefault = defaultKey;
		def.m_fMin = 0;
		def.m_fMax = 0;
		def.m_fStep = 0;
		def.m_fShownMultiplier = 1;
		def.m_sFormatKey = "";

		if (keys)
			def.m_aChoiceKeys = keys;
		else
			def.m_aChoiceKeys = new array<string>();

		if (labelKeys)
			def.m_aChoiceLabelKeys = labelKeys;
		else
			def.m_aChoiceLabelKeys = new array<string>();

		def.m_sLabelKey = labelKey;
		def.m_sDescriptionKey = descKey;

		return def;
	}
}
