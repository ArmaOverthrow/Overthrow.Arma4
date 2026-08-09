//------------------------------------------------------------------------------------------------
//! RegisterFOB never mints a second record inside the 10 m wire-matching tolerance (BUG-129).
//!
//! Every position-keyed FOB RPC (RpcDo_RemoveFOB, SetPriorityFOB) matches records by "nearest
//! within 10 m", so two records that close are indistinguishable on the wire: they draw two
//! overlapping map icons and the loser of an undeploy's nearest-match removal survives as a
//! permanent orphan marker. The guard makes a re-registration at an occupied spot REUSE the
//! existing record (updating its owner) instead of inserting a duplicate - which is also the
//! recovery path for a player re-deploying where a lost FOB's record still lies.
//!
//! The case drives the real public entry point (RegisterFOB) with marker entities rather than
//! poking m_FOBs directly, because the guard under test lives at the top of that method. A
//! synthetic playerId of -1 resolves to an empty persistent id (GetPersistentIDFromPlayerID
//! returns "" for anything < 1), which the method tolerates on every path it takes here.
//!
//! House rule compliance: both marker entities are deleted and every record this case inserted is
//! removed from m_FOBs before any verdict is returned, so campaign state is restored even on
//! failure.
//!
//! PROVEN ABLE TO FAIL (2026-08-09): with the guard's tolerance mutated from `< 10` to `< 0` the
//! case fails with "second registration 5 m away minted a duplicate record (2 new records for one
//! spot)"; restored, it passes.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_CampaignSuite, timeoutS: 30)]
class OVT_TEST_Campaign_FOB_RegisterDuplicateGuard : SCR_AutotestCaseBase
{
	//! Where the markers stand. Arbitrary but away from the origin, so the case cannot pass by
	//! colliding with a zero-vector default somewhere.
	static const vector TEST_POS = "4000 0 4000";

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (!resistance)
		{
			SetResultFailure("OVT_Global.GetResistanceFaction() is null in a campaign-tier case");
			return true;
		}

		IEntity markerA = SpawnMarker(TEST_POS);
		IEntity markerB = SpawnMarker(TEST_POS + "5 0 0");
		if (!markerA || !markerB)
		{
			Cleanup(resistance, markerA, markerB);
			SetResultFailure("Could not spawn the marker entities the case registers as FOBs");
			return true;
		}

		int baseline = resistance.m_FOBs.Count();

		resistance.RegisterFOB(markerA, -1);
		int afterFirst = resistance.m_FOBs.Count();

		// 5 m away - inside the 10 m tolerance, so this must reuse the first record
		resistance.RegisterFOB(markerB, -1);
		int afterSecond = resistance.m_FOBs.Count();

		Cleanup(resistance, markerA, markerB);

		if (afterFirst != baseline + 1)
		{
			SetResultFailure("First registration at an empty spot inserted %1 record(s), expected exactly 1",
				(afterFirst - baseline).ToString());
			return true;
		}

		if (afterSecond != afterFirst)
		{
			SetResultFailure("Second registration 5 m away minted a duplicate record (%1 new records for one spot)",
				(afterSecond - baseline).ToString());
			return true;
		}

		PrintFormat("RegisterFOB duplicate guard held: 2 registrations 5 m apart produced 1 record (baseline %1)",
			baseline.ToString());
		SetResultSuccess();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns a bare positional entity for RegisterFOB to read its origin from.
	//! \param[in] pos Where to stand it.
	//! \return The entity, or null when the spawn failed.
	protected IEntity SpawnMarker(vector pos)
	{
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = pos;
		return GetGame().SpawnEntity(GenericEntity, GetGame().GetWorld(), params);
	}

	//------------------------------------------------------------------------------------------------
	//! Restores campaign state: removes every record this case put near TEST_POS and both markers.
	//! \param[in] resistance The manager whose m_FOBs the case mutated.
	//! \param[in] markerA First marker entity, may be null.
	//! \param[in] markerB Second marker entity, may be null.
	protected void Cleanup(OVT_ResistanceFactionManager resistance, IEntity markerA, IEntity markerB)
	{
		for (int i = resistance.m_FOBs.Count() - 1; i >= 0; i--)
		{
			OVT_FOBData fob = resistance.m_FOBs[i];
			if (fob && vector.Distance(fob.location, TEST_POS) < 20)
				resistance.m_FOBs.Remove(i);
		}

		if (markerA)
			SCR_EntityHelper.DeleteEntityAndChildren(markerA);
		if (markerB)
			SCR_EntityHelper.DeleteEntityAndChildren(markerB);
	}
}
