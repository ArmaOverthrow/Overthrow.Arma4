//------------------------------------------------------------------------------------------------
//! Pull down the occupying faction's forward operating base.
//!
//! ⚠ THE FIFTEEN-SECOND HOLD IS AUTHORED IN THE PREFAB, NOT HERE. `Duration 15` sits inside the
//! additionalActions block of Prefabs/Bases/OVT_OccupyingFOB.et, exactly as BaseFlag_US.et wires
//! OVT_CaptureBaseAction and TransmitterTower_01_base.et wires OVT_SabotageTowerAction. There is no
//! script-side duration to set and adding one would give the same number two homes.
//!
//! ==========================================================================================
//! ⚠ NOTHING HERE MAY ASK THE DIRECTOR WHERE THE FORWARD BASE IS. NONE OF THE DIRECTOR'S STATE
//! REPLICATES - deliberately, G12: this feature adds no RplProp and no client-visible surface beyond
//! the Game Master panel. On a dedicated server's client, IsFOBUp() is false and GetFOBPosition() is
//! the zero vector, so an action gated on either would be invisible or permanently refused for every
//! player who is not the host. The flag this action is attached to IS the forward base, on every
//! machine, so every client-side question is asked about the OWNER ENTITY and the server re-derives
//! the same questions from its own record.
//! ==========================================================================================
//!
//! ⚠ EVERYTHING THIS CLASS DECIDES, THE SERVER DECIDES AGAIN. CanBePerformedScript runs on the client
//! so the prompt can say WHY it is refusing; OVT_CampaignRequestComponent.RpcAsk_DismantleEnemyFOB
//! reaches OVT_ObjectiveDirectorComponent.OnFOBDismantledByPlayer(), which asks the same rule against
//! the server's own director and the server's own view of where the caller is standing, before
//! anything happens. That is not belt-and-braces - this epic's headline debt is unvalidated capture
//! RPCs (BUG-025) and this feature deliberately adds no third. The request carries NO arguments at
//! all, so there is nothing in the payload for a client to lie about.
//!
//! ⚠ THE REFUSAL REASON IS THE DIRECTOR'S, NOT THIS CLASS'S. CanDismantleFOBAt() answers the key, so
//! the text a player reads and the rule the server enforces cannot drift apart.
//!
//! THE DEFENDER CHECK IS CACHED FOR A SECOND, and that is not premature optimisation: answering it
//! walks every AI agent in the world, and Reforger asks CanBePerformedScript while the prompt is on
//! screen. A second of staleness on "is anybody still shooting at me" is invisible; a full agent walk
//! per frame is not.
//------------------------------------------------------------------------------------------------
class OVT_DismantleEnemyFOBAction : ScriptedUserAction
{
	//! How long an answer stays good for, in milliseconds. See the class header.
	protected const int CACHE_LIFETIME_MS = 1000;

	protected int m_iCacheStamp;
	protected bool m_bCacheValid;
	protected bool m_bCachedAllowed;
	protected string m_sCachedRefusal;

	//------------------------------------------------------------------------------------------------
	//! Asks the server to take the forward base down. It carries nothing: the server uses the caller's
	//! own character origin and its own record of where the base is (BUG-025), so this action only has
	//! to say "I am asking".
	//! \param[in] pOwnerEntity The forward base's flag.
	//! \param[in] pUserEntity The player performing the action.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		OVT_CampaignRequestComponent campaign = OVT_ControllerComponent<OVT_CampaignRequestComponent>.Get();
		if (!campaign)
			return;

		campaign.DismantleEnemyFOB();
	}

	//------------------------------------------------------------------------------------------------
	//! Shown to the resistance and hidden from the occupying faction.
	//!
	//! ⚠ IT DOES NOT ASK WHETHER A FORWARD BASE EXISTS - see the class header. The flag is the forward
	//! base, and the flag is what this action is on; a client cannot see the director's record and must
	//! not pretend to. A structure that has outlived the campaign's record of it shows the action and
	//! gets a stated refusal from the server, which is the honest outcome.
	//! \param[in] user The player looking at it.
	//! \return True when the action belongs on this entity for this player.
	override bool CanBeShownScript(IEntity user)
	{
		if (!user)
			return false;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return false;

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(user);
		if (!character)
			return false;

		// Only the resistance pulls this down. Asked of the looking player's own faction rather than of
		// a campaign flag, because that is the one question a client can answer for itself.
		return character.GetFactionKey() != config.m_sOccupyingFaction;
	}

	//------------------------------------------------------------------------------------------------
	//! Performable only once the site is clear of the occupying faction.
	//! \param[in] user The player performing it.
	//! \return True when the hold may start.
	override bool CanBePerformedScript(IEntity user)
	{
		if (!user)
			return false;

		bool allowed = ResolveAllowed(user.GetOrigin());

		if (!allowed && m_sCachedRefusal != "")
			SetCannotPerformReason(m_sCachedRefusal);

		return allowed;
	}

	//------------------------------------------------------------------------------------------------
	//! The director's rule, applied to the flag this action is on, at most once per CACHE_LIFETIME_MS.
	//! \param[in] position Where the player is standing.
	//! \return Whether a dismantle would be allowed.
	protected bool ResolveAllowed(vector position)
	{
		int now = System.GetTickCount();

		if (m_bCacheValid && now - m_iCacheStamp < CACHE_LIFETIME_MS && now >= m_iCacheStamp)
			return m_bCachedAllowed;

		IEntity owner = GetOwner();
		OVT_ObjectiveDirectorComponent director = OVT_Global.GetObjectiveDirector();

		if (!owner || !director)
		{
			m_bCacheValid = false;
			m_sCachedRefusal = "";
			return false;
		}

		string refusal;
		m_bCachedAllowed = director.CanDismantleFOBAt(position, owner.GetOrigin(), refusal);
		m_sCachedRefusal = refusal;
		m_iCacheStamp = now;
		m_bCacheValid = true;

		return m_bCachedAllowed;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}
}
