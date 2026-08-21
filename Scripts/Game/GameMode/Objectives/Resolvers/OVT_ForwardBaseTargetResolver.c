//------------------------------------------------------------------------------------------------
//! A STANDING ASSET'S OWN POSITION - the forward operating base, once it is up.
//!
//! Where the forward-base garrison goes: not at the objective and not at the supplying base, but at the
//! thing the garrison is FOR. Answers nothing at all while the asset is not standing, which is the
//! refusal the hard-coded garrison sender made by returning early when the base was not up.
//!
//! ⚠ IT IS KEYED, NOT FOB-SPECIFIC (G10). The key is authored, so the checkpoint asset that follows
//! this feature reuses this resolver by authoring a different string rather than by shipping a second
//! class. The shipped key is the one the director declares as ASSET_FOB.
//!
//! ⚠ IT READS THE ASSET REGISTRY, WHICH IS THE SAME OBJECT THE RAISE WRITES. IsAssetUp()/
//! GetAssetPosition() resolve the record the raise module registered, so this resolver and the thing it
//! points at cannot drift; a mistyped key makes the asset vanish entirely rather than go subtly stale.
//!
//! ⚠ ITS CONSUMER ARRIVES WITH THE FORWARD BASE (build phase 5). It ships here with the rest of the
//! resolver family so the seam is complete and the phase that authors it adds no new script.
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class OVT_ForwardBaseTargetResolver : OVT_ObjectiveTargetResolver
{
	[Attribute(defvalue: "fob", desc: "Which standing asset to send at. 'fob' is the forward operating base; a later asset adds a key rather than a resolver")]
	string m_sAssetKey;

	//------------------------------------------------------------------------------------------------
	//! \param[in] objective The objective the operation belongs to.
	//! \param[in] factionIndex The faction running the operation. Unused - an asset belongs to the
	//!            objective, and the objective belongs to one faction.
	//! \param[out] positions Receives the asset's position, or nothing while it is not standing.
	//! \return True when the asset is up.
	override bool Resolve(notnull OVT_ObjectiveInstance objective, int factionIndex, notnull array<vector> positions)
	{
		positions.Clear();

		OVT_ObjectiveAssetRecord asset = objective.GetAsset(m_sAssetKey);
		if (!asset || !asset.up)
			return false;

		positions.Insert(asset.position);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The resolver's name, for the registry's validator and for debug output.
	override string GetResolverName()
	{
		return "ForwardBase";
	}
}
