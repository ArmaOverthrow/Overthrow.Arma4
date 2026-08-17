//------------------------------------------------------------------------------------------------
//! Deployment virtualization keys - PURE, SYSTEM-FREE string arithmetic.
//!
//! ===========================================================================================
//! HARD RULE: NOTHING IN THIS FILE MAY TOUCH A SYSTEM, THE GAME MODE OR ANY LIVE STATE.
//! ===========================================================================================
//!
//! A registered AI group is tagged with an owner system and an owner key, and that key is the ONLY
//! thing a consumer must be able to re-derive after a load in order to reclaim what it registered.
//! For deployments the key is built out of two things that do not move: the config name, and the
//! rounded position of the marker the deployment lives on.
//!
//!     deployment key   <sanitised config name>@<round(x)>_<round(z)>[#<ordinal>]
//!     owner key        <deployment key>#<sanitised module tag>
//!
//! Two characters are structural - '@' splits the name from the coordinates and '#' splits a base
//! key from its collision ordinal and a deployment key from its module tag - so both are stripped
//! out of every free-text part before it is composed in. Whitespace goes the same way: an authored
//! name is a human label and may gain or lose a space between releases, which would silently orphan
//! every group registered under the old spelling.
//!
//! Rounding to whole metres is deliberate: a key derived from a raw float would differ in its last
//! digit between the save that wrote it and the run that re-derives it, and the reclaim would find
//! nothing. One metre is far below the spacing two deployments OF ONE CONFIG are ever created at
//! (the creation path enforces a 250 m same-name dedup), so the rounding cannot merge two
//! deployments that share a key's name part - and where it somehow does, the ordinal separates
//! them. ⚠ Two deployments of DIFFERENT configs may share a position outright since the base-defense
//! migration removed the blanket 100 m separation; their keys differ in the name part, which is why
//! that change needed nothing here.
//!
//! Everything here is a pure function of its arguments, which is exactly what lets the cheapest
//! tier in the tree assert it.
//------------------------------------------------------------------------------------------------
class OVT_DeploymentVirtualKey
{
	//! What an empty or fully-stripped name becomes, so a key is never a bare "@0_0".
	static const string UNNAMED = "unnamed";

	//! Splits the sanitised name from the rounded coordinates.
	static const string POSITION_MARK = "@";

	//! Splits a base key from its ordinal, and a deployment key from its module tag.
	static const string PART_MARK = "#";

	//! Splits the two rounded coordinates.
	static const string AXIS_MARK = "_";

	//! Prefix of the positional fallback tag, "m" + index.
	static const string INDEX_TAG_PREFIX = "m";

	//! Every code at or below this one is a space or a control character, and is dropped.
	static const int ASCII_SPACE = 32;

	//! '#'
	static const int ASCII_HASH = 35;

	//! '@'
	static const int ASCII_AT = 64;

	//------------------------------------------------------------------------------------------------
	//! Builds the base deployment key: a sanitised name, then the position rounded to whole metres.
	//!
	//! Y is deliberately absent. A key is a label, not a location, and the height of a marker is the
	//! one component of its position that a surface snap or a terrain edit can move under it.
	//! \param[in] configName The deployment config's authored name; may be empty.
	//! \param[in] x Position on the X axis, in metres.
	//! \param[in] z Position on the Z axis, in metres.
	//! \return The base key, e.g. "TowerGarrison@4821_7093". Never empty.
	static string DeriveKey(string configName, float x, float z)
	{
		int roundedX = Math.Round(x);
		int roundedZ = Math.Round(z);

		return Sanitise(configName) + POSITION_MARK + roundedX.ToString() + AXIS_MARK + roundedZ.ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! Strips everything out of a free-text part that would make a composed key ambiguous.
	//!
	//! '@' and '#' are structural. Whitespace and control codes are dropped because an authored label
	//! is edited by people: "Tower Garrison" and "Tower  Garrison" must not be two different owners.
	//! A part that is empty, or that consists entirely of stripped characters, becomes UNNAMED rather
	//! than nothing, so a key always has all three of its pieces.
	//! \param[in] name The free-text part.
	//! \return The sanitised part; never empty.
	static string Sanitise(string name)
	{
		string clean;

		int length = name.Length();
		for (int i = 0; i < length; i++)
		{
			int code = name.ToAscii(i);

			if (code <= ASCII_SPACE)
				continue;

			if (code == ASCII_HASH || code == ASCII_AT)
				continue;

			clean = clean + name.Get(i);
		}

		if (clean.IsEmpty())
			return UNNAMED;

		return clean;
	}

	//------------------------------------------------------------------------------------------------
	//! Separates keys that would otherwise collide - same config, same rounded spot.
	//!
	//! Ordinal 0 is the first holder and keeps the base key unchanged, so the ordinary case carries no
	//! suffix at all and a key stays readable in a log. Anything above it appends "#<ordinal>"; the
	//! caller counts from 2, so "#2" reads as "the second one here".
	//! \param[in] baseKey A key from DeriveKey().
	//! \param[in] ordinal 0 for the first holder, 2 and up for later ones.
	//! \return baseKey for ordinal 0 or below, baseKey + "#" + ordinal otherwise.
	static string Disambiguate(string baseKey, int ordinal)
	{
		if (ordinal <= 0)
			return baseKey;

		return baseKey + PART_MARK + ordinal.ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! The tag that identifies ONE spawning module within a deployment.
	//!
	//! The authored name wins when there is one, because it survives a module being re-ordered in the
	//! config. Where there is none - both shipped vehicle configs leave it blank - the position in the
	//! spawning list is the fallback, which is stable as long as the config is not re-ordered. That
	//! is the honest trade: an unnamed module's groups are reclaimed by position, so re-ordering an
	//! unnamed module orphans its groups exactly once.
	//! \param[in] moduleName The module's authored name; may be empty.
	//! \param[in] spawningIndex The module's index within its deployment's spawning modules.
	//! \return The authored name when it has one, otherwise "m" + index.
	static string ModuleTag(string moduleName, int spawningIndex)
	{
		if (!moduleName.IsEmpty())
			return moduleName;

		return INDEX_TAG_PREFIX + spawningIndex.ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! Composes the owner key one spawning module registers its groups under.
	//!
	//! The module tag is SANITISED here rather than by ModuleTag(), so the tag stays readable for a
	//! log line while the composed key stays unambiguous. Without it, ("a#b", "c") and ("a", "b#c")
	//! would both compose to "a#b#c" and two different modules would reclaim each other's groups.
	//! The deployment key is passed through untouched: its own '@' and its ordinal suffix are
	//! structure, not free text.
	//! \param[in] deploymentKey A key from DeriveKey(), optionally through Disambiguate().
	//! \param[in] moduleTag A tag from ModuleTag().
	//! \return The owner key, "<deploymentKey>#<sanitised tag>".
	static string OwnerKey(string deploymentKey, string moduleTag)
	{
		return deploymentKey + PART_MARK + Sanitise(moduleTag);
	}

	//------------------------------------------------------------------------------------------------
	//! How many more groups a module has to register to reach the count it wants.
	//!
	//! Never negative: holding MORE than the wanted count is a normal state - a count re-rolled lower
	//! on a restore, or a reinforcement that has already been paid for - and it means "register
	//! nothing", never "unregister the difference". Unregistering is a decision with its own path.
	//! \param[in] wanted The count the module wants to hold.
	//! \param[in] held The count it holds now.
	//! \return wanted - held, floored at 0.
	static int MissingCount(int wanted, int held)
	{
		if (wanted <= held)
			return 0;

		if (wanted <= 0)
			return 0;

		if (held < 0)
			return wanted;

		return wanted - held;
	}
}
