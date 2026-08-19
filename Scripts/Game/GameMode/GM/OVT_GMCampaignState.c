//------------------------------------------------------------------------------------------------
//! The CLIENT-SIDE store of everything a Game Master's snapshot told this machine about the campaign.
//!
//! It is a plain Managed object with no replication of its own: the wire is
//! OVT_GMRequestComponent's owner-targeted RPC fan, and this is where a committed snapshot lands.
//! Siblings (the Overthrow panel, HUD icons, waypoint visualisation, the GM map layers) read THIS
//! and never touch an RPC - OVT_ControllerComponent<OVT_GMRequestComponent>.Get().GetState().
//!
//! WHY THE COUNTDOWNS ARE NOT FIELDS BUT READERS. The server sends "seconds remaining as of now"
//! once per poll; between polls the client ticks the value down itself against the world clock, so a
//! GM sees a smooth countdown at ~0 additional bytes per second. That is the pattern
//! OVT_RadioTowerData.GetDisabledRemaining() already proves in this codebase
//! (OVT_OccupyingFactionManager.c:101-111), copied rather than reinvented. The arrival stamp is what
//! makes it work, so it is written by the commit path and nowhere else.
//!
//! THE PER-ENTITY RECORD ARRAYS. Four of them, one per record class in OVT_GMRecords.c, filled by
//! the fan between CampaignSchedule and SnapshotEnd. They are never null - a consumer that asks
//! before the first snapshot lands gets an empty array rather than a null check it will forget to
//! write - and they are replaced WHOLESALE on commit, so no reader can observe a mixture of two
//! snapshots. The join keys are RplId for groups and deployments and the positional m_Bases index for
//! bases; FindGroup() and FindBase() are the two lookups a consumer actually needs, and both are
//! linear over a few dozen records, which is cheaper than maintaining a map that has to be rebuilt
//! every poll anyway.
//------------------------------------------------------------------------------------------------
class OVT_GMCampaignState : Managed
{
	//! The occupying faction's resource distribution is skipped entirely while a QRF is running
	//! (OVT_OccupyingFactionManager.c:1169 - CheckUpdate returns before the 6-hour block). Without
	//! this flag a GM watches the distribution countdown reach zero, sees nothing happen, and files
	//! a bug.
	static const int FLAG_DISTRIBUTION_SUPPRESSED_QRF = 1;

	//! The resistance payout returns early when no players are connected
	//! (OVT_EconomyManagerComponent.c:161-165). Same reasoning as above.
	static const int FLAG_PAYOUT_SUPPRESSED_NO_PLAYERS = 2;

	//! Campaign threat, at float precision (OVT_OccupyingFactionManager.m_iThreat is a float and
	//! GetThreatLevel() truncates it - see GetThreatFloat()).
	float m_fThreat;

	//! The occupying faction's unspent reserve (OVT_OccupyingFactionManager.m_iResources).
	int m_iOFResources;

	//! The occupying faction's deployment pool (OVT_DeploymentManagerComponent faction resources).
	int m_iOFDeploymentResources;

	//! Bitfield of FLAG_* above: why a countdown may reach zero without anything happening.
	int m_iFlags;

	//! What the occupying faction will gain on its next distribution tick, predicted WITHOUT
	//! granting it (OVT_GMSchedule.PredictResourceGain).
	int m_iDistributionAmount;

	//! Real seconds to that distribution, as of m_fReceivedWorldTime.
	float m_fDistributionSeconds;

	//! What the resistance will be paid on its next payout (donations + tax).
	int m_iPayoutAmount;

	//! Real seconds to that payout, as of m_fReceivedWorldTime.
	float m_fPayoutSeconds;

	//! World time (ms) at which the last snapshot was committed. Zero means "nothing committed yet",
	//! which is also what HasData() answers on.
	float m_fReceivedWorldTime;

	//! One record per occupying-faction base, keyed by its positional index into
	//! OVT_OccupyingFactionManager.m_Bases. Never null.
	ref array<ref OVT_GMBaseRecord> m_aBases = new array<ref OVT_GMBaseRecord>();

	//! One record per NON-EMPTY base upgrade - an upgrade holding no resources and fielding no groups
	//! is not sent at all, which is why this count is smaller than the sum of m_iUpgrades over
	//! m_aBases. Never null.
	ref array<ref OVT_GMBaseUpgradeRecord> m_aBaseUpgrades = new array<ref OVT_GMBaseUpgradeRecord>();

	//! One record per active deployment, keyed by RplId. Never null.
	ref array<ref OVT_GMDeploymentRecord> m_aDeployments = new array<ref OVT_GMDeploymentRecord>();

	//! One record per tagged, live AI group, keyed by RplId. Never null.
	ref array<ref OVT_GMGroupRecord> m_aGroups = new array<ref OVT_GMGroupRecord>();

	//! How many records the SERVER said it sent. A consumer that finds this larger than the four
	//! array counts plus the campaign records is looking at a fan that lost records in flight or was
	//! truncated by the server's cap - which the server also logged.
	int m_iReportedRecordCount;

	//! The occupying faction's current objective, as a display name. EMPTY MEANS IT HAS NO TARGET,
	//! which is a normal state (an early campaign in which the resistance holds nothing) and not a
	//! missing value.
	//!
	//! ⚠ APPENDED, and every new scalar has to be added to THREE places, not one: the declaration here,
	//! CopyFrom() and Clear(). A field missed in Clear() leaks the previous campaign's objective into
	//! the next one - the editor closes, the store is emptied, and one row keeps showing a town from a
	//! game that ended.
	string m_sObjectiveName;

	//! Which phase of the ramp that objective is in, as OVT_EObjectivePhase's INTEGER.
	//!
	//! Stored as the ordinal that crossed the wire rather than as the enum: the sending build may be
	//! older or newer than this one, and a value this build does not recognise must be displayable as
	//! "unknown" rather than silently read as whichever member happens to share its number.
	int m_iObjectivePhase;

	//------------------------------------------------------------------------------------------------
	//! Whether a snapshot has ever been committed into this store.
	//! \return True once a complete snapshot has landed and has not since been cleared.
	bool HasData()
	{
		return m_fReceivedWorldTime > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the occupying faction's distribution is currently suppressed by a running QRF.
	//! \return True when the countdown will pass without a distribution.
	bool IsDistributionSuppressed()
	{
		return (m_iFlags & FLAG_DISTRIBUTION_SUPPRESSED_QRF) != 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the resistance payout is currently suppressed because nobody is connected.
	//! \return True when the countdown will pass without a payout.
	bool IsPayoutSuppressed()
	{
		return (m_iFlags & FLAG_PAYOUT_SUPPRESSED_NO_PLAYERS) != 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Real seconds until the occupying faction's next resource distribution, ticked down locally
	//! from the last snapshot. Never negative.
	//! \return Seconds remaining, or 0 when due/unknown.
	float GetDistributionSecondsRemaining()
	{
		return RemainingFrom(m_fDistributionSeconds);
	}

	//------------------------------------------------------------------------------------------------
	//! Real seconds until the next resistance payout, ticked down locally from the last snapshot.
	//! Never negative.
	//! \return Seconds remaining, or 0 when due/unknown.
	float GetPayoutSecondsRemaining()
	{
		return RemainingFrom(m_fPayoutSeconds);
	}

	//------------------------------------------------------------------------------------------------
	//! The origin record of one AI group, by the RplId a GM selection yields.
	//!
	//! The caller has an entity (a group the GM clicked, or one under an icon) and reads its
	//! RplComponent.Id(); an EntityID would name a different entity or nothing at all on the server,
	//! which is why the wire carries RplId and this is keyed on it.
	//! \param[in] rplId The group entity's RplId.
	//! \return Its record, or null when the group is untagged, dead or this store is empty.
	OVT_GMGroupRecord FindGroup(RplId rplId)
	{
		if(!rplId.IsValid()) return null;

		foreach(OVT_GMGroupRecord record : m_aGroups)
		{
			if(record && record.m_RplId == rplId) return record;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The aggregate record of one base, by its positional index into
	//! OVT_OccupyingFactionManager.m_Bases - the same index a consumer uses to read the base's
	//! location out of the locally replicated base list.
	//! \param[in] baseIndex Positional index into m_Bases.
	//! \return Its record, or null when no snapshot has landed or the index is not in it.
	OVT_GMBaseRecord FindBase(int baseIndex)
	{
		if(baseIndex < 0) return null;

		foreach(OVT_GMBaseRecord record : m_aBases)
		{
			if(record && record.m_iBaseIndex == baseIndex) return record;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Copies another store's values into this one. Used by the commit path to move a fully staged
	//! snapshot into the live store in one step, so no reader can ever observe a half-applied
	//! snapshot.
	//! \param[in] other The staging store.
	void CopyFrom(OVT_GMCampaignState other)
	{
		if(!other) return;

		m_fThreat = other.m_fThreat;
		m_iOFResources = other.m_iOFResources;
		m_iOFDeploymentResources = other.m_iOFDeploymentResources;
		m_iFlags = other.m_iFlags;
		m_iDistributionAmount = other.m_iDistributionAmount;
		m_fDistributionSeconds = other.m_fDistributionSeconds;
		m_iPayoutAmount = other.m_iPayoutAmount;
		m_fPayoutSeconds = other.m_fPayoutSeconds;
		m_iReportedRecordCount = other.m_iReportedRecordCount;

		// ⚠ THE OBJECTIVE PAIR BELONGS HERE AND NOT IN CopyRecords(). That method copies the four
		// PER-ENTITY record arrays; these two are campaign-wide scalars of exactly the same kind as
		// threat and the resource pools above, and they arrive on their own campaign record, not on the
		// per-entity fan.
		m_sObjectiveName = other.m_sObjectiveName;
		m_iObjectivePhase = other.m_iObjectivePhase;

		// Element copies, not array-object swaps: the staging store keeps its arrays for the next fan
		// and refills them with FRESH record objects, so the two stores never alias a record that is
		// about to be rewritten, and a consumer holding a record from the previous snapshot is not
		// surprised by it changing under them.
		CopyRecords(other);
	}

	//------------------------------------------------------------------------------------------------
	//! The record half of CopyFrom(). Split out only to keep that method readable.
	//! \param[in] other The staging store.
	protected void CopyRecords(notnull OVT_GMCampaignState other)
	{
		m_aBases.Clear();
		foreach(OVT_GMBaseRecord baseRecord : other.m_aBases)
		{
			m_aBases.Insert(baseRecord);
		}

		m_aBaseUpgrades.Clear();
		foreach(OVT_GMBaseUpgradeRecord upgradeRecord : other.m_aBaseUpgrades)
		{
			m_aBaseUpgrades.Insert(upgradeRecord);
		}

		m_aDeployments.Clear();
		foreach(OVT_GMDeploymentRecord deploymentRecord : other.m_aDeployments)
		{
			m_aDeployments.Insert(deploymentRecord);
		}

		m_aGroups.Clear();
		foreach(OVT_GMGroupRecord groupRecord : other.m_aGroups)
		{
			m_aGroups.Insert(groupRecord);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Drops everything. Called when the editor closes: a GM who is not looking holds no campaign
	//! data, and a stale store must never be mistaken for a live one.
	void Clear()
	{
		m_fThreat = 0;
		m_iOFResources = 0;
		m_iOFDeploymentResources = 0;
		m_iFlags = 0;
		m_iDistributionAmount = 0;
		m_fDistributionSeconds = 0;
		m_iPayoutAmount = 0;
		m_fPayoutSeconds = 0;
		m_fReceivedWorldTime = 0;
		m_iReportedRecordCount = 0;

		// ⚠ AND HERE. A field added to the declaration and to CopyFrom() but forgotten in Clear() shows
		// no symptom until a second campaign starts in the same client session, at which point the panel
		// labels it with the previous campaign's objective until the first snapshot lands.
		m_sObjectiveName = "";
		m_iObjectivePhase = OVT_EObjectivePhase.IDLE;

		m_aBases.Clear();
		m_aBaseUpgrades.Clear();
		m_aDeployments.Clear();
		m_aGroups.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! The local tick-down shared by both countdown readers.
	//! \param[in] snapshotSeconds Seconds remaining as of the last commit.
	//! \return Seconds remaining now, clamped at 0.
	protected float RemainingFrom(float snapshotSeconds)
	{
		if(snapshotSeconds <= 0) return 0;
		if(m_fReceivedWorldTime <= 0) return 0;

		BaseWorld world = GetGame().GetWorld();
		if(!world) return snapshotSeconds;

		float elapsed = (world.GetWorldTime() - m_fReceivedWorldTime) / 1000;
		float remaining = snapshotSeconds - elapsed;
		if(remaining < 0) return 0;

		return remaining;
	}
}
