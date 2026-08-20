//------------------------------------------------------------------------------------------------
//! The one way a built Overthrow structure becomes a ruin, and the one way it comes back.
//!
//! ⚠ THIS IS A DESTRUCTION PATH, NOT A REMOVAL PATH. Nothing here deletes anything:
//! OVT_ResistanceFactionManager.DestroyPlacedItem() remains the only way a structure leaves the
//! world. Ruin() answering false is what keeps the two from blurring - a structure that carries no
//! destruction component is removed exactly the way it is removed today, by its caller falling back.
//!
//! Pure statics with no state, in the shape of the navmesh-rebuild facade beside this file: there is
//! no registry of placed structures anywhere in the tree, so there is nothing for a manager to hold.
//!
//! ! No navmesh call belongs here either - the base destruction component already regenerates it
//! (SCR_DestructionMultiPhaseComponent.GoToDamagePhase :189), and the Q4 gate greps this file for it.
//------------------------------------------------------------------------------------------------
class OVT_StructureDamage
{
	//------------------------------------------------------------------------------------------------
	//! AUTHORITY ONLY. Drives a structure to its ruined phase, with effects on every machine.
	//! \param[in] entity The structure, or the root a destructible child hangs from.
	//! \param[in] withEffects Whether the explosion, smoke and sound are raised.
	//! \return True when a destruction component took the change. False means the caller still owns
	//! the entity - for sabotage that means falling back to DestroyPlacedItem().
	static bool Ruin(IEntity entity, bool withEffects = true)
	{
		OVT_StructureDestructionComponent destruction = Resolve(entity);
		if (!destruction)
			return false;

		// A client asking is a silent no-op rather than an error: every real caller is server-side.
		if (!Replication.IsServer())
			return false;

		destruction.RuinIt(withEffects);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! AUTHORITY ONLY. Restores a ruin to its intact model on every machine. Never raises effects.
	//! \param[in] entity The structure, or the root a destructible child hangs from.
	//! \return True when a destruction component took the change.
	static bool Repair(IEntity entity)
	{
		OVT_StructureDestructionComponent destruction = Resolve(entity);
		if (!destruction)
			return false;

		if (!Replication.IsServer())
			return false;

		destruction.RepairIt();

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a structure is currently a ruin. Safe on a client, on a plain prop and on null.
	//! \param[in] entity The structure.
	//! \return True only when a destruction component is present and above its intact phase.
	static bool IsRuined(IEntity entity)
	{
		OVT_StructureDestructionComponent destruction = Resolve(entity);
		if (!destruction)
			return false;

		return destruction.IsRuined();
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a structure's own surfaces should work: its user actions, its shop, its parking and its
	//! support stations. THE PHASE-0 GATE every one of them shares.
	//!
	//! It answers TRUE for everything that is not a ruin - a vehicle, a town building, a prop, null -
	//! so a gate can call it without first asking whether its owner is a buildable at all.
	//!
	//! IT ASKS THE ROOT, not the entity handed to it, because a surface routinely sits somewhere other
	//! than the entity that carries the phase: the recruitment tent's action is on a table CHILD, and
	//! the maintenance ramp's phase is on a child while its surfaces are on the bare root. Going up to
	//! the root and then letting Resolve() come back down one level covers both directions.
	//! \param[in] entity The entity the surface belongs to.
	//! \return False only while a retrofitted structure is in its ruined phase.
	static bool IsUsable(IEntity entity)
	{
		if (!entity)
			return true;

		IEntity root = entity.GetRootParent();
		if (!root)
			root = entity;

		return !IsRuined(root);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether this structure can be ruined at all, i.e. whether the retrofit reached its prefab.
	//! \param[in] entity The structure.
	//! \return True when a destruction component is reachable from it.
	static bool IsDestructible(IEntity entity)
	{
		return Resolve(entity) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! The entity, then ONE level of its children - the vehicle-maintenance-ramp shape, where
	//! OVT_BuildableComponent sits on a bare root and the destructible object is a child. The same
	//! root-then-children walk OVT_LootIntoVehicleAction does for truck beds.
	//! \param[in] entity The structure.
	//! \return Its destruction component, or null.
	static OVT_StructureDestructionComponent Resolve(IEntity entity)
	{
		if (!entity)
			return null;

		OVT_StructureDestructionComponent destruction = OVT_StructureDestructionComponent.Cast(entity.FindComponent(OVT_StructureDestructionComponent));
		if (destruction)
			return destruction;

		IEntity child = entity.GetChildren();
		while (child)
		{
			destruction = OVT_StructureDestructionComponent.Cast(child.FindComponent(OVT_StructureDestructionComponent));
			if (destruction)
				return destruction;

			child = child.GetSibling();
		}

		return null;
	}
}
