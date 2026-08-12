//------------------------------------------------------------------------------------------------
//! TIER A case - the map faction palette's CONTRACT, deliberately not its colours.
//!
//! OVT_MapFactionPalette decides what colour the map draws a faction in, by campaign ROLE rather than
//! by the faction's own engine colour. Its classification half (TryGetRole) reads the campaign config
//! and belongs to the manual gate; everything asserted below is world-free.
//!
//! ⚠️ THIS CASE PINS PROPERTIES, NOT SHADES. The four default colours exist to be curated in
//! play-testing - that is the entire reason the per-layer palettes exist - so a test that asserted
//! "occupying is 0.855 0.031 0.028" would fail every time somebody did the thing the feature is for.
//! What it pins instead is what must stay true at any shade:
//!  - THE ROLES ARE DISTINGUISHABLE. Occupying, resistance and supporting must not resolve to the same
//!    colour. A palette whose roles collapse renders a map that cannot be read at all, and the
//!    likeliest way to get one is a copy-paste while retuning;
//!  - AN UNROLED FACTION RESOLVES TO NULL, not to a colour. The three marker overrides disagree about
//!    what unknown looks like (town black, base and radio tower white) and each supplies its own
//!    fallback, so a palette that answered with m_UnknownColor here would silently restyle every
//!    unowned marker to white;
//!  - GetColorForRole NEVER returns null, which is the other half of that split: the role path has no
//!    caller-side fallback, and a null there would black-hole an icon tint;
//!  - THE PACKED ALPHA IS THE CALLER'S, NEVER THE COLOUR'S. This is the load-bearing one. Alpha is how
//!    the map carries meaning hue does not - contested versus held territory, in-effect versus
//!    suppressed influence, restricted versus friendly-restricted rings - so packing the palette
//!    colour's own alpha would flatten three separate distinctions at once while leaving every hue
//!    looking correct;
//!  - alpha is CLAMPED rather than wrapped, so an out-of-range value degrades to opaque or invisible
//!    rather than to a wrapped byte that reads as a plausible wrong alpha.
//!
//! PROVEN ABLE TO FAIL (CLAUDE.md: no case ships without this). Four inversions, each reverted:
//!  1. GetColorForFactionIndex returning m_UnknownColor instead of null for an unroled index -> failed
//!     the null-contract assertion only;
//!  2. PackColor using colour.A() * 255 instead of the alpha parameter -> failed both alpha
//!     assertions and left every RGB assertion green, which is exactly the silent shape described
//!     above;
//!  3. dropping the Math.ClampInt on alpha -> failed the clamp assertions (300 packed as 44);
//!  4. GetColorForRole's RESISTANCE_FACTION arm returning m_OccupyingColor -> failed the
//!     roles-are-distinguishable assertion.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_MapFactionPalette : SCR_AutotestCaseBase
{
	//! An alpha that is neither 0 nor 255 nor any colour's own, so a packed byte matching it cannot be
	//! a coincidence.
	protected static const int PROBE_ALPHA = 137;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		// A script-constructed palette, i.e. the GetDefault path: no config supplied any attribute, so
		// this also asserts EnsureDefaults actually fills them.
		OVT_MapFactionPalette palette = new OVT_MapFactionPalette();

		Color occupying = palette.GetColorForRole(OVT_FactionType.OCCUPYING_FACTION);
		Color resistance = palette.GetColorForRole(OVT_FactionType.RESISTANCE_FACTION);
		Color supporting = palette.GetColorForRole(OVT_FactionType.SUPPORTING_FACTION);

		// --- THE ROLE PATH ALWAYS ANSWERS. It has no caller-side fallback.
		if (!occupying || !resistance || !supporting)
		{
			SetResultFailure("A campaign role resolved to a null colour - the role path has no caller-side fallback, so every one of the three must answer");
			return true;
		}

		// --- THE ROLES ARE DISTINGUISHABLE.
		if (SameColor(occupying, resistance))
		{
			SetResultFailure("The occupying and resistance roles resolved to the same colour - a map whose sides share a colour cannot be read");
			return true;
		}

		if (SameColor(occupying, supporting) || SameColor(resistance, supporting))
		{
			SetResultFailure("The supporting role shares a colour with the occupying or resistance role");
			return true;
		}

		// --- AN UNROLED FACTION IS THE CALLER'S PROBLEM, NOT THE PALETTE'S. A negative index is
		// "no faction" and is refused before the campaign config is ever consulted, which is what makes
		// this assertion world-free.
		if (palette.GetColorForFactionIndex(-1))
		{
			SetResultFailure("An unroled faction index resolved to a colour - the three marker overrides supply their own unknown colour (town black, base and tower white) and depend on getting null here");
			return true;
		}

		// --- THE PACKED ALPHA IS THE CALLER'S. Packed through the unroled path on purpose: that is the
		// one branch where the palette picks the colour itself, so a leaked colour alpha would show.
		int packed = palette.GetArgb(-1, PROBE_ALPHA);
		int alpha, red, green, blue;
		Color.UnpackInt(packed, alpha, red, green, blue);

		if (alpha != PROBE_ALPHA)
		{
			SetResultFailure("GetArgb packed alpha %1 where the caller asked for %2 - alpha carries contested-versus-held, in-effect-versus-suppressed and restricted-versus-friendly, and the palette must never supply it", alpha.ToString(), PROBE_ALPHA.ToString());
			return true;
		}

		// The unknown colour is white by default; assert it is opaque-white RGB rather than a role hue,
		// so a fallback that silently picked the occupier would be caught.
		if (red != 255 || green != 255 || blue != 255)
		{
			SetResultFailure("The unroled fallback packed RGB %1 %2 %3 rather than white - a hue there would read as a faction that may not be there", red.ToString(), green.ToString(), blue.ToString());
			return true;
		}

		// --- ALPHA IS CLAMPED, NOT WRAPPED.
		Color.UnpackInt(palette.GetArgb(-1, 300), alpha, red, green, blue);
		if (alpha != 255)
		{
			SetResultFailure("An alpha of 300 packed as %1 rather than clamping to 255", alpha.ToString());
			return true;
		}

		Color.UnpackInt(palette.GetArgb(-1, -5), alpha, red, green, blue);
		if (alpha != 0)
		{
			SetResultFailure("An alpha of -5 packed as %1 rather than clamping to 0", alpha.ToString());
			return true;
		}

		// --- THE ROLE PACK USES THE ROLE'S OWN RGB at the caller's alpha, which is what keeps a marker
		// and the region beneath it the same hue at two different opacities.
		Color.UnpackInt(palette.GetArgbForRole(OVT_FactionType.OCCUPYING_FACTION, PROBE_ALPHA), alpha, red, green, blue);

		if (alpha != PROBE_ALPHA)
		{
			SetResultFailure("GetArgbForRole packed alpha %1 where the caller asked for %2", alpha.ToString(), PROBE_ALPHA.ToString());
			return true;
		}

		if (red != Math.ClampInt(Math.Round(occupying.R() * 255), 0, 255) || green != Math.ClampInt(Math.Round(occupying.G() * 255), 0, 255) || blue != Math.ClampInt(Math.Round(occupying.B() * 255), 0, 255))
		{
			SetResultFailure("GetArgbForRole packed RGB %1 %2 %3, which is not the occupying role's own colour", red.ToString(), green.ToString(), blue.ToString());
			return true;
		}

		SetResultSuccess();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! RGB-only equality. Alpha is excluded deliberately: two roles that differ only in alpha are still
	//! the same hue on the map, and this test is about whether the sides can be told apart.
	protected bool SameColor(Color a, Color b)
	{
		return Math.Round(a.R() * 255) == Math.Round(b.R() * 255)
			&& Math.Round(a.G() * 255) == Math.Round(b.G() * 255)
			&& Math.Round(a.B() * 255) == Math.Round(b.B() * 255);
	}
}
