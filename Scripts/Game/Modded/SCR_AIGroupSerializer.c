//------------------------------------------------------------------------------------------------
//! Keeps a group that vanilla's persistence just RESTORED persistence-tracked.
//!
//! THE PROBLEM THIS SOLVES (virtualization/core T3.1, the Phase 1 T1.3 verdict). Overthrow untracks
//! every AI group unconditionally in Modded/SCR_AIGroup.EOnInit - the BUG-118 fix, because garrisons,
//! patrols, QRFs, deployments and town civilians are all rebuilt from manager state on every boot and
//! their records could never be claimed or deleted. Virtualization's registered groups are the one
//! category that must survive a save, and they opt out through the ArmPersistenceExemption() one-shot
//! armed around their spawn.
//!
//! A group coming back from a SAVE has no such spawn site: the persistence system spawns it itself,
//! from inside this serializer. Its EOnInit would then untrack it and - because UntrackTransient
//! discards the record (`removeData`) - DELETE the very record it was just restored from, a second
//! or so after the world loaded. Arming the exemption here closes that hole deterministically instead
//! of racing the transient-untrack retry queue.
//!
//! Every group this serializer spawns is by definition record-backed, and under Overthrow's config
//! the only AI-group records that can exist are the ones core opted in, so the exemption is exactly
//! as narrow as it needs to be.
//!
//! ⚠ INERT, DELIBERATELY, AND EXPECTED TO STAY THAT WAY. No AI-group config in Overthrow.conf grants
//! `SelfSpawn`, so nothing reaches this serializer's spawn path. Phase 5 settled the question this
//! seam was built against and went the OTHER way (api.md §8, "Route B"): the virtualization manager
//! persists complete re-creation state and rebuilds its own group entities on load, so no vanilla
//! AIGroup record is written for a virtual group at all and the manager's m_bPersistGroupEntities
//! stays false. This file is kept because it is the correct behaviour for the day anything DOES grant
//! an AI group `SelfSpawn` - without it, such a group would be untracked and its record destroyed a
//! second after the world loaded - and because it costs nothing while nothing does.
//------------------------------------------------------------------------------------------------
modded class SCR_AIGroupSerializer
{
	//------------------------------------------------------------------------------------------------
	//! Runs immediately before the persistence system spawns the group entity, which is where the
	//! one-shot has to be armed (EOnInit has already run by the time anyone holds the entity) - the
	//! same contract vanilla uses one line lower with SCR_AIGroup.IgnoreSpawning(true).
	override bool DeserializeSpawnData(out ResourceName prefab, notnull out EntitySpawnParams params, notnull LoadContext context)
	{
		SCR_AIGroup.ArmPersistenceExemption(true);

		bool result = super.DeserializeSpawnData(prefab, params, context);

		// Vanilla leaves IgnoreSpawning armed on purpose (Deserialize disarms it). Ours is consumed
		// by the group's EOnInit; clearing it on the failure path keeps it from leaking onto whatever
		// spawns next.
		if (!result)
			SCR_AIGroup.ArmPersistenceExemption(false);

		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! Belt and braces for the case where the one-shot was consumed by a different entity: the group
	//! exists here, so any queued release can be withdrawn by name.
	override protected bool Deserialize(notnull IEntity entity, notnull LoadContext context)
	{
		SCR_AIGroup.ArmPersistenceExemption(false);

		bool result = super.Deserialize(entity, context);

		OVT_PersistenceManagerComponent.CancelUntrackTransient(entity);

		return result;
	}
}
