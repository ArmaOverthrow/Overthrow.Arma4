//------------------------------------------------------------------------------------------------
//! Base class for user actions on a recruit THE ACTING PLAYER OWNS. The mirror image of
//! OVT_BaseCivilianUserAction, which exists to keep actions OFF recruits.
//!
//! OWNERSHIP COMES FROM THE RECRUIT RECORD, NOT OVT_PlayerOwnerComponent. The record is the one
//! route present on every recruit body: a recruit spawned from the recruit prefab and a civilian
//! recruited in place are different prefabs, and only the manager's table knows both of them are
//! recruits. Reading m_sOwnerPersistentId also means this asks the same question the server asks in
//! OVT_RecruitCommandComponent, so the action shown and the request accepted cannot disagree about
//! who owns what.
//!
//! THE SHOW/PERFORM SPLIT is OVT_LockVehicleAction's (:15-39): CanBeShownScript answers "is this
//! action relevant to this player at all", CanBePerformedScript adds the RplComponent.IsOwner()
//! check that keeps a client from driving an action on a body it does not control. Derived classes
//! override CanShowRecruitAction/CanPerformRecruitAction and never the engine overrides, so the
//! ownership rule cannot be forgotten by one of them.
//!
//! EVERY DEREFERENCE IS GUARDED. This runs on CLIENTS, every frame the player looks at a character,
//! including during JIP when the recruit manager, the config and the replicated recruit table may
//! all be absent - and on any plain civilian, where there is simply no record. A null here is
//! ordinary, not exceptional: it means "not a recruit of mine", and the action hides.
//------------------------------------------------------------------------------------------------
class OVT_BaseRecruitUserAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	//! Override this to add visibility rules on top of "an owned recruit".
	//! \param[in] user The acting character.
	//! \param[in] recruit The record for the entity this action sits on. Never null.
	//! \return True to show the action.
	protected bool CanShowRecruitAction(IEntity user, notnull OVT_RecruitData recruit)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Override this to add performability rules on top of "an owned recruit I control".
	//! Call SetCannotPerformReason() before returning false so the action stays visible and says why.
	//! \param[in] user The acting character.
	//! \param[in] recruit The record for the entity this action sits on. Never null.
	//! \return True when the action can be performed now.
	protected bool CanPerformRecruitAction(IEntity user, notnull OVT_RecruitData recruit)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The recruit record for the entity this action sits on, or null when it is not a recruit.
	//!
	//! GetRecruitFromEntity() has a client path of its own (the RplId map), so this answers on both
	//! sides of the wire - which is what lets an action be shown correctly on a remote client.
	//! \return The record, or null.
	protected OVT_RecruitData GetRecruitRecord()
	{
		IEntity owner = GetOwner();
		if(!owner) return null;

		OVT_RecruitManagerComponent recruits = OVT_Global.GetRecruits();
		if(!recruits) return null;

		return recruits.GetRecruitFromEntity(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the acting player owns the recruit this action sits on.
	//! \param[in] user The acting character.
	//! \param[in] recruit The record to test.
	//! \return True when the record's owner is the acting player.
	protected bool IsOwnedByUser(IEntity user, notnull OVT_RecruitData recruit)
	{
		if(!user) return false;

		if(recruit.m_sOwnerPersistentId.IsEmpty()) return false;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return false;

		string userPersistentId = players.GetPersistentIDFromControlledEntity(user);
		if(userPersistentId.IsEmpty()) return false;

		return recruit.m_sOwnerPersistentId == userPersistentId;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a character is currently sitting in a vehicle compartment.
	//!
	//! Here rather than on the one action that needs it today because the server-side refusal it
	//! mirrors (OVT_RecruitCommandComponent.RESULT_IN_VEHICLE) is a rule about recruit bodies in
	//! general. As of 2026-08-24 CanBeShownScript() asks it for EVERY recruit action, so a derived
	//! class asking it again is belt-and-braces rather than the gate.
	//! \param[in] entity The character to test.
	//! \return True when the entity is in a compartment.
	protected bool IsInCompartment(IEntity entity)
	{
		if(!entity) return false;

		CompartmentAccessComponent compartmentAccess = CompartmentAccessComponent.Cast(entity.FindComponent(CompartmentAccessComponent));
		if(!compartmentAccess) return false;

		return compartmentAccess.IsInCompartment();
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		OVT_RecruitData recruit = GetRecruitRecord();
		if(!recruit) return false;

		if(!IsOwnedByUser(user, recruit)) return false;

		// 🔴 A RECRUIT IN A SEAT OFFERS NOTHING. Author, 2026-08-24: *"hide all their actions when they
		// are in a vehicle (ie remove from group and switch gear). it makes it hard to access the
		// vehicle actions when they are in one."* The recruit's actions and the VEHICLE's actions share
		// one context menu, so every recruit riding along pushes Get Out, the turret and the inventory
		// further down a list the player is trying to use while sitting in the thing.
		//
		// ⚠ ON THE BASE, so it cannot be forgotten by a later action - the same reason the ownership
		// rule lives here. Nothing a player wants to do to a recruit is worth doing while it is seated:
		// parking one in a seat is nonsense (the vehicle drives off with a body "holding position"),
		// and a gear swap with someone in a compartment is worse.
		if(IsInCompartment(GetOwner())) return false;

		return CanShowRecruitAction(user, recruit);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		OVT_RecruitData recruit = GetRecruitRecord();
		if(!recruit) return false;

		if(!IsOwnedByUser(user, recruit)) return false;

		RplComponent genericRpl = RplComponent.Cast(user.FindComponent(RplComponent));
		if(!genericRpl || !genericRpl.IsOwner()) return false;

		return CanPerformRecruitAction(user, recruit);
	}

	//------------------------------------------------------------------------------------------------
	//! Local only, like every other Overthrow action: the effect is a request to the server, and the
	//! server's answer arrives by replication.
	override bool HasLocalEffectOnlyScript() { return true; };
}
