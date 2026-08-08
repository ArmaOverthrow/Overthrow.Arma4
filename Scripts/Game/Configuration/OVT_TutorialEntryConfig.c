//------------------------------------------------------------------------------------------------
//! How a tutorial entry is presented to the player.
//------------------------------------------------------------------------------------------------
enum OVT_TutorialPresentation
{
	//! HUD overlay. Never takes movement, aim or fire; auto-dismisses; escalates to MODAL on one key.
	NONMODAL = 0,
	//! A focusable OVT_UIContext with real buttons. Multi-page entries are always shown this way.
	MODAL = 1
};

//------------------------------------------------------------------------------------------------
//! One page of a tutorial entry's body text.
//!
//! An entry with more than one page is a sequence and is always presented modally (plan decision
//! D10) - paging a non-focusable HUD overlay would cost two more gameplay keybindings for no
//! current benefit.
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class OVT_TutorialPage
{
	[Attribute(desc: "#OVT- localization key for this page's body text")]
	//! Localization key for this page's body text.
	string m_sBody;

	[Attribute(ResourceName.Empty, UIWidgets.ResourcePickerThumbnail, "Optional image shown with this page", "edds imageset")]
	//! Optional image shown beside the body. ResourceName.Empty for none.
	ResourceName m_sImage;
}

//------------------------------------------------------------------------------------------------
//! A single tutorial entry: what it says, how it is shown, and what makes it fire.
//!
//! One .conf per entry under Configs/Tutorials/, registered on the game-mode prefab exactly the way
//! m_aJobConfigs is (plan decision D2) - which is what lets a server owner or a modder add, retune
//! or retire an entry with no EnforceScript at all, and lets a third-party mod append with the
//! proven `m_aTutorialEntries + { ... }` delta form.
//!
//! Modelled directly on OVT_JobConfig: configRoot, nested `ref array<ref ...>` object arrays.
//!
//! m_sId is the wire identity (plan decision D4) and the permanent key of the per-machine seen
//! store. It is IMMUTABLE once shipped and is never reused: changing it re-shows the entry for
//! everyone, and recycling a retired id makes it never show for veterans. Retire with m_bEnabled 0.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot : true)]
class OVT_TutorialEntryConfig
{
	[Attribute(desc: "Stable lowercase-kebab id, immutable once shipped and never reused")]
	//! Stable unique id, e.g. "economy-first-buy". Immutable once shipped.
	string m_sId;

	[Attribute(desc: "#OVT- localization key for the entry title")]
	//! Localization key for the title.
	string m_sTitle;

	[Attribute("0", uiwidget: UIWidgets.ComboBox, desc: "How this entry is presented", enums: ParamEnumArray.FromEnum(OVT_TutorialPresentation))]
	//! Presentation mode. An entry declaring NONMODAL with more than one page is coerced to MODAL.
	OVT_TutorialPresentation m_ePresentation;

	[Attribute("0", desc: "Higher priority entries are shown first when several are queued")]
	//! Queue priority. Higher shows first; equal priorities are shown in the order they arrived.
	int m_iPriority;

	[Attribute(desc: "Optional m_sTitle localization key of the field-manual entry 'Learn more' opens")]
	//! Deep-link target: the m_sTitle key of a field-manual entry. "" hides the Learn more button.
	string m_sFieldManualTitleKey;

	[Attribute("", UIWidgets.Object, desc: "Body pages. At least one; more than one forces MODAL.")]
	//! Body pages, in order. At least one.
	ref array<ref OVT_TutorialPage> m_aPages;

	[Attribute("", UIWidgets.Object, desc: "Trigger bindings. The entry fires when ANY of them matches.")]
	//! Trigger bindings. The entry fires when ANY of them matches an event occurrence.
	ref array<ref OVT_TutorialTrigger> m_aTriggers;

	[Attribute("1", desc: "Uncheck to retire an entry. A retired entry never fires and its id is never reused.")]
	//! Whether this entry can fire at all. The intended retirement path.
	bool m_bEnabled;
}
