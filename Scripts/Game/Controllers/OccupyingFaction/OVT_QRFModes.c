//------------------------------------------------------------------------------------------------
//! WHAT KIND OF BATTLE OVT_QRFControllerComponent IS RUNNING, AND HOW FAR ALONG IT IS.
//!
//! One controller, two modes (occupying/counter-attacks D14). The alternative - a second controller
//! class beside the QRF's - was rejected because the battle layer is a SINGLETON by design: the
//! campaign's "one battle at a time" invariant is the single field m_CurrentQRF, and everything that
//! keys off it (the player-capture block, the world suppression, the map circle, the HUD panel, the
//! objective director's freeze) would have grown a second thing to ask.
//!
//! ==================================================================================================
//! 🔴 STANDARD IS ZERO, AND STANDARD IS TODAY'S CODE PATH. READ THIS BEFORE ADDING A BRANCH.
//! ==================================================================================================
//! Every battle a PLAYER starts is STANDARD, and a STANDARD battle must be incapable of taking a new
//! code path: it is the most-played event in the game and the one nobody is exercising while this
//! feature is built, so a silent regression there would ship. Every branch on OVT_EQRFMode in the
//! controller therefore leaves the STANDARD side byte-for-byte as it was, and every new field on the
//! controller defaults so that a STANDARD battle never reads it.
//!
//! NEITHER ENUM IS PERSISTED AND NEITHER CROSSES THE WIRE. No siege state is serialized at all
//! (§3.8/§3.9): a save taken mid-siege comes back with no battle, and the objective director resets on
//! its first tick. So unlike OVT_EObjectiveKind/OVT_EObjectivePhase these two MAY be renumbered - but
//! there is no reason to, and anything that later puts one of them in a payload inherits the
//! never-renumber rule with it.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Who started this battle, and therefore how it behaves.
//------------------------------------------------------------------------------------------------
enum OVT_EQRFMode
{
	//! A player captured a base or started an uprising. Announced at once, a 120 s countdown, waves
	//! 4-8 minutes apart, scoring the instant the countdown reaches zero. UNCHANGED BY THIS FEATURE.
	STANDARD = 0,

	//! The occupying faction's objective director is taking a place back. A silent encirclement, a
	//! 30-real-minute muster window, and only then a fight. ONLY OVT_ObjectiveDirectorComponent sets
	//! this, and only through the occupying faction manager's two starters.
	COUNTER_ATTACK = 1
}

//------------------------------------------------------------------------------------------------
//! How far along a COUNTER_ATTACK battle is.
//!
//! ⚠ MEANINGLESS FOR A STANDARD BATTLE. IsEngaged() short-circuits on the mode before it ever looks
//! at the stage, so a standard controller sitting in SILENT_DEPLOY forever is correct and harmless.
//!
//! ⚠ THE MACHINE ONLY EVER GOES FORWARD, and it can never skip BATTLE. Scoring is gated on
//! `m_iTimer <= 0`, which is unreachable in SILENT_DEPLOY (the clock is parked at its 120 000 default
//! and never moves) and in MUSTER (it counts 1 800 000 down to zero, and reaching zero IS the BATTLE
//! transition). A siege that could resolve without passing through BATTLE would leak a permanently
//! suppressed town, because the civilian transition is a PAIR fired at BATTLE and again at the finish.
//------------------------------------------------------------------------------------------------
enum OVT_EQRFStage
{
	//! Troops are being put on the ground and walked out to their ring slots. Nobody has been told:
	//! no notification, no HUD panel, no map circle, no travel or respawn restriction, and the world
	//! goes on living - the economy ticks, deployments run, the town keeps its civilians.
	SILENT_DEPLOY = 0,

	//! The encirclement is complete and the resistance has been told. The clock is showing, the
	//! restrictions are on, and nothing is being scored for 30 real minutes.
	MUSTER = 1,

	//! The assault. Identical to a standard battle from here: zone scoring, the same win condition,
	//! the same handoff to OnQRFFinishedBase/OnQRFFinishedTown.
	BATTLE = 2
}
