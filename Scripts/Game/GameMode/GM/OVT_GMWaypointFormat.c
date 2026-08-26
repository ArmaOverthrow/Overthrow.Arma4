//------------------------------------------------------------------------------------------------
//! Waypoint classification and route geometry for the Game Master waypoint visualization.
//!
//! PURE STATICS, USED ON BOTH SIDES. Every method here is a function of its arguments alone - no
//! state, no engine singletons, no entity lookups, no world access of any kind. That is what makes
//! this file the one genuinely unit-testable piece of the feature, and its Logic-tier case is the
//! only automated evidence the colour table and the leg geometry are right.
//!
//! DO NOT introduce an engine, manager or singleton reference into this file. Its test lives in
//! Scripts/Game/Tests/TestSuites/Logic/, whose guard is a directory-wide text grep, and the pattern
//! it forbids is matched in comments as readily as in code. Two features have already tripped it.
//!
//! WHY THE PREFAB NAME AND NOT A RUNTIME API. There is no engine call for "what kind of waypoint is
//! this". AIWaypoint.GetCompletionType() answers with a completion RULE (All / Leader / Any), not a
//! kind, and nearly every waypoint is the same script class with a different behaviour tree, so
//! ClassName() collapses Move, Patrol, Scout and SearchAndDestroy into one answer. The prefab
//! resource name distinguishes all twelve of the prefabs Overthrow actually assigns
//! (Prefabs/GameMode/OVT_OverthrowGameMode.et:69-82), and classification is then a pure string
//! function.
//!
//! MATCHING IS ON THE EXACT FILE STEM, NOT ON "CONTAINS", AND THAT IS DELIBERATE.
//! AIWaypoint_Defend_ConflictBaseTeamPatrol contains the substring "Patrol": a contains-chain that
//! tested PATROL first would classify every occupying-faction base defend group as a patrol. Exact
//! stem comparison makes the answer independent of the order the table is written in, which is the
//! only form of this function that cannot silently rot. The Logic case pins exactly that name.
//!
//! AN UNRECOGNISED NAME IS UNKNOWN, NEVER A GUESS. A vanilla GM-assigned waypoint, a modded one and
//! an empty string all classify UNKNOWN and render in the neutral base colour. Guessing would paint
//! a waypoint as a defend point on the strength of a substring.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! The coarse kind of a waypoint. It drives colour and nothing else - there is no behaviour, no
//! ordering and no meaning attached to the ordinals beyond "what the wire carries".
//------------------------------------------------------------------------------------------------
enum OVT_EGMWaypointType
{
	UNKNOWN,
	MOVE,
	PATROL,
	DEFEND,
	SEARCH_DESTROY,
	WAIT,
	SCOUT,
	GET_IN,
	GET_OUT,
	ACTION,
	CYCLE
}

//------------------------------------------------------------------------------------------------
//! Classification and route geometry. See the file header for why both live here.
//------------------------------------------------------------------------------------------------
class OVT_GMWaypointFormat
{
	//! The twelve prefab stems assigned in Prefabs/GameMode/OVT_OverthrowGameMode.et:69-82, matched
	//! exactly. AIWaypoint_Patrol appears once although two config attributes point at it.
	protected static const string STEM_MOVE = "AIWaypoint_Move";
	protected static const string STEM_PATROL = "AIWaypoint_Patrol";
	protected static const string STEM_LOITER = "AIWaypoint_Loiter_CO";
	protected static const string STEM_DEFEND = "AIWaypoint_Defend";
	protected static const string STEM_DEFEND_BASE = "AIWaypoint_Defend_ConflictBaseTeamPatrol";
	protected static const string STEM_GET_IN = "AIWaypoint_GetIn";
	protected static const string STEM_GET_OUT = "AIWaypoint_GetOut";
	protected static const string STEM_SCOUT = "AIWaypoint_Scout";
	protected static const string STEM_WAIT = "AIWaypoint_Wait";
	protected static const string STEM_CYCLE = "AIWaypoint_Cycle";
	protected static const string STEM_SEARCH_DESTROY = "AIWaypoint_SearchAndDestroy";
	protected static const string STEM_ACTION = "AIWaypoint_UserAction";

	//------------------------------------------------------------------------------------------------
	//! The coarse kind of the waypoint a resource name refers to.
	//!
	//! Accepts whatever the prefab-name helper returned, which is normally the full braced form
	//! "{GUID}Prefabs/AI/Waypoints/AIWaypoint_Patrol.et". The GUID, the directories and the extension
	//! are all stripped before comparison, so the braced and unbraced forms of one name necessarily
	//! give the same answer.
	//! \param[in] prefabResourceName A prefab resource name, braced or bare, possibly empty.
	//! \return An OVT_EGMWaypointType value as int; UNKNOWN for anything not in the table.
	static int ClassifyPrefab(string prefabResourceName)
	{
		string stem = Stem(prefabResourceName);
		if (stem == "")
			return OVT_EGMWaypointType.UNKNOWN;

		// Exact comparisons: the order of these branches carries no meaning and cannot create the
		// Defend/Patrol trap a contains-chain would.
		if (stem == STEM_MOVE)
			return OVT_EGMWaypointType.MOVE;

		if (stem == STEM_PATROL)
			return OVT_EGMWaypointType.PATROL;

		// A loiter waypoint is a patrol as far as a reader looking at a line is concerned.
		if (stem == STEM_LOITER)
			return OVT_EGMWaypointType.PATROL;

		if (stem == STEM_DEFEND)
			return OVT_EGMWaypointType.DEFEND;

		// Ends in "Patrol" and is not one. This is the case the Logic tier pins.
		if (stem == STEM_DEFEND_BASE)
			return OVT_EGMWaypointType.DEFEND;

		if (stem == STEM_GET_IN)
			return OVT_EGMWaypointType.GET_IN;

		if (stem == STEM_GET_OUT)
			return OVT_EGMWaypointType.GET_OUT;

		if (stem == STEM_SCOUT)
			return OVT_EGMWaypointType.SCOUT;

		if (stem == STEM_WAIT)
			return OVT_EGMWaypointType.WAIT;

		if (stem == STEM_CYCLE)
			return OVT_EGMWaypointType.CYCLE;

		if (stem == STEM_SEARCH_DESTROY)
			return OVT_EGMWaypointType.SEARCH_DESTROY;

		if (stem == STEM_ACTION)
			return OVT_EGMWaypointType.ACTION;

		return OVT_EGMWaypointType.UNKNOWN;
	}

	//------------------------------------------------------------------------------------------------
	//! The file stem of a resource name: no GUID, no directories, no extension.
	//!
	//! "{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et" -> "AIWaypoint_Move", and a bare
	//! "AIWaypoint_Move" is returned unchanged. A name that is nothing but a GUID, or empty, gives "".
	//! \param[in] resourceName The name to reduce.
	//! \return The stem, or "" when there is nothing left after stripping.
	static string Stem(string resourceName)
	{
		string work = resourceName;
		if (work == "")
			return "";

		// Drop a leading "{...}" GUID if one is present. A resource name with a path loses it to the
		// slash strip below anyway; this covers "{GUID}Name.et" with no directories at all.
		int brace = work.IndexOf("}");
		if (brace >= 0)
			work = work.Substring(brace + 1, work.Length() - brace - 1);

		int slash = work.LastIndexOf("/");
		if (slash >= 0)
			work = work.Substring(slash + 1, work.Length() - slash - 1);

		int backslash = work.LastIndexOf("\\");
		if (backslash >= 0)
			work = work.Substring(backslash + 1, work.Length() - backslash - 1);

		int dot = work.LastIndexOf(".");
		if (dot >= 0)
			work = work.Substring(0, dot);

		return work;
	}

	//------------------------------------------------------------------------------------------------
	//! How many legs a route of n waypoints has.
	//!
	//! Vertex 0 is the GROUP's position and vertex i+1 is waypoint i, so leg i runs vertex i to
	//! vertex i+1 and TERMINATES AT WAYPOINT i. That gives n legs for an open route. A cyclic route
	//! adds one more leg closing waypoint n-1 back to waypoint 0, which only exists when there are at
	//! least two waypoints to close between: a single waypoint is drawn as one leg from the group and
	//! nothing else, cyclic or not, and an empty route draws nothing at all.
	//! \param[in] waypointCount Number of waypoints in the route; negative is treated as none.
	//! \param[in] cyclic Whether the route closes back to its first waypoint.
	//! \return The number of legs to draw, never negative.
	static int LegCount(int waypointCount, bool cyclic)
	{
		if (waypointCount <= 0)
			return 0;

		if (cyclic && waypointCount >= 2)
			return waypointCount + 1;

		return waypointCount;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a leg is the one leading to the group's current waypoint.
	//!
	//! currentIndex is -1 whenever the current waypoint is not one of the emitted records - the group
	//! has none, the engine reported the cycle container rather than a child, or truncation dropped
	//! it. All three are first-class states in which NOTHING is highlighted, and none of them is an
	//! error worth printing.
	//! \param[in] legIndex The leg being drawn.
	//! \param[in] currentIndex Index of the current waypoint, or -1 for none.
	//! \return True only for the single leg terminating at the current waypoint.
	static bool IsHighlightLeg(int legIndex, int currentIndex)
	{
		if (currentIndex < 0)
			return false;

		return legIndex == currentIndex;
	}
}
