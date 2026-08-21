//------------------------------------------------------------------------------------------------
//! "THE THING WE BUILT IS STANDING." One asset record's up flag, as an authored conjunct.
//!
//! The port of the `fobUp` term shared by both halves of the counter-attack gate
//! (OVT_ObjectivePhaseRules.TownPhase3Gate / BasePhase3Gate). The occupying faction does not launch
//! its battle out of thin air: the forward base is the staging ground the whole middle phase exists to
//! build, and a battle mounted without one is the dice-roll attack this feature replaced.
//!
//! ⚠ IT IS KEYED, NOT FOB-SPECIFIC (G10). The key is authored, so the checkpoint asset that follows
//! this feature reuses this condition by authoring a different string rather than by shipping a second
//! class. The shipped key is the one the director declares as ASSET_FOB.
//!
//! ⚠ Evaluate() IS SIDE-EFFECT FREE: it reads one boolean off the objective's own record.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_AssetUpObjectiveCondition : OVT_BaseObjectiveConditionModule
{
	[Attribute(defvalue: "fob", desc: "Which standing asset this conjunct is about. 'fob' is the forward operating base. A later asset adds a key rather than a condition class")]
	string m_sAssetKey;

	[Attribute(defvalue: "0", desc: "Invert the test: 1 makes this conjunct true only while the asset is NOT standing. Authored for a doctrine that has to finish something before it builds; the shipped plans author 0")]
	bool m_bInverted;

	//------------------------------------------------------------------------------------------------
	//! \return True when the asset is standing, or the opposite when inverted.
	override bool Evaluate()
	{
		OVT_ObjectiveInstance objective = GetObjective();
		if (!objective || !objective.IsLive())
			return false;

		bool up = false;

		OVT_ObjectiveAssetRecord asset = objective.GetAsset(m_sAssetKey);
		if (asset)
			up = asset.up;

		if (m_bInverted)
			return !up;

		return up;
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty prefix: the condition READS a record the asset's own module owns, and writes
	//!         nothing.
	override string GetBagPrefix()
	{
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! 🔴 EVERY ATTRIBUTE, BY HAND, INCLUDING THE PARENT'S. Not chained, silently drops what it forgets.
	//!
	//! What a dropped line costs here: drop m_sAssetKey and every clone asks about the empty key, which
	//! no asset is ever registered under - the conjunct is false forever and the phase can never advance;
	//! drop m_bInverted and an inverted authoring silently becomes a plain one, which passes exactly when
	//! the author meant it to fail.
	//! \return An independent copy.
	override OVT_BaseObjectiveModule CloneModule()
	{
		OVT_AssetUpObjectiveCondition clone = new OVT_AssetUpObjectiveCondition();

		clone.m_sModuleName = m_sModuleName;
		clone.m_sAssetKey = m_sAssetKey;
		clone.m_bInverted = m_bInverted;

		return clone;
	}
}
