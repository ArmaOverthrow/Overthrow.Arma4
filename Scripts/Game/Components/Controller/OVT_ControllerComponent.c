//------------------------------------------------------------------------------------------------
//! THE way to reach a component on the LOCAL player's OVT_OverthrowController.
//!
//! Usage: OVT_ControllerComponent<OVT_ShopTransactionComponent>.Get()
//!
//! WHY A GENERIC CLASS. EnforceScript has no generic METHODS, so a static on a generic CLASS is how
//! this project stands in for one - exactly the trick OVT_ComponentFinder<Class T> already uses
//! (OVT_Component.c:10), which this composes with rather than duplicating.
//!
//! WHY IT REPLACES SIX HAND-WRITTEN OVT_Global GETTERS. Every one of them was the same three lines -
//! GetController(), null-check, Cast - and each new controller component added another copy to a file
//! that is meant to be a service locator, not a component registry. With this accessor, adding a
//! component to OVT_OverthrowController costs nothing in OVT_Global: the rule is now mechanically
//! true that **no OVT_Global getter is ever added for a controller component**.
//!
//! THE NAME AND SHAPE ARE EPIC-LEVEL API, NOT AN IMPLEMENTATION DETAIL. Two other features have
//! already committed to this exact spelling in prose, before it existed:
//!   - docs/features/core/options/requirements.md:41        (OVT_OptionsComponent on the controller)
//!   - docs/features/core/player-groups/implementation.md:397 ("no new OVT_Global getter")
//! Renaming it breaks those contracts. Do not rename it.
//!
//! IT RETURNS NULL MORE OFTEN THAN A MANAGER GETTER DOES, AND THAT IS NORMAL:
//!   - on a DEDICATED SERVER there is no local player, so it is null forever;
//!   - on a client before ownership assignment (OVT_OverthrowController.RpcDo_NotifyOwnerAssignment)
//!     the controller is not registered yet;
//!   - a component that is not on Prefabs/GameMode/OVT_OverthrowController.et resolves null with no
//!     compile error and no runtime error - which is why every new component gets an Init-tier case
//!     asserting it resolves off a registered controller.
//! EVERY caller must null-check. A dropped client-local action is acceptable; a script error is not.
//------------------------------------------------------------------------------------------------
class OVT_ControllerComponent<Class T>
{
	//------------------------------------------------------------------------------------------------
	//! Finds the component of type T on the local player's controller entity.
	//! \return The component, or null when there is no local controller or it does not carry T.
	static T Get()
	{
		return OVT_ComponentFinder<T>.Find(OVT_Global.GetController());
	}
}
