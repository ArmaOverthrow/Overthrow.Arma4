//------------------------------------------------------------------------------------------------
//! TIER A case - waypoint classification and route geometry for the Game Master route drawing.
//!
//! OVT_GMWaypointFormat is a class of pure statics and it is the entirety of that drawing's
//! automatable surface: everything else the feature does is an RPC fan and a Shape call inside the
//! Game Master editor, which no suite can open and no suite can look at.
//!
//! WHAT IT PINS:
//!  - THE WHOLE COLOUR TABLE. All twelve waypoint prefabs the campaign assigns are classified here,
//!    stated from the prefab list rather than obtained by asking the subject what it does;
//!  - THE DEFEND/PATROL ORDERING TRAP. AIWaypoint_Defend_ConflictBaseTeamPatrol ENDS IN "Patrol". A
//!    contains-chain that tested PATROL before DEFEND would mis-colour every occupying-faction base
//!    defend group, compile clean and look plausible on screen. This is the case most likely to catch
//!    a real regression;
//!  - PREFIX INVARIANCE. The wire carries whatever the prefab-name helper returned, which is normally
//!    the braced "{GUID}Path/Name.et" form. Braced and bare must classify identically or the feature
//!    works in a test and not in the game;
//!  - THE HONEST UNKNOWN. An unrecognised name and an empty string classify UNKNOWN rather than
//!    guessing from a substring. An unfamiliar waypoint must render neutrally, not as a defend point;
//!  - THE LEG ARITHMETIC. Vertex 0 is the group, so an open route of 4 waypoints has 4 legs and a
//!    cyclic one has 5. An off-by-one here draws a leg to a vertex that does not exist;
//!  - THE DEGENERATE ROUTES. One waypoint is one leg whether or not the route claims to cycle, and no
//!    waypoints is no legs;
//!  - THE -1 SENTINEL. "Nothing is current" is a first-class answer with three real causes, and it
//!    must highlight no leg at all rather than leg 0.
//!
//! Every expectation below is stated by the route specification. Nothing is read back from the
//! subject to decide what to assert.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_GMWaypointFormat : SCR_AutotestCaseBase
{
	//! The braced prefix form the prefab-name helper returns in the running game.
	static const string PREFIX = "{22A875E30470BD4F}Prefabs/AI/Waypoints/";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- THE TWELVE ASSIGNED PREFABS, in the braced form the wire actually carries.
		if (!ExpectClass("AIWaypoint_Move.et", OVT_EGMWaypointType.MOVE, "the move waypoint"))
			return true;

		if (!ExpectClass("AIWaypoint_Patrol.et", OVT_EGMWaypointType.PATROL, "the patrol waypoint, assigned twice in config"))
			return true;

		if (!ExpectClass("AIWaypoint_Loiter_CO.et", OVT_EGMWaypointType.PATROL, "loiter reads as a patrol to someone looking at a line"))
			return true;

		if (!ExpectClass("AIWaypoint_Defend.et", OVT_EGMWaypointType.DEFEND, "the plain defend waypoint"))
			return true;

		if (!ExpectClass("AIWaypoint_GetIn.et", OVT_EGMWaypointType.GET_IN, "the get-in waypoint"))
			return true;

		if (!ExpectClass("AIWaypoint_GetOut.et", OVT_EGMWaypointType.GET_OUT, "the get-out waypoint"))
			return true;

		if (!ExpectClass("AIWaypoint_Scout.et", OVT_EGMWaypointType.SCOUT, "the scout waypoint"))
			return true;

		if (!ExpectClass("AIWaypoint_Wait.et", OVT_EGMWaypointType.WAIT, "the wait waypoint - half of every perimeter patrol"))
			return true;

		if (!ExpectClass("AIWaypoint_Cycle.et", OVT_EGMWaypointType.CYCLE, "the cycle container"))
			return true;

		if (!ExpectClass("AIWaypoint_SearchAndDestroy.et", OVT_EGMWaypointType.SEARCH_DESTROY, "the search-and-destroy waypoint"))
			return true;

		if (!ExpectClass("AIWaypoint_UserAction.et", OVT_EGMWaypointType.ACTION, "the smart-action waypoint"))
			return true;

		// --- THE ORDERING TRAP. Twelfth prefab, and the one that breaks a naive implementation.
		if (!ExpectClass("AIWaypoint_Defend_ConflictBaseTeamPatrol.et", OVT_EGMWaypointType.DEFEND,
			"the base defend waypoint, whose name ENDS IN 'Patrol'"))
			return true;

		// --- PREFIX INVARIANCE. The same name, braced and bare, is the same waypoint.
		int braced = OVT_GMWaypointFormat.ClassifyPrefab(PREFIX + "AIWaypoint_Patrol.et");
		int bare = OVT_GMWaypointFormat.ClassifyPrefab("AIWaypoint_Patrol");

		if (braced != bare)
		{
			SetFailure("'{GUID}Path/AIWaypoint_Patrol.et' classified as %1 and the bare stem as %2; the GUID and path must not change the answer",
				TypeName(braced), TypeName(bare));
			return true;
		}

		// --- THE HONEST UNKNOWN.
		int unrecognised = OVT_GMWaypointFormat.ClassifyPrefab("{ABCDEF0123456789}Prefabs/AI/Waypoints/AIWaypoint_SomethingNobodyWrote.et");
		if (unrecognised != OVT_EGMWaypointType.UNKNOWN)
		{
			SetFailure("an unrecognised waypoint prefab classified as %1; anything not in the table must be UNKNOWN rather than a guess",
				TypeName(unrecognised));
			return true;
		}

		int empty = OVT_GMWaypointFormat.ClassifyPrefab("");
		if (empty != OVT_EGMWaypointType.UNKNOWN)
		{
			SetFailure("an empty prefab name classified as %1 instead of UNKNOWN", TypeName(empty));
			return true;
		}

		// --- LEG ARITHMETIC. Vertex 0 is the group, so leg i terminates at waypoint i.
		if (!ExpectLegs(4, false, 4, "an open 4-waypoint route: group->WP0 .. WP2->WP3"))
			return true;

		if (!ExpectLegs(4, true, 5, "a cyclic 4-waypoint route adds the leg closing WP3 back to WP0"))
			return true;

		if (!ExpectLegs(1, true, 1, "a single waypoint has nothing to close a loop between"))
			return true;

		if (!ExpectLegs(0, false, 0, "an empty open route draws nothing"))
			return true;

		if (!ExpectLegs(0, true, 0, "an empty cyclic route draws nothing either"))
			return true;

		// --- HIGHLIGHT RESOLUTION.
		if (!OVT_GMWaypointFormat.IsHighlightLeg(2, 2))
		{
			SetFailure("leg 2 was not highlighted while waypoint 2 is the current one; the leg terminating at the current waypoint is the highlight");
			return true;
		}

		if (OVT_GMWaypointFormat.IsHighlightLeg(2, 1))
		{
			SetFailure("leg 2 was highlighted while waypoint 1 is the current one; exactly one leg may highlight");
			return true;
		}

		// The sentinel, checked on more than one leg so an implementation comparing against 0 cannot
		// slip through on the leg that happens to agree.
		if (OVT_GMWaypointFormat.IsHighlightLeg(0, -1) || OVT_GMWaypointFormat.IsHighlightLeg(3, -1))
		{
			SetFailure("a leg was highlighted with a current index of -1; -1 means nothing is current and must highlight no leg at all");
			return true;
		}

		Print("GM waypoint format: all twelve assigned prefabs classify to their intended type (including the base defend waypoint whose name ends in 'Patrol'), a GUID prefix changes nothing, unrecognised and empty names are UNKNOWN, leg counts match the group-first vertex model, and -1 highlights nothing");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one prefab stem classifies as required, in the braced form the wire carries.
	//! \param[in] fileName The prefab file name, with extension, no path.
	//! \param[in] expected The OVT_EGMWaypointType the prefab table requires.
	//! \param[in] label Human description, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectClass(string fileName, int expected, string label)
	{
		string resource = PREFIX + fileName;
		int actual = OVT_GMWaypointFormat.ClassifyPrefab(resource);

		if (actual == expected)
			return true;

		SetFailure(label + ": '" + fileName + "' classified as %1, expected %2", TypeName(actual), TypeName(expected));

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one leg count, naming the route shape on failure.
	//! \param[in] waypointCount Waypoints in the route.
	//! \param[in] cyclic Whether the route closes back to its first waypoint.
	//! \param[in] expected Legs the route specification requires.
	//! \param[in] label Human description, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectLegs(int waypointCount, bool cyclic, int expected, string label)
	{
		int actual = OVT_GMWaypointFormat.LegCount(waypointCount, cyclic);

		if (actual == expected)
			return true;

		SetFailure(label + ": LegCount(" + waypointCount.ToString() + ", " + cyclic.ToString() + ") gave %1, expected %2",
			actual.ToString(), expected.ToString());

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Readable name of a waypoint type, so a failure names the kind rather than an ordinal.
	//! \param[in] type An OVT_EGMWaypointType value as int.
	//! \return The enum member name.
	protected string TypeName(int type)
	{
		return typename.EnumToString(OVT_EGMWaypointType, type);
	}
}
