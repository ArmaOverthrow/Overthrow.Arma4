//------------------------------------------------------------------------------------------------
//! One extra "why can't I use this support station" reason: the player cannot pay for the fuel.
//!
//! WHY AN ENUM VALUE AND NOT A BESPOKE CHECK. The vanilla support-station stack already plumbs a
//! refusal reason all the way from SCR_BaseSupportStationComponent.IsValid, through
//! SCR_SupportStationManagerComponent.GetClosestValidSupportStation (which keeps the HIGHEST reason
//! it saw), into SCR_BaseUseSupportStationAction.SetCanPerform and out to the player as a localized
//! string via GetInvalidPerformReasonString. Refusing through that seam greys the action, shows a
//! real reason and - because the server re-runs CanBePerformedScript inside PerformAction - stops the
//! refuel authoritatively. One value buys all of that (implementation.md D6).
//!
//! WHY 550. The manager resolves competing reasons by taking the numerically highest one, so the
//! value chosen decides which message a player sees when several stations refuse at once:
//!   - above the fuel band (NO_FUEL_TO_GIVE 500 / FUEL_CANISTER_EMPTY 501 / FUEL_TANK_FULL 502), so a
//!     broke player standing at a pump that HAS fuel is told the actionable thing - that they cannot
//!     pay - rather than something about the pump;
//!   - below HEAL_* (600), IS_MOVING (800) and IN_USE (9999), which are all conditions no amount of
//!     money would fix and therefore deserve to win.
//!
//! Explicit values in a modded enum are legal: the base game does exactly this in
//! scripts/Game/Enums/ETagCategory.c. (The "user defined values are not allowed" note in
//! Core/generated/Attributes/EnumBitFlag.c is about the UI attribute widgets, not the language.)
//------------------------------------------------------------------------------------------------
modded enum ESupportStationReasonInvalid
{
	OVT_CANNOT_AFFORD_FUEL = 550,	//!< The refuelling player cannot afford the next tick of fuel
}
