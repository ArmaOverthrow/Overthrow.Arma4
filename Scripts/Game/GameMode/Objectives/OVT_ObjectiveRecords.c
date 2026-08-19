//------------------------------------------------------------------------------------------------
//! The occupying faction's current intent, as plain data.
//!
//! Everything here is a Managed record in the OVT_FOBData shape (OVT_ResistanceFactionManager.c:22-38):
//! public fields, no behaviour, no back-references. They are owned by OVT_ObjectiveDirectorComponent,
//! written by its own version-first serializer, and read by the GM snapshot builder from Phase 8.
//!
//! ⚠ THE ENUM INTEGERS ARE PART OF TWO WIRE FORMATS AND MAY NEVER BE RENUMBERED.
//! They travel in the director's save payload (as ints, positionally) and, from Phase 8, in the GM
//! snapshot. Renumbering one would silently re-label every saved objective and every panel row - the
//! same rule virtualization/integration recorded for OVT_EGroupOrigin. A new member is APPENDED with
//! the next free integer; nothing is ever inserted, reordered or removed.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! What kind of place the current objective is.
//!
//! ⚠ NEVER RENUMBER - see the file header.
//! Villages, radio towers and forward bases are deliberately NOT kinds: villages fall as collateral,
//! towers are handled WITHIN an objective, and a forward base is the occupying faction's own.
//------------------------------------------------------------------------------------------------
enum OVT_EObjectiveKind
{
	NONE = 0,
	TOWN = 1,
	BASE = 2
}

//------------------------------------------------------------------------------------------------
//! How far along the ramp the current objective is.
//!
//! ⚠ NEVER RENUMBER - see the file header.
//! IDLE is zero on purpose: a freshly constructed record, a payload that failed to read and a
//! campaign that has not chosen a target yet all mean the same thing, and all three should read as
//! "no objective" rather than as a half-built harassment phase.
//------------------------------------------------------------------------------------------------
enum OVT_EObjectivePhase
{
	IDLE = 0,
	HARASSMENT = 1,
	FOB = 2,
	COUNTER_QRF = 3
}

//------------------------------------------------------------------------------------------------
//! The current objective: what it is, where it is, how far the ramp has got, and every timer.
//!
//! ⚠ EVERY TIMER HERE IS A TICK COUNT, NOT A DEADLINE (D4). One director tick decrements each of
//! them by one, so an early return in the tick freezes them all by construction and they serialize as
//! plain integers that still mean the same thing in the session that reads them back.
//------------------------------------------------------------------------------------------------
class OVT_ObjectiveRecord : Managed
{
	//! What kind of place this is. NONE means there is no objective, whatever the other fields say.
	OVT_EObjectiveKind kind;

	//! Where it is. THE KEY: positions are what this epic already persists and match back reliably,
	//! and a town id or a base index is neither stable across worlds nor meaningful in a save.
	vector position;

	//! Display name, for logs and for the GM panel. Resolved from the campaign when the objective is
	//! chosen and re-resolved on the first tick after a restore - it is a label, never an identifier,
	//! and is deliberately NOT part of the save payload.
	string name;

	//! Which phase of the ramp the objective is in.
	OVT_EObjectivePhase phase;

	//! Ticks left before this phase gives up and the objective is abandoned. The wedge-breaker: every
	//! phase has an exit, and this is the one that does not depend on anything going right.
	int phaseTicks;

	//! Ticks left before the next operation is sent at this objective.
	int nextOpTicks;

	//! Harassment operations completed here. Drives the group ladder (bigger groups each time).
	int harassmentSuccesses;

	//! Sabotage missions completed here. Drives the base counter-attack gate.
	int sabotageSuccesses;
}

//------------------------------------------------------------------------------------------------
//! The occupying faction's forward operating base for the current objective.
//!
//! ⚠ spent IS A COUNTER, NOT A WALLET (G5). Every resource it records left the ONE deployment pool
//! at the moment its deployment was created; nothing is held here. The ceiling it is counted against
//! comes from OVT_ObjectivePhaseRules.FOBBudgetCeiling().
//------------------------------------------------------------------------------------------------
class OVT_ObjectiveFOBRecord : Managed
{
	//! Whether the forward base is standing. False leaves every field below meaningless rather than
	//! wrong - they are only read while this is true.
	bool up;

	//! Where the structure stands.
	vector position;

	//! The base it is supplied from. Losing that base is one of the three starvation inputs.
	vector sourceBasePosition;

	//! What has been spent from the deployment pool on this forward base and everything sourced from
	//! it, counted against the ceiling.
	int spent;

	//! Consecutive ticks this forward base has been cut off. Counts UP - the one counter in the
	//! machine that does, because recovery has to zero it and a countdown cannot express that. The
	//! freeze covers it identically: no tick, no increment.
	int starvationTicks;

	//! Config name of the deployment that carries the structure. THE RE-LINK KEY: after a load the
	//! director finds the live deployment again by this name plus the position above, on its first
	//! tick, never during deserialization.
	string deploymentName;
}

//------------------------------------------------------------------------------------------------
//! One place the director has agreed to leave alone for a while.
//!
//! An objective that could not be built toward - no forward-base site anywhere in its band, most
//! often - sits out ONE selection round rather than being picked again immediately and failing
//! again. It is a cooldown, not a ban.
//------------------------------------------------------------------------------------------------
class OVT_ObjectiveBlacklistEntry : Managed
{
	//! Where the blacklisted objective was. Matched through OVT_ObjectiveSelection.PositionKey().
	vector position;

	//! Selection rounds still to serve. Zero means served; the entry is pruned on the next selection.
	int roundsLeft;
}

//------------------------------------------------------------------------------------------------
//! One deployment the director created for the current objective, so the reset path can take it back
//! down again.
//!
//! NAME PLUS POSITION, NOT AN ID. Deployment entity ids do not survive a session and the deployment
//! framework's own lookup is name-plus-position (GetDeploymentNearPosition), so this record is
//! exactly the pair that lookup wants. It is runtime-only and is NOT persisted: a restored campaign
//! re-links only the forward base, and anything else the director had standing is either still
//! findable at the objective or already gone.
//------------------------------------------------------------------------------------------------
class OVT_ObjectiveDeploymentRef : Managed
{
	//! The deployment config's name, as the framework registers it.
	string configName;

	//! Where it was created.
	vector position;
}
