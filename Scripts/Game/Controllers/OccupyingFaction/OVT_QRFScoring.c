//------------------------------------------------------------------------------------------------
//! PURE STATICS - how much one fighter is worth to zone control, and which way the score swings.
//!
//! THE HARD RULE, the same one OVT_QRFBearing carries: every function here is a function of its
//! arguments and nothing else. No world, no entity, no manager, no clock, no randomness.
//!
//! WHAT CHANGED AND WHY (author, 2026-08-25): *"QRF scoring should be weighted a little by distance
//! to the QRF so closer characters count for slightly more points than distant ones."* Zone control
//! used to be a flat head count inside QRF_POINT_RANGE - a man standing on the flag and a man 219 m
//! away at the edge of the ring were worth exactly the same, so a force could win a battle without
//! ever contesting the ground it was fighting over.
//!
//! ⚠ "A LITTLE" IS THE WHOLE BRIEF, AND THE FIRST NUMBER TRIED WAS TOO STEEP. At EDGE_WEIGHT 0.5 a
//! man on the objective is worth 1.9x a man at the ring's edge, and four defenders on the flag beat
//! SIX attackers - position overturning a 50% numerical advantage is a different game mode, not a
//! weighting. At 0.75 the flag is worth 1.3x the edge, which reads as intended:
//!
//!   3 on the flag vs 3 at the edge  -> the closer side wins        (position breaks an even fight)
//!   4 on the flag vs 5 at the edge  -> stalemate                   (position offsets one man)
//!   4 on the flag vs 6 at the edge  -> the attackers win           (numbers still beat position)
//!
//! Anyone retuning this should re-run those three cases; they are the whole of the intent.
//------------------------------------------------------------------------------------------------
class OVT_QRFScoring
{
	//! What a fighter at the very edge of the scoring ring is worth, against 1.0 on the objective
	//! itself. Deliberately gentle - see the class header for the three cases that set it.
	static const float EDGE_WEIGHT = 0.75;

	//! How far apart two weighted sides have to be before the score moves off centre, in men. Below
	//! this the battle is a stalemate and the score is pulled towards zero, exactly as an exact tie
	//! used to be. It exists because floats do not have ties: without it, 3.0001 vs 3.0 would read as
	//! a decisive advantage and the "push towards zero" branch could never run again.
	static const float STALEMATE_DEADBAND = 0.5;

	//------------------------------------------------------------------------------------------------
	//! What one fighter at \a distance contributes to zone control.
	//!
	//! Linear from 1.0 at the objective to EDGE_WEIGHT at \a pointRange, and 0 beyond it - the range
	//! test the caller used to make is folded in here, so "outside the ring" and "worth nothing" are
	//! one statement rather than two that can drift apart.
	//! \param[in] distance Metres from the objective.
	//! \param[in] pointRange The scoring ring's radius. Non-positive answers 0 rather than dividing.
	//! \return A weight in [EDGE_WEIGHT, 1.0], or 0 when outside the ring.
	static float FighterWeight(float distance, float pointRange)
	{
		if (pointRange <= 0)
			return 0;

		if (distance >= pointRange)
			return 0;

		if (distance <= 0)
			return 1.0;

		return 1.0 - ((1.0 - EDGE_WEIGHT) * (distance / pointRange));
	}

	//------------------------------------------------------------------------------------------------
	//! Which way the score moves this tick, and by how much.
	//!
	//! The shipped rules, unchanged except that the two sides are now weighted sums rather than head
	//! counts:
	//!   - the resistance holds the ring and there is no enemy anywhere in the battle: +5, the fast
	//!     push that ends an uncontested battle quickly;
	//!   - one side outweighs the other by more than the deadband: +/-1 towards it;
	//!   - otherwise the battle is a stalemate and the score is pulled one step towards zero.
	//! \param[in] resistanceWeight Weighted resistance strength in the ring.
	//! \param[in] occupyingWeight Weighted occupying strength in the ring.
	//! \param[in] occupyingAnywhere Living occupying fighters anywhere in the battle - a HEAD COUNT,
	//!            not a weight: "is anybody left to fight" is a presence question and a man at the far
	//!            edge still answers it yes.
	//! \param[in] currentPoints The score as it stands, for the pull-towards-zero step.
	//! \return The signed delta to apply to the score.
	static int ResolveSwing(float resistanceWeight, float occupyingWeight, int occupyingAnywhere, int currentPoints)
	{
		if (resistanceWeight > 0 && occupyingAnywhere == 0)
			return 5;

		float advantage = resistanceWeight - occupyingWeight;

		if (advantage > STALEMATE_DEADBAND)
			return 1;

		if (advantage < -STALEMATE_DEADBAND)
			return -1;

		// Stalemate: one step towards zero, and never past it.
		if (currentPoints > 0)
			return -1;

		if (currentPoints < 0)
			return 1;

		return 0;
	}
}
