//------------------------------------------------------------------------------------------------
//! "Do not fortify a place a player is standing in."
//!
//! THE ONE RULE THE FUNDING REWRITE WOULD OTHERWISE LOSE. Base defense used to be bought by
//! OVT_OccupyingFactionManager.CheckUpdate(), which skipped any base with a player inside
//! baseCloseRange + 100 - the line whose comment reads "Dont spawn stuff if a player is watching
//! lol". Under the virtualization lifecycle that concern is sharper, not softer: creating a
//! deployment beside a player materialises its groups IN FRONT OF HIM, because he is an observer and
//! the engine spawns registered groups by observer proximity. So the rule survives as a CREATION
//! GATE on any config that authors this module.
//!
//! ================== THE ASYMMETRY IS THE WHOLE DESIGN OF THIS MODULE ======================
//!   EvaluateStaticCondition()  gates CREATION  -> answers the distance question.
//!   EvaluateCondition()        gates the RUNTIME -> returns TRUE, ALWAYS, UNCONDITIONALLY.
//!
//! Getting that backwards deletes a base's garrison the moment a player arrives. The reinforcement
//! module asks EvaluateCondition() on every one of its checks, and a config authored with
//! m_bDeleteOnConditionFail - which every base-defense config is - DELETES ITS OWN DEPLOYMENT when
//! the answer turns false. A runtime distance test would therefore collect the defense of any base
//! a player walked into: the fight would evaporate on approach, the occupying faction would be
//! refunded for it, and the player would find an empty base. That is the exact opposite of what this
//! module is for, and it would look like a spawning bug rather than a condition bug.
//!
//! "The player must not WATCH it appear" is a statement about the instant of creation. It says
//! nothing about whether the defense may continue to exist while he is there - it obviously must.
//! ==========================================================================================
//!
//! NO PLAYERS AT ALL PASSES. GetPlayerProximity() answers float.MAX when nobody is connected, which
//! is the state a dedicated server is in before the first join and the state the free-at-game-start
//! seeding pass runs in. A baseline force must be on the ground before anyone arrives.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_NoPlayersNearbyConditionDeploymentModule : OVT_BaseConditionDeploymentModule
{
	[Attribute(desc: "Name of this module")]
	string m_sModuleName;

	//! 500 (author, 2026-08-29), up from the legacy 320 = baseCloseRange (220) + 100. Authored as a
	//! plain number rather than read off the difficulty settings on purpose: it is a property of what
	//! a player can SEE appear, which does not change because a campaign is set to Easy, and a config
	//! that wants a different distance can author one. It is the SAME number as
	//! OVT_InfantrySpawningDeploymentModule.m_fNoSpawnNearResistance, deliberately: the two gates sit at
	//! opposite ends of the same path and a gap between them is exactly what let men through before.
	[Attribute(defvalue: "500", desc: "A deployment of this config is never CREATED with living resistance closer than this to the candidate position. Runtime is unaffected - see the class header. Matches the convergence gate's own 500 m")]
	float m_fMinPlayerDistance;

	//------------------------------------------------------------------------------------------------
	//! Creation gate: is every player far enough away that this deployment can appear unseen?
	//! \param[in] position The candidate position.
	//! \param[in] factionIndex The faction the evaluator is deploying for. Unused - a player of any
	//!            faction watching a force appear is the same problem.
	//! \param[in] threatLevel The candidate's scored threat. Unused.
	//! \return True when the nearest player is at least m_fMinPlayerDistance away, or when there is
	//!         no player at all.
	override bool EvaluateStaticCondition(vector position, int factionIndex, float threatLevel)
	{
		// ⚠ THE RESISTANCE, BY DISTANCE, AND NOTHING ELSE (author, 2026-08-25: "it shouldnt be line of
		// sight gated at all, simply distance to the nearest resistance").
		//
		// 🔴 THE "ASK PLAYERMANAGER AS WELL" WORKAROUND MOVED INTO OVT_ResistancePresence (2026-08-29).
		// It lived here, privately, because the sphere query has a reproduced blind spot - it answered
		// "nobody" with a real player-controlled character standing on the candidate position. Keeping
		// the workaround in this one module is precisely why every OTHER gate in the framework - the
		// rebuy gate above all - kept letting men through. IsGroundHeld now fails closed for every
		// caller, so this is one call again.
		return !OVT_ResistancePresence.IsGroundHeld(position, m_fMinPlayerDistance);
	}

	//------------------------------------------------------------------------------------------------
	//! Runtime gate: ALWAYS TRUE, and the class header says why in full.
	//!
	//! ⚠ DO NOT "FIX" THIS TO TEST THE DISTANCE. With m_bDeleteOnConditionFail authored - which every
	//! base-defense config authors - this method returning false collects the deployment. Testing
	//! player proximity here would delete a base's whole defense the moment a player walked in.
	//! \return True, unconditionally.
	override bool EvaluateCondition()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! EVERY attribute has to appear here. A module is cloned out of its config template for each
	//! deployment and CloneModule copies by hand, so a forgotten attribute silently ships the class
	//! default instead of the authored value - that is how m_fMaxCruiseSpeed was lost on the vehicle
	//! module for a whole release. A dropped m_fMinPlayerDistance would clone as 0, which passes every
	//! position and turns this module into a no-op nobody would notice until a play-tester watched a
	//! garrison materialise around him.
	override OVT_BaseDeploymentModule CloneModule()
	{
		OVT_NoPlayersNearbyConditionDeploymentModule clone = new OVT_NoPlayersNearbyConditionDeploymentModule();

		clone.m_sModuleName = m_sModuleName;
		clone.m_fMinPlayerDistance = m_fMinPlayerDistance;

		return clone;
	}

	//------------------------------------------------------------------------------------------------
	void PrintDebugInfo()
	{
		Print(string.Format("No Players Nearby Condition Module: %1", m_sModuleName));
		Print(string.Format("  Minimum Player Distance: %1m (creation only)", m_fMinPlayerDistance));
	}
}
