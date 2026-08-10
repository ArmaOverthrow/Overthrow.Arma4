//------------------------------------------------------------------------------------------------
//! The tutorial trigger catalog: every event a tutorial entry can be bound to.
//!
//! ELEVEN of these are raised on the SERVER from an existing manager ScriptInvoker - nine per-player,
//! plus TOWN_CONTROL_CHANGE and BASE_CONTROL_CHANGE, which have no acting player by construction and
//! are fanned out by proximity instead. The remaining three (MAP_OPENED, MENU_OPENED,
//! PLAYER_SPAWNED) are raised CLIENT-LOCALLY with no round trip. The full table - source invoker,
//! what m_iValue carries and what m_sFilter
//! carries - is the contract in the implementation plan's section 5 and is the authority for what a
//! trigger author may rely on.
//------------------------------------------------------------------------------------------------
enum OVT_TutorialEvent
{
	//! A player bought something. m_iValue = the actual cost paid.
	PLAYER_BUY = 0,
	//! A player was credited for a sale. m_iValue = the total credited.
	PLAYER_SELL = 1,
	//! A player completed a shop transaction. m_iValue = amount, m_sFilter = shop type.
	PLAYER_TRANSACTION = 2,
	//! A player placed a placeable. m_sFilter = the placeable's name.
	PLAYER_PLACE = 3,
	//! A player finished a buildable. m_sFilter = the buildable's name.
	PLAYER_BUILD = 4,
	//! A recruit joined a player's roster.
	PLAYER_RECRUIT_ADDED = 5,
	//! A player's skill changed. m_sFilter = the skill key.
	PLAYER_SKILL = 6,
	//! A player's wanted level rose. m_iValue = the new level.
	PLAYER_WANTED = 7,
	//! A town changed hands. GLOBAL - no acting player; delivered by proximity.
	TOWN_CONTROL_CHANGE = 8,
	//! A base changed hands. GLOBAL - no acting player; delivered by proximity.
	BASE_CONTROL_CHANGE = 9,
	//! The player opened the map. Client-local.
	MAP_OPENED = 10,
	//! The player opened an Overthrow menu. Client-local. m_sFilter = the context class name.
	MENU_OPENED = 11,
	//! The local player spawned. Client-local.
	PLAYER_SPAWNED = 12,
	//! A player crossed INTO the close range of a base the occupying faction still holds - the same
	//! radius the map draws as a restricted area. Fires on the crossing, not once per second inside,
	//! and never for a recruit. Carries no payload: an entry bound to it is about bases in general,
	//! because a first-time player has no way to tell one base from another yet.
	PLAYER_ENTER_BASE = 13
};

//------------------------------------------------------------------------------------------------
//! One occurrence of a trigger event, as pure data.
//!
//! Built by whoever observed the event - the server manager from an invoker callback, or the client
//! component from a local hook - and handed to the matcher. Deliberately free of entities, RplIds
//! and engine handles so that both the matcher and this class are Logic-tier testable: the acting
//! player is already resolved to a runtime player id before matching, and everything an entity
//! contributed has already been flattened into m_iValue or m_sFilter.
//------------------------------------------------------------------------------------------------
class OVT_TutorialEventContext : Managed
{
	//! Which event occurred.
	OVT_TutorialEvent m_eEvent;

	//! Runtime player id of the acting player, or -1 for a global event with no actor.
	int m_iPlayerId;

	//! The event's numeric payload (cost, total, level, ...). 0 when the event carries none.
	int m_iValue;

	//! The event's string payload (placeable name, shop type, skill key, ...). "" when it carries none.
	string m_sFilter;
}

//------------------------------------------------------------------------------------------------
//! One trigger binding on a tutorial entry: an event, an optional numeric threshold and an optional
//! exact-match string filter.
//!
//! ONE class with a virtual match rather than a class hierarchy (plan decision D3). Tutorial
//! triggers differ only by which event and two optional filters, so the ten-subclass shape that
//! OVT_JobCondition needs would be pure ceremony here. The base class ships the only implementation
//! anyone needs; the virtual exists purely so a third-party mod can subclass for exotic matching
//! without a framework change.
//!
//! Derives from ScriptAndConfig for the same reason OVT_JobCondition does: it lets an entry hold a
//! `ref array<ref OVT_TutorialTrigger>` that the Workbench object picker can populate with either
//! this class or a subclass of it.
//------------------------------------------------------------------------------------------------
class OVT_TutorialTrigger : ScriptAndConfig
{
	[Attribute("0", uiwidget: UIWidgets.ComboBox, desc: "Event this trigger listens for", enums: ParamEnumArray.FromEnum(OVT_TutorialEvent))]
	//! Event this trigger listens for.
	OVT_TutorialEvent m_eEvent;

	[Attribute("0", desc: "Minimum value the event must carry. 0 means no threshold.")]
	//! Minimum m_iValue the event must carry. 0 disables the threshold entirely; a set threshold
	//! ACCEPTS an equal value and rejects anything lower.
	int m_iMinValue;

	[Attribute("", desc: "Exact, case-sensitive string the event's filter must equal. Empty means no filter.")]
	//! Exact, case-sensitive value the event's m_sFilter must equal. "" disables the filter.
	string m_sFilter;

	//------------------------------------------------------------------------------------------------
	//! Whether this trigger fires for the given event occurrence.
	//!
	//! Virtual so a subclass can add exotic matching; the base implementation is event, then
	//! threshold, then filter, and it is the only one the shipped entries use.
	//! \param[in] ctx The event occurrence. A null context never matches.
	//! \return True when this trigger fires.
	bool Matches(OVT_TutorialEventContext ctx)
	{
		if (!ctx)
			return false;

		if (ctx.m_eEvent != m_eEvent)
			return false;

		// 0 is the "no threshold" sentinel, not a threshold of zero. A real threshold is inclusive.
		if (m_iMinValue != 0 && ctx.m_iValue < m_iMinValue)
			return false;

		// "" is the "no filter" sentinel. A real filter is exact and case-sensitive: the values it
		// compares against are class names and config keys, where case carries meaning.
		if (m_sFilter != "" && ctx.m_sFilter != m_sFilter)
			return false;

		return true;
	}
}
