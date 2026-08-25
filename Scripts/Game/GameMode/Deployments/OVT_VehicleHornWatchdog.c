//------------------------------------------------------------------------------------------------
//! RELEASES AN AI DRIVER'S HORN WHEN THE ENGINE FORGETS TO.
//!
//! THE FAULT IS BELOW SCRIPT, AND THAT IS WHY THIS EXISTS. `SetVehicleHorn` has ZERO callers in the
//! whole game - not in the vanilla script tree and not in Overthrow, which only ever reads
//! `GetVehicleHorn`. Both the press and the release are native, so nothing script-side can stop the
//! horn going on; the most it can do is write the flag back afterwards.
//!
//! WHY ONLY AI DRIVERS LATCH. For a player the horn is an input action - `VehicleHorn`, Type Analog,
//! FilterPreset "hold" (chimeraInputCommon.conf) - and the input system rewrites the value from the
//! device every frame, so a player physically cannot leave it stuck. An AI driver has no device:
//! whatever the native driving code last wrote just sits in his CharacterInputContext. One missed
//! write-back and the horn sounds until the character is deleted.
//!
//! TRIGGERS ON RECORD, both while the driver is failing to make progress: an unachievable MOVE order
//! the crew keeps retrying (2026-08-23, fixed for mounted forces by clearing the order on arrival),
//! and an obstacle in front of the vehicle - a civilian standing in the road (2026-08-24). Ruled out
//! as causes: Overthrow's LOD pin (removed 2026-08-24 where the transport is an observer; it still
//! latched) and the vehicle prefab driving deltas (reverted to stock the same day; it still latched).
//!
//! A COURTESY HONK IS SHORT. The cap is generous enough that no legitimate honk reaches it, so this
//! never suppresses one - it only ends a hold that was never going to end on its own.
//------------------------------------------------------------------------------------------------
class OVT_VehicleHornWatchdog
{
	//! How long an AI driver may hold the horn before it is written back to zero, in milliseconds.
	static const int MAX_HELD_MS = 4000;

	//------------------------------------------------------------------------------------------------
	//! One observation of a driver's horn. Counter-advance in the same shape as
	//! OVT_InsertionGeometry.AdvanceStuckTicks: the caller owns the accumulator, this returns the new
	//! one.
	//!
	//! Consecutive, not cumulative - a horn that is off resets it, so short honks never add up to a
	//! release.
	//! \param[in] driver The character in the driver's seat. Null is legal (nobody is driving).
	//! \param[in] deltaTime Milliseconds since the caller's last update.
	//! \param[in] heldMs The accumulator as it stands.
	//! \return The new accumulator: advanced, or zero when the horn is off or has just been released.
	static int Tick(IEntity driver, int deltaTime, int heldMs)
	{
		CharacterInputContext input = ResolveInput(driver);
		if (!input)
			return 0;

		// ⚠ NOT `== 0`. A character with no vehicle context reads **-1**, not 0 (measured on a freshly
		// spawned character, 2026-08-25), so an `== 0` test counts every man on foot as honking and
		// would write the flag on him. Only a POSITIVE value is a horn being held.
		if (input.GetVehicleHorn() <= 0)
			return 0;

		int held = heldMs + deltaTime;
		if (held < MAX_HELD_MS)
			return held;

		input.SetVehicleHorn(0);

		Print(string.Format("[Overthrow] Released an AI driver's horn after %1 ms held - the engine never wrote it back (SetVehicleHorn has no other caller in the game)",
			held.ToString()), LogLevel.NORMAL);

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] driver The character to read. Null is legal.
	//! \return His input context, or null when there is no driver or no controller.
	static CharacterInputContext ResolveInput(IEntity driver)
	{
		if (!driver)
			return null;

		CharacterControllerComponent controller = CharacterControllerComponent.Cast(driver.FindComponent(CharacterControllerComponent));
		if (!controller)
			return null;

		return controller.GetInputContext();
	}
}
