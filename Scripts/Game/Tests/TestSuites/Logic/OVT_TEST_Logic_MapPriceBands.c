//------------------------------------------------------------------------------------------------
//! TIER A case - the map shop indicator's caret and badge banding.
//!
//! This is the ONE part of map/location-types that can be asserted world-free. OVT_MapShopPriceBands
//! is a static function of (townStock, townMaxStock) and (distanceToPort) with no manager, no widget
//! and no world behind it, which is exactly why the calculator was split into a pure half and a
//! reading half in the first place.
//!
//! WHAT IT PINS:
//!  - the scarcity term reproduces the economy's own arithmetic, including its divide-by-zero clamp;
//!  - every band boundary resolves to the LESS EXTREME band, uniformly, in both directions;
//!  - the neutral band draws nothing at all - no glyph, no direction word - which is what makes
//!    "omit the row" the only possible rendering of a neutral item;
//!  - a negative distance to port hides the badge instead of banding as a huge discount. That case
//!    is real: the distance lookup returns -1 when no ports are registered, and a naive band would
//!    render a confident claim built on the absence of data.
//!
//! FLOAT DISCIPLINE. Scarcity boundaries are asserted EXACTLY, because 2.5, 5.0 and 7.5 are all
//! exactly representable in binary and the band edges are those same literals. Remoteness boundaries
//! are asserted one metre either side instead: the per-metre coefficient 0.0001 is NOT exactly
//! representable, so a distance of exactly 500.000 m lands on whichever side the rounding falls, and
//! a millimetre of port distance is not a behaviour anybody can observe. Asserting it would be
//! pinning the float unit in the last place, not the design.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_MapPriceBands : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		// --- THE SCARCITY TERM, against the arithmetic it mirrors:
		//     100 * 0.1 * (1 - stock / max), i.e. +10 when sold out and 0 at exactly full.
		if (!ExpectPercent(0, 10, 10.0, "a sold-out item in a town with capacity 10"))
			return true;

		if (!ExpectPercent(5, 10, 5.0, "an item at half the town's capacity"))
			return true;

		if (!ExpectPercent(10, 10, 0.0, "an item at exactly the town's capacity"))
			return true;

		// Stock legitimately overshoots to twice capacity before shops refuse to buy, so the term
		// genuinely reaches -10 and real bargains exist. This is the case that makes the down carets
		// reachable at all.
		if (!ExpectPercent(20, 10, -10.0, "a town glutted to twice its capacity"))
			return true;

		// The divide-by-zero clamp: a capacity below 1 is treated as 1, never divided by.
		if (!ExpectPercent(0, 0, 10.0, "a sold-out item whose town capacity computed as zero"))
			return true;

		if (!ExpectPercent(2, 0, -10.0, "two units held against a town capacity that computed as zero"))
			return true;

		if (!ExpectPercent(0, -50, 10.0, "a negative town capacity"))
			return true;

		// --- CARET BANDS. Every boundary belongs to the LESS EXTREME band, in both directions.
		if (!ExpectScarcityLevel(10.0, OVT_MapPriceLevel.UP_3, "the top of the range"))
			return true;

		if (!ExpectScarcityLevel(7.6, OVT_MapPriceLevel.UP_3, "just inside three carets up"))
			return true;

		if (!ExpectScarcityLevel(7.5, OVT_MapPriceLevel.UP_2, "exactly on the two/three caret boundary"))
			return true;

		if (!ExpectScarcityLevel(5.1, OVT_MapPriceLevel.UP_2, "just inside two carets up"))
			return true;

		if (!ExpectScarcityLevel(5.0, OVT_MapPriceLevel.UP_1, "exactly on the one/two caret boundary"))
			return true;

		if (!ExpectScarcityLevel(2.6, OVT_MapPriceLevel.UP_1, "just inside one caret up"))
			return true;

		if (!ExpectScarcityLevel(2.5, OVT_MapPriceLevel.NEUTRAL, "exactly on the neutral/one caret boundary"))
			return true;

		if (!ExpectScarcityLevel(0.0, OVT_MapPriceLevel.NEUTRAL, "an item at exactly the town's capacity"))
			return true;

		if (!ExpectScarcityLevel(-2.5, OVT_MapPriceLevel.NEUTRAL, "exactly on the neutral/one caret down boundary"))
			return true;

		if (!ExpectScarcityLevel(-2.6, OVT_MapPriceLevel.DOWN_1, "just inside one caret down"))
			return true;

		if (!ExpectScarcityLevel(-5.0, OVT_MapPriceLevel.DOWN_1, "exactly on the one/two caret down boundary"))
			return true;

		if (!ExpectScarcityLevel(-5.1, OVT_MapPriceLevel.DOWN_2, "just inside two carets down"))
			return true;

		if (!ExpectScarcityLevel(-7.5, OVT_MapPriceLevel.DOWN_2, "exactly on the two/three caret down boundary"))
			return true;

		if (!ExpectScarcityLevel(-7.6, OVT_MapPriceLevel.DOWN_3, "just inside three carets down"))
			return true;

		if (!ExpectScarcityLevel(-10.0, OVT_MapPriceLevel.DOWN_3, "the bottom of the range"))
			return true;

		// The two ends of the stock range, straight through the convenience entry point, so the
		// composition of GetScarcityPercent and GetScarcityLevel is pinned as well as each half.
		if (OVT_MapShopPriceBands.GetScarcityLevelForStock(0, 10) != OVT_MapPriceLevel.UP_3)
		{
			SetResultFailure("A sold-out item banded as %1, expected three carets up",
				typename.EnumToString(OVT_MapPriceLevel, OVT_MapShopPriceBands.GetScarcityLevelForStock(0, 10)));
			return true;
		}

		if (OVT_MapShopPriceBands.GetScarcityLevelForStock(10, 10) != OVT_MapPriceLevel.NEUTRAL)
		{
			SetResultFailure("An item at exactly the town's capacity banded as %1, expected neutral",
				typename.EnumToString(OVT_MapPriceLevel, OVT_MapShopPriceBands.GetScarcityLevelForStock(10, 10)));
			return true;
		}

		if (OVT_MapShopPriceBands.GetScarcityLevelForStock(20, 10) != OVT_MapPriceLevel.DOWN_3)
		{
			SetResultFailure("A town glutted to twice capacity banded as %1, expected three carets down",
				typename.EnumToString(OVT_MapPriceLevel, OVT_MapShopPriceBands.GetScarcityLevelForStock(20, 10)));
			return true;
		}

		// --- THE NEUTRAL BAND DRAWS NOTHING. There is no neutral glyph in the imageset and none is
		// wanted: a neutral item is omitted from the panel entirely, and that is only a safe rendering
		// rule while neutral keeps yielding an empty quad name and an empty caption.
		if (!OVT_MapShopPriceBands.IsNeutral(OVT_MapPriceLevel.NEUTRAL))
		{
			SetResultFailure("IsNeutral() did not recognise the neutral band");
			return true;
		}

		if (OVT_MapShopPriceBands.GetCaretIcon(OVT_MapPriceLevel.NEUTRAL) != "")
		{
			SetResultFailure("The neutral band asked for the glyph '%1'; it must ask for none",
				OVT_MapShopPriceBands.GetCaretIcon(OVT_MapPriceLevel.NEUTRAL));
			return true;
		}

		if (OVT_MapShopPriceBands.GetMagnitude(OVT_MapPriceLevel.NEUTRAL) != 0)
		{
			SetResultFailure("The neutral band claimed %1 carets, expected 0",
				OVT_MapShopPriceBands.GetMagnitude(OVT_MapPriceLevel.NEUTRAL).ToString());
			return true;
		}

		if (OVT_MapShopPriceBands.IsUp(OVT_MapPriceLevel.NEUTRAL) || OVT_MapShopPriceBands.IsDown(OVT_MapPriceLevel.NEUTRAL))
		{
			SetResultFailure("The neutral band claimed a direction");
			return true;
		}

		// --- THE SIX DRAWN BANDS: distinct glyphs, correct magnitude, correct direction.
		array<OVT_MapPriceLevel> drawn = {
			OVT_MapPriceLevel.DOWN_3, OVT_MapPriceLevel.DOWN_2, OVT_MapPriceLevel.DOWN_1,
			OVT_MapPriceLevel.UP_1, OVT_MapPriceLevel.UP_2, OVT_MapPriceLevel.UP_3
		};

		array<string> seenGlyphs = new array<string>();

		foreach (OVT_MapPriceLevel level : drawn)
		{
			string glyph = OVT_MapShopPriceBands.GetCaretIcon(level);

			if (glyph == "")
			{
				SetResultFailure("Band %1 asked for no glyph", typename.EnumToString(OVT_MapPriceLevel, level));
				return true;
			}

			if (seenGlyphs.Contains(glyph))
			{
				SetResultFailure("Glyph '%1' is used by more than one band", glyph);
				return true;
			}

			seenGlyphs.Insert(glyph);

			if (OVT_MapShopPriceBands.IsUp(level) == OVT_MapShopPriceBands.IsDown(level))
			{
				SetResultFailure("Band %1 is neither, or both, of up and down", typename.EnumToString(OVT_MapPriceLevel, level));
				return true;
			}

			int magnitude = OVT_MapShopPriceBands.GetMagnitude(level);
			if (magnitude < 1 || magnitude > 3)
			{
				SetResultFailure("Band %1 claimed %2 carets, expected 1 to 3",
					typename.EnumToString(OVT_MapPriceLevel, level), magnitude.ToString());
				return true;
			}
		}

		// The exact quad names, because they are a contract with the imageset and a typo in either
		// half renders a blank row that nothing else in this project can catch.
		if (!ExpectGlyph(OVT_MapPriceLevel.UP_1, "up_1") || !ExpectGlyph(OVT_MapPriceLevel.UP_2, "up_2") || !ExpectGlyph(OVT_MapPriceLevel.UP_3, "up_3"))
			return true;

		if (!ExpectGlyph(OVT_MapPriceLevel.DOWN_1, "down_1") || !ExpectGlyph(OVT_MapPriceLevel.DOWN_2, "down_2") || !ExpectGlyph(OVT_MapPriceLevel.DOWN_3, "down_3"))
			return true;

		// --- DIRECTION IS CARRIED BY THE GLYPH ALONE (2026-08-10: the "Dearer"/"Cheaper" captions were
		// removed by user directive, so GetDirectionKey no longer exists). F-6 requires up-versus-down
		// to survive with COLOUR removed, and the glyph is a directional shape drawn in flat white, so
		// the property that must now hold is that up and down never share a quad. Asserted above by the
		// seenGlyphs uniqueness check and pinned by name in the ExpectGlyph block, which is what would
		// catch an atlas edit that made up_2 and down_2 identical.
		if (OVT_MapShopPriceBands.GetCaretIcon(OVT_MapPriceLevel.UP_2) == OVT_MapShopPriceBands.GetCaretIcon(OVT_MapPriceLevel.DOWN_2))
		{
			SetResultFailure("Up and down asked for the same glyph, so direction cannot be read from shape");
			return true;
		}

		// --- THE REMOTENESS BADGE.
		// A negative distance means "no ports are registered", not "next to a port". It must hide.
		if (!ExpectRemoteness(-1, OVT_MapPriceLevel.NEUTRAL, "no ports registered at all"))
			return true;

		if (!ExpectRemoteness(-9999, OVT_MapPriceLevel.NEUTRAL, "a wildly negative distance"))
			return true;

		if (!ExpectRemoteness(0, OVT_MapPriceLevel.NEUTRAL, "a shop standing on a port"))
			return true;

		if (!ExpectRemoteness(499, OVT_MapPriceLevel.NEUTRAL, "a shop just inside the hidden band"))
			return true;

		if (!ExpectRemoteness(501, OVT_MapPriceLevel.UP_1, "a shop just past the hidden band"))
			return true;

		if (!ExpectRemoteness(1499, OVT_MapPriceLevel.UP_1, "a shop just inside one caret"))
			return true;

		if (!ExpectRemoteness(1501, OVT_MapPriceLevel.UP_2, "a shop just past one caret"))
			return true;

		if (!ExpectRemoteness(2999, OVT_MapPriceLevel.UP_2, "a shop just inside two carets"))
			return true;

		if (!ExpectRemoteness(3001, OVT_MapPriceLevel.UP_3, "a shop just past two carets"))
			return true;

		if (!ExpectRemoteness(12000, OVT_MapPriceLevel.UP_3, "a shop right across the map from a port"))
			return true;

		// The badge never reads as a discount: the remoteness term only ever raises a price.
		if (OVT_MapShopPriceBands.IsDown(OVT_MapShopPriceBands.GetRemotenessLevel(12000)))
		{
			SetResultFailure("A remote shop banded as cheaper; distance to port only ever raises prices");
			return true;
		}

		Print("Map price bands: scarcity mirrors the economy's arithmetic including its capacity clamp, every band boundary falls to the less extreme band, the neutral band draws no glyph and no caption, and a negative port distance hides the badge instead of banding as a discount");

		SetResultSuccess();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one scarcity percentage, naming the case in the failure message.
	//! \param[in] townStock Stock across the town.
	//! \param[in] townMaxStock The town's capacity.
	//! \param[in] expected The percentage the model specifies.
	//! \param[in] label Human description, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectPercent(int townStock, int townMaxStock, float expected, string label)
	{
		float actual = OVT_MapShopPriceBands.GetScarcityPercent(townStock, townMaxStock);

		if (OVT_TEST_LogicFixture.FloatEquals(actual, expected))
			return true;

		SetResultFailure("%1 produced a scarcity term of %2, expected %3", label, actual.ToString(), expected.ToString());
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one caret band.
	//! \param[in] scarcityPercent The term to band.
	//! \param[in] expected The band the table specifies.
	//! \param[in] label Human description, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectScarcityLevel(float scarcityPercent, OVT_MapPriceLevel expected, string label)
	{
		OVT_MapPriceLevel actual = OVT_MapShopPriceBands.GetScarcityLevel(scarcityPercent);

		if (actual == expected)
			return true;

		SetResultFailure(label + " (%1) banded as %2, expected %3", scarcityPercent.ToString(),
			typename.EnumToString(OVT_MapPriceLevel, actual),
			typename.EnumToString(OVT_MapPriceLevel, expected));

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one badge band.
	//! \param[in] distanceToPort Metres to the nearest port, negative for "unknown".
	//! \param[in] expected The band the table specifies.
	//! \param[in] label Human description, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectRemoteness(float distanceToPort, OVT_MapPriceLevel expected, string label)
	{
		OVT_MapPriceLevel actual = OVT_MapShopPriceBands.GetRemotenessLevel(distanceToPort);

		if (actual == expected)
			return true;

		SetResultFailure(label + " (%1 m) banded as %2, expected %3", distanceToPort.ToString(),
			typename.EnumToString(OVT_MapPriceLevel, actual),
			typename.EnumToString(OVT_MapPriceLevel, expected));

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts the imageset quad name a band asks for.
	//! \param[in] level The band.
	//! \param[in] expected The quad name authored in overthrow_priceicons.imageset.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectGlyph(OVT_MapPriceLevel level, string expected)
	{
		string actual = OVT_MapShopPriceBands.GetCaretIcon(level);

		if (actual == expected)
			return true;

		SetResultFailure("Band %1 asked for glyph '%2', expected '%3'",
			typename.EnumToString(OVT_MapPriceLevel, level), actual, expected);

		return false;
	}
}
