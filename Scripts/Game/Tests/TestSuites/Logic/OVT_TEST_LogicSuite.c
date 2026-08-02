//------------------------------------------------------------------------------------------------
//! TIER A - pure campaign logic. WORLD-FREE, and the cheapest suite in the tree (~8 s).
//!
//! ===========================================================================================
//! TIER RULE, NON-NEGOTIABLE:
//!   NOTHING IN THIS SUITE MAY TOUCH A MANAGER, THE GAME MODE, OR THE WORLD.
//! ===========================================================================================
//!
//! The rule is enforced three ways, which is why it is worth stating once here rather than
//! repeating it per case:
//!
//!  1. BY THE ENGINE. This suite overrides GetWorldFile() to return ResourceName.Empty, which
//!     makes the framework's Setup_OpenWorld skip the scenario change entirely (findings.md 1.2):
//!     no world is ever loaded, the harness starts ONCE instead of twice, and a run costs ~8 s
//!     instead of ~17 s. With no world there is no Overthrow game mode and no manager, so a stray
//!     manager lookup here is a null dereference, never a silent pass.
//!  2. BY A REVIEWER GREP over this directory (Definition of Done Q5), which looks for the name of
//!     Overthrow's static manager accessor and for the engine's game-mode getter. That grep does
//!     not distinguish code from comments, so neither identifier may be written ANYWHERE under
//!     TestSuites/Logic/ - not even in prose. Phase 4 tripped exactly this trap by quoting its own
//!     rule verbatim (findings.md, Phase 4 observation 6); this header is worded around it
//!     deliberately.
//!  3. BY CONSTRUCTION. Every subject is built with `new` and fed values by hand. No case reads
//!     state it did not itself write.
//!
//! WHAT BELONGS HERE
//!   Deterministic maths and branching on hand-built objects: town record maths, town modifier
//!   recalculation, pure job conditions, skill effects, player levelling. These cases cost nothing
//!   and cannot flake, so this is the tier to prefer whenever the logic allows it.
//!
//! WHAT DOES NOT
//!   - Anything needing live managers (Tier B, OVT_TEST_InitSuite) or a started campaign
//!     (Tier C OVT_TEST_CampaignSuite / Tier D OVT_TEST_PersistenceSuite).
//!   - Anything RNG-gated. An RNG branch is not deterministic, and this feature's quality bar
//!     bans the framework's per-case retry attribute outright: retrying a non-deterministic case
//!     until it passes converts "this is flaky" into "this is slow". Where a subject has an
//!     RNG-gated branch, the deterministic branches are covered and the RNG branch is NAMED and
//!     SKIPPED in a comment saying so (see OVT_TEST_Logic_Town.c, support modifier system).
//!
//! HOUSE RULES FOR THIS TIER
//!  - `new` does NOT apply [Attribute()] defvalues. Attribute defaults are applied by the
//!    container/config loader; a hand-built object starts with every field zeroed or empty. Every
//!    field a case depends on must therefore be set EXPLICITLY, including fields whose declared
//!    default is exactly what the case wants. This bites hardest where the "unset" sentinel is -1
//!    (see the job conditions) or where a multiplier defaults to 1 (a hand-built object gets 0,
//!    which silently zeroes the whole expression).
//!  - Cases run alphabetically by class name (findings.md, Phase 3) and MUST be independent.
//!  - Floats are compared with an epsilon, never with ==.
//!  - Suspected bugs are PINNED, not fixed and not hidden (Decision 10). A case that pins one says
//!    so in its name and in its comment, and the finding is logged in findings.md under
//!    "Bugs found (log only)".
//!  - [BaseContainerProps()] is MANDATORY on a concrete suite class, or a group config silently
//!    instantiates nothing (`Unknown class`, empty <testsuites>, exit 2 - findings.md 1.10).
//!    Phase 6 puts this suite in both the Fast and the All group.
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class OVT_TEST_LogicSuite : OVT_TEST_SuiteBase
{
	//------------------------------------------------------------------------------------------------
	//! No world at all - see the tier rule above.
	//!
	//! This is the ONE place a suite is allowed to override world selection. OVT_TEST_SuiteBase
	//! deliberately does not touch GetWorldFile() so that world choice stays in
	//! OVT_AutotestFramework.c for every world-bearing suite; a suite that wants NO world is a
	//! per-suite decision and is made here, explicitly.
	//! \return An empty resource name, which makes the framework skip the scenario change.
	override ResourceName GetWorldFile()
	{
		return ResourceName.Empty;
	}
}

//------------------------------------------------------------------------------------------------
//! Hand-built subjects shared by the Tier A cases.
//!
//! Exists so that a case body reads as "arrange one value, assert one claim" instead of ten lines
//! of field assignment, and so that the "`new` applies no Attribute defvalues" rule is honoured in
//! ONE place rather than being re-learned per case. Every factory below sets every field it cares
//! about explicitly, even when the declared attribute default matches.
//------------------------------------------------------------------------------------------------
class OVT_TEST_LogicFixture
{
	//! Tolerance for every float comparison in this tier.
	static const float EPSILON = 0.0001;

	//------------------------------------------------------------------------------------------------
	//! Builds a town record with no manager, no controller and no world behind it.
	//! \param[in] population Civilian population.
	//! \param[in] support Number of supporters.
	//! \param[in] stability Stability percentage.
	//! \return A fully-populated town record centred on the world origin.
	static OVT_TownData MakeTown(int population, int support, int stability = 100)
	{
		OVT_TownData town = new OVT_TownData();
		town.location = "0 0 0";
		town.population = population;
		town.targetPopulation = population;
		town.support = support;
		town.stability = stability;
		town.faction = 0;
		town.size = OVT_TownSize.TOWN;
		town.gunDealerPosition = vector.Zero;
		town.areaHeat = 0;
		return town;
	}

	//------------------------------------------------------------------------------------------------
	//! Builds a three-entry modifier config, so a modifier record's id can be 0, 1 or 2.
	//! Only baseEffect is read by Recalculate(); the rest are set to inert values because a
	//! hand-built config would otherwise carry zeroes the attribute defaults would never produce.
	//! \param[in] effect0 baseEffect of modifier id 0.
	//! \param[in] effect1 baseEffect of modifier id 1.
	//! \param[in] effect2 baseEffect of modifier id 2.
	//! \return The config, ready to assign to a modifier system's m_Config.
	static OVT_ModifiersConfig MakeModifiersConfig(float effect0, float effect1, float effect2)
	{
		OVT_ModifiersConfig config = new OVT_ModifiersConfig();
		config.m_aModifiers = new array<ref OVT_ModifierConfig>();

		config.m_aModifiers.Insert(MakeModifierConfig("effect0", effect0));
		config.m_aModifiers.Insert(MakeModifierConfig("effect1", effect1));
		config.m_aModifiers.Insert(MakeModifierConfig("effect2", effect2));

		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! Builds a single modifier config entry.
	//! \param[in] name Modifier name (unused by Recalculate, set so the object is not half-built).
	//! \param[in] baseEffect The value Recalculate() sums.
	//! \return The config entry.
	static OVT_ModifierConfig MakeModifierConfig(string name, float baseEffect)
	{
		OVT_ModifierConfig entry = new OVT_ModifierConfig();
		entry.name = name;
		entry.title = name;
		entry.baseEffect = baseEffect;
		entry.timeout = 0;
		entry.stackLimit = 1;
		entry.flags = OVT_ModifierFlags.ACTIVE;
		entry.handler = null;
		return entry;
	}

	//------------------------------------------------------------------------------------------------
	//! Builds a modifier record list from up to three modifier ids. A negative id is skipped, so
	//! MakeModifierList() with no arguments returns an empty list (the identity case).
	//! \param[in] idA First modifier id, or -1 for none.
	//! \param[in] idB Second modifier id, or -1 for none.
	//! \param[in] idC Third modifier id, or -1 for none.
	//! \return The list, owned by the caller.
	static array<ref OVT_TownModifierData> MakeModifierList(int idA = -1, int idB = -1, int idC = -1)
	{
		array<ref OVT_TownModifierData> list = new array<ref OVT_TownModifierData>();

		if (idA >= 0)
			list.Insert(MakeModifierData(idA));

		if (idB >= 0)
			list.Insert(MakeModifierData(idB));

		if (idC >= 0)
			list.Insert(MakeModifierData(idC));

		return list;
	}

	//------------------------------------------------------------------------------------------------
	//! Builds one modifier record pointing at a config index.
	//! \param[in] id Index into the config's m_aModifiers.
	//! \return The record.
	static OVT_TownModifierData MakeModifierData(int id)
	{
		OVT_TownModifierData data = new OVT_TownModifierData();
		data.id = id;
		data.timer = 0;
		return data;
	}

	//------------------------------------------------------------------------------------------------
	//! Epsilon float comparison. Never compare floats with == in this tier: 1 - 0.4 is not 0.6 in
	//! binary floating point, and every skill effect computes exactly that shape of expression.
	//! \param[in] actual Observed value.
	//! \param[in] expected Expected value.
	//! \return True when the two are equal within EPSILON.
	static bool FloatEquals(float actual, float expected)
	{
		return Math.AbsFloat(actual - expected) <= EPSILON;
	}
}
