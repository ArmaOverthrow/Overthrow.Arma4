//------------------------------------------------------------------------------------------------
//! THE RULE FOR "A DEPLOYMENT'S FORCE MUST NOT WALK INTO A BATTLE THAT IS ALREADY BEING FOUGHT".
//!
//! ================== WHY THIS EXISTS: TWO FEATURES' DESIGNS DISAGREED ====================
//! The QRF's original design (docs/features/occupying/qrf/context.md) says: "all garrison/patrol/
//! civilian spawn predicates check !m_CurrentQRF ... the contested base loses its defenders by design
//! (perf headroom)". That was written when base defence was base UPGRADES, and the upgrades honoured
//! it by simply not spawning while a battle existed.
//!
//! virtualization/base-defense-migration then moved base defence onto DEPLOYMENTS, whose evaluator
//! states the opposite rule outright: "Existing deployments' groups live entirely on the engine's
//! lifecycle and are unaffected by either [guard], which is a strict improvement on the old behaviour
//! where the whole force also stopped being maintained." Both statements are deliberate; neither is a
//! regression. They simply cannot both hold at the place being fought over.
//!
//! A play-test found the seam: tower guards materialising at a contested base MID-BATTLE, because
//! OVT_DeploymentManagerComponent.EvaluateDeployments()'s QRF guard only ever blocked CREATING
//! deployments, and an already-registered garrison comes up on the engine's proximity lifecycle with
//! nobody asking anybody's permission.
//!
//! THE USER'S DECISION (2026-08-19), which this file encodes and which is deliberately narrow:
//! SUPPRESS ONLY NEAR THE BATTLE. The contested area loses its defenders, exactly as the QRF has
//! always intended; the rest of the map keeps its forces maintained, preserving the migration's
//! improvement. "Near" is the QRF's own QRF_RANGE, so the suppressed area is the same circle the
//! battle already scores, restricts fast travel and blocks respawn inside.
//! =======================================================================================
//!
//! ================== THE THREE CONJUNCTS, AND WHY EACH ONE IS THERE =====================
//!  1. ENGAGED, NOT MERELY EXISTING. A counter-attack siege is a battle object for up to 31 minutes
//!     before a shot is fired (occupying/counter-attacks D15/§3.9). Suppressing during SILENT_DEPLOY
//!     or MUSTER would empty the objective town of its garrison while the siege is still supposed to
//!     be a secret - which is precisely the tell §3.9 exists to avoid. A STANDARD, player-initiated
//!     battle is engaged from creation, so nothing about a player's own battle changes.
//!  2. THE OCCUPYING FACTION ONLY. The QRF's zone-control scoring deliberately counts resistance AI
//!     as holding the ground (occupying/qrf, 2026-08-18), so nerfing the player's own committed
//!     forces mid-battle would be the opposite of what was asked for.
//!  3. WITHIN QRF_RANGE OF THE BATTLE. Measured against OVT_OccupyingFactionManager.m_vQRFLocation,
//!     which is the server-side truth for where the battle is.
//! =======================================================================================
//!
//! PURE BY CONTRACT. Nothing here resolves a manager, a component or a world: every input is passed
//! in, so the rule can be pinned in the Logic tier instead of being inferred from play. Both statics
//! are total - there is no input for which they fail rather than answer.
//------------------------------------------------------------------------------------------------
class OVT_DeploymentBattleSuppression
{
	//------------------------------------------------------------------------------------------------
	//! Is `position` inside the battle's own circle?
	//!
	//! ⚠ THE RADIUS IS THE QRF'S OWN CONSTANT, READ FROM IT, NEVER COPIED. OVT_QRFControllerComponent
	//! .QRF_RANGE is the same 750 m the battle's zone-control scoring, the fast-travel veto and the
	//! respawn veto all use, and a second copy of the number that drifted would give the player a
	//! suppression circle that did not line up with the circle drawn on their map.
	//!
	//! ⚠ STRICTLY LESS THAN, matching every other QRF_RANGE consumer in the tree
	//! (OVT_QRFControllerComponent.c CheckUpdatePoints, OVT_RespawnService.IsInsideQrf,
	//! OVT_FastTravelService). Exactly on the ring is OUTSIDE.
	//! \param[in] battleLocation Where the battle is - m_vQRFLocation on the occupying faction manager.
	//! \param[in] position The position being asked about.
	//! \return True when position is inside the battle circle.
	static bool WithinBattleRange(vector battleLocation, vector position)
	{
		return vector.Distance(battleLocation, position) < OVT_QRFControllerComponent.QRF_RANGE;
	}

	//------------------------------------------------------------------------------------------------
	//! THE WHOLE RULE, in one place, as a function of plain values.
	//!
	//! ⚠ FACTION KEYS, NEVER FACTION INDICES. A virtualization record carries m_sFactionKey precisely
	//! because indices are positional and move between saves (virtualization/core api.md §3), and this
	//! rule is asked about records. An EMPTY occupying key answers false rather than matching another
	//! empty one: "we could not work out who the occupying faction is" must never be read as "this
	//! nameless force is theirs".
	//! \param[in] battleEngaged OVT_OccupyingFactionManager.IsQRFEngaged() - the shooting has started.
	//! \param[in] occupyingFactionKey The occupying faction's key, e.g. "USSR".
	//! \param[in] forceFactionKey The key of the force being asked about.
	//! \param[in] battleLocation Where the battle is.
	//! \param[in] position Where the force is.
	//! \return True when this force must not materialise until the battle is over.
	static bool SuppressesMaterialisation(bool battleEngaged, string occupyingFactionKey, string forceFactionKey, vector battleLocation, vector position)
	{
		if (!battleEngaged)
			return false;

		if (occupyingFactionKey.IsEmpty())
			return false;

		if (forceFactionKey != occupyingFactionKey)
			return false;

		return WithinBattleRange(battleLocation, position);
	}
}
