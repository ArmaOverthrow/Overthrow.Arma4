//! The map's faction colour scheme, expressed as CAMPAIGN ROLES rather than as factions.
//!
//! WHY ROLES AND NOT FACTION COLOURS. Every faction-coloured thing on the map used to resolve through
//! Faction.GetFactionColor(), which hands the map's readability to whichever faction the campaign
//! happens to cast as the occupier. That works when it casts USSR - OPFOR's red reads as "the enemy" -
//! and fails when it casts US, because BLUFOR is a bright cyan-blue (0.016 0.552 0.905) that fights the
//! topographic map background and reads as friendly. The player's map should say OCCUPIER, not USSR.
//!
//! THE DEFAULTS REPRODUCE THE LOOK THAT ALREADY WORKED, deliberately. Occupying is OPFOR's own red and
//! resistance is INDFOR/FIA's own green, copied from the vanilla faction configs rather than invented,
//! so a USSR campaign renders exactly as it did before this class existed and ONLY a non-red occupier
//! changes. That is what makes this safe to land mid-play-test: there is no campaign in which the map
//! gets a new look it did not already have.
//!
//! ONE PALETTE PER LAYER, NOT ONE PER MAP. Every canvas layer and every location type carries its own
//! optional palette, so a fill that has to sit UNDER lines can be shifted without moving the lines, and
//! a marker can be darkened without darkening the region beneath it. A null palette means "use the
//! shared default" (GetDefault), so the conf files stay empty until somebody actually wants to curate
//! one surface - and the shared default remains the single place that decides what "occupier red" is.
//!
//! ALPHA IS NEVER TAKEN FROM THESE COLOURS. GetArgb packs at the caller's alpha, because alpha is how
//! the map carries meaning that hue does not: contested versus held territory, in-effect versus
//! suppressed influence, restricted versus friendly-restricted rings. A palette colour's own alpha is
//! only ever used when a caller asks for a Color rather than a packed int.
[BaseContainerProps()]
class OVT_MapFactionPalette
{
	//-----------------------------------------------------------------------
	// ATTRIBUTES
	//-----------------------------------------------------------------------
	//
	// ⚠️ The defvalue strings below MUST match the literals in EnsureDefaults(). A config-constructed
	// palette gets the defvalue; a script-constructed one (GetDefault) gets the literal. They are two
	// paths to the same colour and silently disagreeing would mean the shared default and an "untouched"
	// per-layer palette render differently.

	[Attribute(defvalue: "0.855 0.031 0.028 1", UIWidgets.ColorPicker, desc: "Colour of the OCCUPYING faction, whichever faction the campaign cast as occupier. Default is OPFOR's own red.")]
	protected ref Color m_OccupyingColor;

	[Attribute(defvalue: "0 0.44 0.078 1", UIWidgets.ColorPicker, desc: "Colour of the RESISTANCE (player) faction. Default is INDFOR/FIA's own green.")]
	protected ref Color m_ResistanceColor;

	[Attribute(defvalue: "0.016 0.552 0.905 1", UIWidgets.ColorPicker, desc: "Colour of the SUPPORTING faction. Nothing on the map is owned by it today, so this is near-unreachable - it exists so a supporting-faction site could never silently render as the occupier.")]
	protected ref Color m_SupportingColor;

	[Attribute(defvalue: "1 1 1 1", UIWidgets.ColorPicker, desc: "Colour for a faction that is none of the three campaign roles, or that cannot be resolved at all. White: a pale wash, ring or line still reads as itself, whereas any hue would read as a faction that may not be there.")]
	protected ref Color m_UnknownColor;

	[Attribute(defvalue: "false", desc: "Ignore the four colours above and use each faction's OWN engine colour, i.e. the behaviour before roles existed. A per-layer A/B switch for play-testing a colour decision against the old look; leave off.")]
	protected bool m_bUseEngineFactionColors;

	//-----------------------------------------------------------------------
	// SHARED DEFAULT
	//-----------------------------------------------------------------------

	//! The palette every layer and type falls back to when its own attribute is unset, which is all of
	//! them until somebody curates one. Constructed once from the literals in EnsureDefaults.
	protected static ref OVT_MapFactionPalette s_Default;

	//! The shared default palette. Never null.
	static OVT_MapFactionPalette GetDefault()
	{
		if (!s_Default)
		{
			s_Default = new OVT_MapFactionPalette();
		}

		return s_Default;
	}

	//! Fill any colour the config did not supply. A config-constructed palette arrives with all four set
	//! from their defvalues and this is a no-op; a script-constructed one (GetDefault, or a conf block
	//! that lists only the colour it wanted to shift) is where it does the work.
	protected void EnsureDefaults()
	{
		if (!m_OccupyingColor)
			m_OccupyingColor = new Color(0.855, 0.031, 0.028, 1);

		if (!m_ResistanceColor)
			m_ResistanceColor = new Color(0, 0.44, 0.078, 1);

		if (!m_SupportingColor)
			m_SupportingColor = new Color(0.016, 0.552, 0.905, 1);

		if (!m_UnknownColor)
			m_UnknownColor = new Color(1, 1, 1, 1);
	}

	//-----------------------------------------------------------------------
	// ROLE CLASSIFICATION
	//-----------------------------------------------------------------------

	//! Which campaign role, if any, a faction index belongs to.
	//!
	//! CLIENT-SAFE. Both keys this reads - m_sOccupyingFaction and m_sPlayerFaction - are replicated to
	//! every client in OVT_OccupyingFactionManager's JIP stream, which also recomputes both cached
	//! indices on arrival, so a client classifies exactly as the server does. It is worth stating
	//! because the config component itself carries no [RplProp] at all and the field looks server-only.
	//!
	//! RESISTANCE IS TESTED FIRST on purpose: if a misconfigured campaign ever cast the player's own
	//! faction as the occupier, the player's sites should still read as the player's.
	//! \param[in] factionIndex Faction index, or any negative value for "no faction".
	//! \param[out] role The campaign role, valid only when this returns true.
	//! \return true when the index is one of the three campaign roles.
	static bool TryGetRole(int factionIndex, out OVT_FactionType role)
	{
		if (factionIndex < 0)
			return false;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return false;

		if (factionIndex == config.GetPlayerFactionIndex())
		{
			role = OVT_FactionType.RESISTANCE_FACTION;
			return true;
		}

		if (factionIndex == config.GetOccupyingFactionIndex())
		{
			role = OVT_FactionType.OCCUPYING_FACTION;
			return true;
		}

		if (factionIndex == config.GetSupportingFactionIndex())
		{
			role = OVT_FactionType.SUPPORTING_FACTION;
			return true;
		}

		return false;
	}

	//-----------------------------------------------------------------------
	// COLOUR RESOLUTION
	//-----------------------------------------------------------------------

	//! This palette's colour for a campaign role. Never null.
	//! \param[in] role The campaign role.
	//! \return The configured colour, or the faction's own engine colour when m_bUseEngineFactionColors is set.
	Color GetColorForRole(OVT_FactionType role)
	{
		EnsureDefaults();

		if (m_bUseEngineFactionColors)
		{
			Color engineColour = GetEngineColorForRole(role);
			if (engineColour)
				return engineColour;
		}

		switch (role)
		{
			case OVT_FactionType.OCCUPYING_FACTION:
				return m_OccupyingColor;

			case OVT_FactionType.RESISTANCE_FACTION:
				return m_ResistanceColor;

			case OVT_FactionType.SUPPORTING_FACTION:
				return m_SupportingColor;
		}

		return m_UnknownColor;
	}

	//! This palette's colour for a faction index, by way of its campaign role.
	//!
	//! IT RETURNS NULL FOR AN UNROLED FACTION RATHER THAN THE UNKNOWN COLOUR, keeping the contract the
	//! marker overrides were written against: they do not agree on what unknown looks like (the town
	//! marker falls back to black, the base and radio-tower markers to white), so the decision stays
	//! theirs. The canvas path wants a colour unconditionally and uses GetArgb, which applies
	//! m_UnknownColor for them.
	//! \param[in] factionIndex Faction index, or any negative value for "no faction".
	//! \return The role colour, or null when the index belongs to no campaign role.
	Color GetColorForFactionIndex(int factionIndex)
	{
		if (m_bUseEngineFactionColors)
			return OVT_MapLocationType.GetFactionColorByIndex(factionIndex);

		OVT_FactionType role;
		if (!TryGetRole(factionIndex, role))
			return null;

		return GetColorForRole(role);
	}

	//! Packed ARGB for a faction index at the caller's alpha - the canvas layers' one entry point.
	//!
	//! THE ALPHA IS A PARAMETER RATHER THAN THE COLOUR'S OWN, because alpha is load-bearing on this map:
	//! contested versus held territory, in-effect versus suppressed influence, restricted versus
	//! friendly-restricted rings. Packing the palette colour's alpha would flatten every one of those
	//! distinctions.
	//! \param[in] factionIndex Faction index, or any negative value for "no faction".
	//! \param[in] alpha Alpha to pack, 0-255, clamped.
	//! \return Packed ARGB, falling back to m_UnknownColor when the index belongs to no campaign role.
	int GetArgb(int factionIndex, int alpha)
	{
		Color colour = GetColorForFactionIndex(factionIndex);
		if (!colour)
		{
			EnsureDefaults();
			colour = m_UnknownColor;
		}

		return PackColor(colour, alpha);
	}

	//! Packed ARGB for a campaign role at the caller's alpha.
	//! \param[in] role The campaign role.
	//! \param[in] alpha Alpha to pack, 0-255, clamped.
	//! \return Packed ARGB.
	int GetArgbForRole(OVT_FactionType role, int alpha)
	{
		return PackColor(GetColorForRole(role), alpha);
	}

	//! Recompose a Color's RGB against a caller-supplied alpha. Color.PackToInt would bake the colour's
	//! own alpha instead, which is exactly what the callers must not get.
	//! \param[in] colour Source colour. Null packs opaque white.
	//! \param[in] alpha Alpha to pack, 0-255, clamped.
	//! \return Packed ARGB.
	static int PackColor(Color colour, int alpha)
	{
		int red = 255;
		int green = 255;
		int blue = 255;

		if (colour)
		{
			red = Math.ClampInt(Math.Round(colour.R() * 255), 0, 255);
			green = Math.ClampInt(Math.Round(colour.G() * 255), 0, 255);
			blue = Math.ClampInt(Math.Round(colour.B() * 255), 0, 255);
		}

		return ARGB(Math.ClampInt(alpha, 0, 255), red, green, blue);
	}

	//! The faction's OWN engine colour for a campaign role, used only by m_bUseEngineFactionColors.
	//! \param[in] role The campaign role.
	//! \return The engine faction colour, or null when the campaign config or faction cannot be resolved.
	protected Color GetEngineColorForRole(OVT_FactionType role)
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return null;

		Faction faction;

		switch (role)
		{
			case OVT_FactionType.OCCUPYING_FACTION:
			{
				faction = config.GetOccupyingFactionData();
				break;
			}

			case OVT_FactionType.RESISTANCE_FACTION:
			{
				faction = config.GetPlayerFactionData();
				break;
			}

			case OVT_FactionType.SUPPORTING_FACTION:
			{
				faction = config.GetSupportingFactionData();
				break;
			}
		}

		if (!faction)
			return null;

		return faction.GetFactionColor();
	}
}
