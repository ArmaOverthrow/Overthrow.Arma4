//------------------------------------------------------------------------------------------------
//! The resource catalogue, loaded from Configs/Resistance/resources.conf.
//!
//! Adding, removing or re-pricing a resource is a .conf edit and nothing else. Shaped after
//! OVT_BuildablesConfig so the same LoadContainer/CreateInstanceFromContainer idiom loads it.
//!
//! Entry ORDER is the wire index (OVT_ResourceDefs) - reordering the .conf reorders the wire, which
//! is safe only because the file ships with the mod and is therefore identical on every machine.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot : true)]
class OVT_ResourcesConfig
{
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_Resource> m_aResources;
}

//------------------------------------------------------------------------------------------------
//! One resource definition. Volume is authored in friendly m3 and converted once to integer litres
//! (D3); weight is display only and never gates a fit.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(), SCR_BaseContainerLocalizedTitleField("m_sTitle")]
class OVT_Resource
{
	[Attribute(desc: "Stable string id - the save, requirement lists and every helper key on this")]
	string m_sId;

	[Attribute(desc: "Display title")]
	string m_sTitle;

	[Attribute(desc: "Display description")]
	string m_sDescription;

	[Attribute("", UIWidgets.ResourceNamePicker, "", "edds", desc: "Row and HUD icon")]
	ResourceName m_tIcon;

	[Attribute(defvalue: "0.1", desc: "Cubic metres one unit occupies")]
	float m_fCubicMetresPerUnit;

	[Attribute(defvalue: "25", desc: "Display weight of one unit, kg. Never gates capacity")]
	float m_fKgPerUnit;

	[Attribute(defvalue: "100", desc: "Config base price. NOT the live price - the manager owns that")]
	int m_iBasePrice;

	[Attribute(defvalue: "1", desc: "1 = may be bought at a port")]
	int m_iImportable;

	[Attribute(defvalue: "0", desc: "1 = needs the illegal-imports gate")]
	int m_iIllegal;

	[Attribute("", desc: "Map icon quad name in overthrow_mapicons.imageset. Empty falls back to the shipped crate quad")]
	string m_sMapIconName;
}
