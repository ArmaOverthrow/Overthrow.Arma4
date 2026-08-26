//------------------------------------------------------------------------------------------------
//! Whether a deployment may take one of its own vehicles away.
//!
//! ONE ANSWER, AND IT IS FINAL (author, test server 2026-08-24): "it should never retry if that check
//! fails, the vehicle is lost at that point whether a player is nearby or they own it now". A vetoed
//! vehicle is given up permanently - no queue, no timer, no second look. It is part of the world now,
//! and the occupying faction has lost it exactly as it lost the crew.
//!
//! The veto that was missing is the one that matters most in play: KILLING A PATROL'S CREW wipes its
//! spawning module in the same tick, teardown deletes the vehicles, and an intact technical therefore
//! evaporated in front of the player who had just fought for it. Proximity is now a reason in its own
//! right, alongside the two ownership reasons.
//!
//! Before this existed, both teardown paths (the vehicle patrol's ReleaseVehicles and the insertion
//! module's ReleaseTruck) carried their own byte-identical copy of the ownership test and neither
//! asked about proximity at all.
//------------------------------------------------------------------------------------------------
class OVT_VehicleClaim
{
	//! 320 m, the same number the insertion module's abandoned-transport sweep and
	//! OVT_NoPlayersNearbyConditionDeploymentModule already use: baseCloseRange (220) + 100. It is a
	//! statement about what a player can SEE, so it is a plain number and not difficulty-scaled.
	static const int PLAYER_WATCHING_RADIUS_M = 320;

	//------------------------------------------------------------------------------------------------
	//! Why this vehicle must not be deleted, if it must not be.
	//!
	//! Three independent signals, any one of which is enough, all of them the project's existing
	//! notions rather than new ones:
	//!   - A PLAYER OWNER. OVT_PlayerOwnerComponent's uid is stamped by the claim path the moment a
	//!     player sits in the driver's seat of an unowned vehicle, and it is the same test the modded
	//!     SCR_GarbageSystem uses to refuse to collect a car.
	//!   - AN OCCUPANT THAT BELONGS TO A PLAYER. A player-controlled character covers the passenger
	//!     seat, which the claim path never fires for; a character carrying a player owner uid covers
	//!     that player's RECRUITS, who are not player-controlled but are certainly not ours to delete.
	//!   - A PLAYER CLOSE ENOUGH TO SEE IT GO. Not a timing concern: a deployment that has lost a
	//!     vehicle in front of the resistance has lost it.
	//! \param[in] vehicle The vehicle to judge.
	//! \return An empty string when the vehicle may be deleted, or the reason it may not - ever.
	static string DeletionVeto(notnull Vehicle vehicle)
	{
		if (OVT_WorldUtils.PlayerInRange(vehicle.GetOrigin(), PLAYER_WATCHING_RADIUS_M))
			return "a player is close enough to see it go";

		OVT_PlayerOwnerComponent owner = OVT_PlayerOwnerComponent.Cast(vehicle.FindComponent(OVT_PlayerOwnerComponent));
		if (owner && !owner.GetPlayerOwnerUid().IsEmpty())
			return "a player owns it";

		BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(
			vehicle.FindComponent(BaseCompartmentManagerComponent)
		);

		if (!compartments)
			return "";

		PlayerManager players = GetGame().GetPlayerManager();

		array<BaseCompartmentSlot> slots = {};
		compartments.GetCompartments(slots);

		foreach (BaseCompartmentSlot slot : slots)
		{
			if (!slot)
				continue;

			IEntity occupant = slot.GetOccupant();
			if (!occupant)
				continue;

			if (players && players.GetPlayerIdFromControlledEntity(occupant) > 0)
				return "a player is riding in it";

			OVT_PlayerOwnerComponent occupantOwner = OVT_PlayerOwnerComponent.Cast(
				occupant.FindComponent(OVT_PlayerOwnerComponent)
			);

			if (occupantOwner && !occupantOwner.GetPlayerOwnerUid().IsEmpty())
				return "a player's recruit is riding in it";
		}

		return "";
	}

}
