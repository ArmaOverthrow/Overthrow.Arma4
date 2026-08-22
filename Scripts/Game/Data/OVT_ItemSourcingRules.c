//------------------------------------------------------------------------------------------------
//! One line of a required-item manifest: a resource, how many are needed, and its unit price.
//------------------------------------------------------------------------------------------------
class OVT_ItemSourcingLine : Managed
{
	string m_sResource;
	int m_iNeeded;
	int m_iUnitPrice;
}

//------------------------------------------------------------------------------------------------
//! Splitting a required-item manifest into "covered by stock" and "charged" - shared by the High
//! Command purchase and the recruitment-tent discount (implementation.md §3.2), which is why it is
//! its own pure class rather than living on either caller.
//------------------------------------------------------------------------------------------------
class OVT_ItemSourcingRules
{
	//------------------------------------------------------------------------------------------------
	//! Splits a manifest into covered and charged quantities, given how much stock each line has.
	//!
	//! Per line: covered = min(needed, available), charged = needed - covered. An unknown or zero
	//! unit price contributes nothing to the charged subtotal but its units still count toward
	//! chargedUnits, so a caller can still refuse an unpriceable line.
	//! \param[in] manifest One entry per required resource.
	//! \param[in] availablePerLine Stock available for each manifest line, same index.
	//! \param[out] coveredUnits Total units covered by stock.
	//! \param[out] coveredValue Total value covered by stock.
	//! \param[out] chargedUnits Total units not covered by stock.
	//! \return The charged subtotal - the amount actually owed before any fee multiplier.
	static int SplitCoverage(notnull array<ref OVT_ItemSourcingLine> manifest, notnull array<int> availablePerLine, out int coveredUnits, out int coveredValue, out int chargedUnits)
	{
		coveredUnits = 0;
		coveredValue = 0;
		chargedUnits = 0;
		int chargedSubtotal = 0;

		for (int i = 0; i < manifest.Count(); i++)
		{
			OVT_ItemSourcingLine line = manifest[i];
			if (!line)
				continue;

			int available = 0;
			if (i < availablePerLine.Count())
				available = availablePerLine[i];

			int covered = Math.Min(line.m_iNeeded, available);
			if (covered < 0)
				covered = 0;

			int charged = line.m_iNeeded - covered;
			if (charged < 0)
				charged = 0;

			coveredUnits += covered;
			coveredValue += covered * line.m_iUnitPrice;
			chargedUnits += charged;
			chargedSubtotal += charged * line.m_iUnitPrice;
		}

		return chargedSubtotal;
	}
}
