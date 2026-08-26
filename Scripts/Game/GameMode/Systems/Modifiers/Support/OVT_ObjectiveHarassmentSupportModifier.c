//------------------------------------------------------------------------------------------------
//! THIS HANDLER EXISTS TO CARRY AN INDEX, AND FOR NOTHING ELSE. It has no behaviour on purpose.
//!
//! The "ObjectiveHarassment" support modifier is applied from OUTSIDE the modifier system - by
//! OVT_TownHarassmentBehaviorDeploymentModule, through TryAddSupportModifierByName(), when a group
//! the occupying faction sent has held a town's centre long enough. Nothing about it is periodic, so
//! there is nothing for OnTick() to do and it is deliberately left empty.
//!
//! ⚠ SO WHY SHIP A CLASS AT ALL? Because OVT_TownModifierSystem.PostInit() only assigns
//! config.handler.m_iIndex to entries that HAVE a handler, and a modifier the campaign applies by
//! name needs its handler to be able to answer for itself the way every other applied-by-code
//! modifier does. Ship the empty class, keep the config entry symmetrical with CivilianDeath and
//! ResistanceVictory, and nobody has to work out later why this one entry has no object under it.
//!
//! ⚠ THE CONFIG ENTRY IS APPENDED TO THE END OF Configs/Modifiers/supportModifiers.conf AND MUST
//! STAY THERE. OVT_Modifier.m_iIndex is the POSITIONAL index of the entry in m_aModifiers, and that
//! index is what the replicated per-town modifier lists carry. Inserting an entry anywhere earlier
//! shifts every later index by one and re-labels every modifier in every live save.
//------------------------------------------------------------------------------------------------
class OVT_ObjectiveHarassmentSupportModifier : OVT_SupportModifier
{
	//------------------------------------------------------------------------------------------------
	//! Deliberately empty - see the class header. This modifier is applied by the harassment
	//! deployment module and expires on its own timeout; nothing about it is polled.
	//! \param[in] town The town being ticked. Unused.
	override void OnTick(OVT_TownData town)
	{
	}
}
