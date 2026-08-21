//------------------------------------------------------------------------------------------------
//! ONE OBJECTIVE, AS PLAIN DATA, FOR THE SAVE PAYLOAD.
//!
//! WHY A RECORD CLASS AND NOT A RUN OF FIELDS. 🔴 A SAVE CONTEXT'S PROPERTIES ARE KEYED BY THE NAME
//! OF THE LOCAL VARIABLE HANDED TO Write() AND Read() - see the serializer's header below - so a
//! per-instance loop that wrote `configName` twice would write one property twice, and there would be
//! no way to tell the two objectives apart on the way back in. Every other N-record payload in this
//! epic already solved that the same way: an array of record objects, written under ONE name, with the
//! count implied by the array (OVT_PersistedJobV2, OVT_PersistedBase, OVT_PersistedLoadoutItem).
//!
//! It mirrors the runtime state rather than being it, for the reason OVT_PersistedLoadoutItem gives:
//! adding a field to OVT_ObjectiveInstance must not silently change the save layout of every existing
//! campaign.
//!
//! ⚠ THE BAG AND THE ASSET REGISTRY TRAVEL AS PARALLEL ARRAYS, not as maps. That is the same choice
//! the loadout record makes for its properties map, and for the same reason: parallel primitive arrays
//! are the forms these contexts are known to round-trip, and the pairing is re-associated by index.
//!
//! ⚠ assetUp IS AN array<int> OF 0/1 AND NOT AN array<bool>, DELIBERATELY. Not one payload anywhere in
//! this tree writes an array of booleans, so nothing establishes that the form round-trips; a single
//! `bool` field does (OVT_PersistedLoadoutItem.isEquipped) and an array<int> does (every counter array
//! in the epic). This is a save format - it is not the place to be the first caller of an untested
//! shape.
//------------------------------------------------------------------------------------------------
class OVT_PersistedObjective
{
	//! The plan being run, by its persistence key. Empty when no registry resolved.
	string configName;

	//! OVT_EObjectiveKind ordinal.
	int targetKind;

	//! Where the objective is. THE KEY - positions are what this epic already persists.
	vector targetPosition;

	//! The authored name of the phase it is in. THE OTHER KEY.
	string phaseName;

	//! The idle clock.
	int phaseTicks;

	//! The countdown to the next operation.
	int nextOpTicks;

	//! Every integer any module has reported, flattened. Index-aligned with bagValues.
	ref array<string> bagKeys = {};
	ref array<int> bagValues = {};

	//! Every position any module has handed forward, flattened. Index-aligned with bagVecValues.
	ref array<string> bagVecKeys = {};
	ref array<vector> bagVecValues = {};

	//! The asset registry, flattened. Every one of the six arrays below is index-aligned with
	//! assetKeys, and every asset writes every field - see the class header on raggedness.
	ref array<string> assetKeys = {};
	ref array<int> assetUp = {};
	ref array<vector> assetPosition = {};
	ref array<vector> assetSource = {};
	ref array<int> assetSpent = {};
	ref array<int> assetStarvation = {};
	ref array<string> assetDeployment = {};

	//------------------------------------------------------------------------------------------------
	//! Copies a live objective into this record. Reads only through the instance's public API.
	//! \param[in] instance The objective being saved.
	void ReadFrom(notnull OVT_ObjectiveInstance instance)
	{
		configName = instance.GetConfigName();
		targetKind = instance.GetTargetKind();
		targetPosition = instance.GetTargetPosition();
		phaseName = instance.GetPhaseName();
		phaseTicks = instance.GetPhaseTicks();
		nextOpTicks = instance.GetNextOpTicks();

		// THE BAG IS THE FORMAT (D9): no module writes its own record, so this pair carries every
		// counter every module has ever reported, enumerable by construction.
		instance.ReadBag(bagKeys, bagValues);
		instance.ReadBagV(bagVecKeys, bagVecValues);

		instance.ReadAssetKeys(assetKeys);

		foreach (string key : assetKeys)
		{
			OVT_ObjectiveAssetRecord asset = instance.GetAsset(key);

			int up = 0;
			vector position = vector.Zero;
			vector source = vector.Zero;
			int spent = 0;
			int starvation = 0;
			string deployment = "";

			if (asset)
			{
				if (asset.up)
					up = 1;

				position = asset.position;

				// ⚠ READ OFF THE BASE RECORD, WITH NO CAST TO THE FORWARD-BASE TYPE. All six fields
				// moved onto OVT_ObjectiveAssetRecord in build phase 5 precisely so this loop is the
				// same six lines for every asset key - a checkpoint asset is a new KEY here, not a new
				// branch, and a cast would silently write zeroes for any asset that is not a forward
				// base.
				source = asset.sourceBasePosition;
				spent = asset.spent;
				starvation = asset.starvationTicks;
				deployment = asset.deploymentName;
			}

			assetUp.Insert(up);
			assetPosition.Insert(position);
			assetSource.Insert(source);
			assetSpent.Insert(spent);
			assetStarvation.Insert(starvation);
			assetDeployment.Insert(deployment);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The forward base this record carries, if it carries one.
	//!
	//! ⚠ IT BUILDS PLAIN DATA AND LOOKS NOTHING UP, because it is called from a codec. The format is
	//! already N assets wide - the checkpoint asset that follows this feature is a new KEY, not a new
	//! field - and the APPLY grows to match when a module owns a second asset.
	//! \return The record, or null when no forward base was saved or the arrays are ragged.
	OVT_ObjectiveFOBRecord BuildForwardBaseRecord()
	{
		if (!assetKeys || !assetUp || !assetPosition || !assetSource || !assetSpent || !assetStarvation || !assetDeployment)
			return null;

		int count = assetKeys.Count();
		if (assetUp.Count() != count || assetPosition.Count() != count || assetSource.Count() != count)
			return null;

		if (assetSpent.Count() != count || assetStarvation.Count() != count || assetDeployment.Count() != count)
			return null;

		for (int i = 0; i < count; i++)
		{
			if (assetKeys[i] != OVT_ObjectiveDirectorComponent.ASSET_FOB)
				continue;

			OVT_ObjectiveFOBRecord fob = new OVT_ObjectiveFOBRecord();
			fob.up = assetUp[i] != 0;
			fob.position = assetPosition[i];
			fob.sourceBasePosition = assetSource[i];
			fob.spent = assetSpent[i];
			fob.starvationTicks = assetStarvation[i];
			fob.deploymentName = assetDeployment[i];

			return fob;
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! Persists the occupying faction's objectives: which PLAN each is running, where, which PHASE it is
//! in by name, every timer, every module counter, the blacklist and every standing asset.
//!
//! BINDING. Listed in the ComponentSerializers block of the game-mode configuration in
//! Configs/Systems/Persistence/Overthrow.conf.
//!
//! ================================================================================================
//! 🔴 THE LOCAL VARIABLE NAMES ARE THE PROPERTY NAMES. READ THIS BEFORE TOUCHING ANYTHING BELOW.
//! ================================================================================================
//! SaveContext.Write() and LoadContext.Read() derive the property key from the NAME OF THE LOCAL they
//! are handed. Writing `objectives` and reading the same payload into a local called `readObjectives`
//! does not read the wrong field - it reads NOTHING, silently, leaving the destination at its default
//! and every later assertion looking like a restore that lost the data.
//!
//! It has now cost this repository twice. OVT_JobManagerSerializer measured it on 2026-08-09 and says
//! so at its own read site. This serializer's version-2 rewrite (occupying/objectives Phase 2) hit it
//! again with `read`-prefixed locals in a per-instance loop, and the whole objective came back as "no
//! objective" - not as a crash, not as a warning, just an empty campaign. **Every Read() below is
//! therefore CHECKED, and every local is named exactly what Serialize() called it.**
//!
//! ⚠ AND IT IS WHY THERE IS NO PER-INSTANCE FIELD LOOP. Writing `configName` once per objective would
//! write one property N times. N objectives travel as an ARRAY OF RECORDS under one name - the shape
//! every other N-record payload in this epic already uses.
//!
//! ⚠ WHY THIS IS ITS OWN SERIALIZER AND NOT AN APPENDIX TO THE OCCUPYING FACTION'S.
//! OVT_OccupyingFactionManagerSerializer is version 2, fully positional, and its record classes carry
//! no version of their own - every field of OVT_PersistedBase is load-bearing by position, which is
//! why a dead `upgrades` array is still declared and still written there rather than removed. The
//! objective payload is expected to GROW as modules land; putting an actively-evolving structure
//! inside the most fragile format in the epic is how that format got its reputation. A separate
//! serializer costs one config entry and buys a format that grows by appending.
//!
//! ⚠ NOTHING HERE TOUCHES THE RESOURCE POOL OR ANY DEPLOYMENT, AND THAT IS A LOAD-ORDER RULE.
//! OVT_OccupyingFactionManager.c documents the hazard this inherits: the deployment manager's restore
//! CLEARS AND REFILLS the faction resource pool and runs AFTER the game-mode component serializers,
//! so anything credited or debited from in here would be overwritten moments later. Deployment
//! entities are separate tracked instances whose restore order relative to this payload is not
//! defined either. Both are therefore deferred to the director's FIRST TICK - the same pattern the
//! legacy upgrade refund uses (QueueLegacyUpgradeRefund -> CreditPendingLegacyRefund).
//!
//! DESERIALIZE IS A PURE CODEC. It reads the payload and makes exactly ONE side-effecting call, into
//! ApplyPersistedObjective() or DiscardPersistedObjective(). No lookup, no query, no arithmetic on
//! live state.
//!
//! A LIVE BATTLE STILL ROLLS BACK, exactly as the campaign has always done: nothing about
//! m_CurrentQRF is persisted, so an objective saved mid-counter-attack comes back with no battle to
//! wait for and the director resets it on its first tick, with the reason in the log.
//!
//! IDEMPOTENT ON A LIVE SESSION. Deserialize also runs when saved data is re-applied to a running
//! campaign (OVT_PersistenceManagerComponent.ReapplyLatestSaveData). Every line of the apply is an
//! assignment or a clear-and-rebuild, so a second pass produces the same state.
//!
//! ================================================================================================
//! FORMAT, VERSION 2. Version FIRST; new fields are APPENDED to a record, never inserted or removed.
//! ================================================================================================
//!
//!   1  int    version                                = 2
//!   2  array<ref OVT_PersistedObjective> objectives   0 entries while the campaign has no objective
//!   3  array<vector> blacklistPositions
//!   4  array<int>    blacklistRounds
//!
//! Each OVT_PersistedObjective carries the plan name, the target kind and position, the phase NAME,
//! both tick counters, the two bags and the asset registry - see that class.
//!
//! ⚠ WHY VERSION 2 REPLACES VERSION 1 WHOLESALE, WITH NO MIGRATION (D2). v1.5 is unreleased, so the
//! only saves carrying a version-1 record are development ones. Migrating would have been the ONLY
//! consumer of the frozen phase-enum integers, so dropping it deletes a whole constraint rather than
//! carrying it forever for one code path - see OVT_ObjectiveRecords.c's rewritten header. A developer
//! carrying an old save loses their in-flight objective and gets one ERROR line saying so; the next
//! tick chooses a new one.
//!
//! THE THREE VERSION OUTCOMES, EACH WITH ITS OWN LOG LINE:
//!   version == 0  the payload is ABSENT - a save from before this serializer, or a read that failed.
//!                 KEEP LIVE STATE, SILENTLY. This is the existing contract and it does not change.
//!   version == 2  read it.
//!   anything else a version-1 record, or one from a future build. Print an ERROR naming the version,
//!                 discard, and let the machine re-select.
//!
//! ⚠ A FAILED READ IS NOT AN EMPTY CAMPAIGN. An unreadable payload leaves the live objective exactly
//! as it is and says so at ERROR - it never applies "no objective", because a genuinely empty campaign
//! reads back successfully with zero records and the two cases are therefore distinguishable. That is
//! the same rule and the same reasoning as OVT_JobManagerSerializer's.
//!
//! ⚠ THE OBJECTIVE'S DISPLAY NAME IS DELIBERATELY NOT IN THE PAYLOAD. It is a label resolved from the
//! town and base registries, not an identifier; storing it would let a save disagree with the world
//! it is restored into. ResolveRestoredObjective() re-resolves it on the first tick.
//------------------------------------------------------------------------------------------------
class OVT_ObjectiveDirectorSerializer : ScriptedComponentSerializer
{
	//! The format this build writes and the only one it can read. See the header for the three
	//! outcomes.
	static const int RECORD_VERSION = 2;

	//! Log prefix, matching the director's own so a save fault reads as part of the same system.
	static const string LOG = "[Overthrow.ObjectiveDirector] ";

	//------------------------------------------------------------------------------------------------
	//! \return The component class this serializer is responsible for.
	override static typename GetTargetType()
	{
		return OVT_ObjectiveDirectorComponent;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes every running objective, then the blacklist.
	//!
	//! ⚠ EVERY LOCAL BELOW IS NAMED WHAT Deserialize() READS IT BACK INTO. See the class header.
	//! \param[in] owner The game mode entity owning the director.
	//! \param[in] component The director being saved.
	//! \param[in] context Save context to write into.
	//! \return OK, or ERROR when the component is not an Overthrow objective director.
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		OVT_ObjectiveDirectorComponent director = OVT_ObjectiveDirectorComponent.Cast(component);
		if (!director)
			return ESerializeResult.ERROR;

		context.WriteValue("version", RECORD_VERSION);

		array<ref OVT_PersistedObjective> objectives = new array<ref OVT_PersistedObjective>();

		int instanceCount = director.GetInstanceCount();
		for (int i = 0; i < instanceCount; i++)
		{
			OVT_ObjectiveInstance instance = director.GetObjectiveInstance(i);
			if (!instance)
				continue;

			OVT_PersistedObjective record = new OVT_PersistedObjective();
			record.ReadFrom(instance);

			objectives.Insert(record);
		}

		context.Write(objectives);

		// TWO PARALLEL ARRAYS rather than a record class, for the reason the deployment manager's
		// serializer gives for the same shape: it keeps the payload to the primitive array forms the
		// binary context is known to round-trip, and the pairing is re-associated by index on load.
		array<vector> blacklistPositions = new array<vector>();
		array<int> blacklistRounds = new array<int>();

		int blacklistCount = director.GetBlacklistCount();
		for (int b = 0; b < blacklistCount; b++)
		{
			blacklistPositions.Insert(director.GetBlacklistPosition(b));
			blacklistRounds.Insert(director.GetBlacklistRounds(b));
		}

		context.Write(blacklistPositions);
		context.Write(blacklistRounds);

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the objectives back and hands the first one to the director to apply.
	//!
	//! ⚠ THE FIRST ONE, AND A SECOND IS REPORTED RATHER THAN SILENTLY DROPPED. The director is shipped
	//! and tested at one concurrent objective; a payload written by a build configured for more is a
	//! real possibility and losing an objective without a word is not an acceptable way to handle it.
	//!
	//! ⚠ EVERY LOCAL BELOW IS NAMED WHAT Serialize() WROTE, AND EVERY READ IS CHECKED. See the class
	//! header for what a `read`-prefixed local costs.
	//! \param[in] owner The game mode entity owning the director.
	//! \param[in] component The director being loaded.
	//! \param[in] context Load context to read from.
	//! \return True when the payload was consumed.
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		OVT_ObjectiveDirectorComponent director = OVT_ObjectiveDirectorComponent.Cast(component);
		if (!director)
			return false;

		// See OVT_DeploymentManagerSerializer.Deserialize(). An ABSENT payload must not clear live
		// state: a campaign saved before this serializer existed, or one whose record failed to read,
		// keeps whatever objective it currently has rather than being silently sent back to idle.
		int version;
		context.ReadValue("version", version);
		if (version < 1)
			return true;

		if (version != RECORD_VERSION)
		{
			// ⚠ NOT ONE FIELD IS READ. This build has never seen this format, so every read after this
			// point would be reading somebody else's bytes.
			director.DiscardPersistedObjective("the saved objective record is version " + version.ToString() + ", and this build reads version " + RECORD_VERSION.ToString() + " only");
			return true;
		}

		array<ref OVT_PersistedObjective> objectives = new array<ref OVT_PersistedObjective>();
		if (!context.Read(objectives))
			return AbortUnreadablePayload("the objectives themselves");

		array<vector> blacklistPositions = new array<vector>();
		if (!context.Read(blacklistPositions))
			return AbortUnreadablePayload("the blacklisted places");

		array<int> blacklistRounds = new array<int>();
		if (!context.Read(blacklistRounds))
			return AbortUnreadablePayload("the blacklist cooldowns");

		if (objectives.Count() > 1)
			Print(LOG + "The save carries " + objectives.Count().ToString() + " objectives and this build runs one - only the first was restored", LogLevel.WARNING);

		if (objectives.IsEmpty())
		{
			// A campaign that had no objective when it was saved. NOT a fault, and not the same thing
			// as an unreadable payload - see the class header.
			director.ApplyPersistedObjective("", OVT_EObjectiveKind.NONE, vector.Zero, "", 0, 0, null, null, null, null, blacklistPositions, blacklistRounds, null);
			return true;
		}

		OVT_PersistedObjective record = objectives[0];
		if (!record)
			return AbortUnreadablePayload("the first objective record");

		director.ApplyPersistedObjective(record.configName, record.targetKind, record.targetPosition, record.phaseName, record.phaseTicks, record.nextOpTicks, record.bagKeys, record.bagValues, record.bagVecKeys, record.bagVecValues, blacklistPositions, blacklistRounds, record.BuildForwardBaseRecord());

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Reports an unreadable payload and leaves the live objective exactly as it is.
	//!
	//! ⚠ IT DOES NOT DISCARD AND IT DOES NOT APPLY "NO OBJECTIVE". A failed Read() leaves its
	//! destination non-null and EMPTY, and applying that would replace a running campaign's objective
	//! with nothing - which is indistinguishable in play from the machine working. A genuinely empty
	//! campaign reads back SUCCESSFULLY with zero records, so the two are distinguishable and this one
	//! aborts. Same rule, same reasoning, as OVT_JobManagerSerializer's.
	//! \param[in] what Which part of the payload could not be read, for the log line.
	//! \return True - the payload is consumed either way.
	protected bool AbortUnreadablePayload(string what)
	{
		Print(LOG + "Could not read " + what + " from a version " + RECORD_VERSION.ToString() + " objective payload - the current objective and its blacklist are left exactly as they are rather than being replaced with nothing. This is a save-format fault: a campaign with no objective reads back successfully, so this is not one", LogLevel.ERROR);

		return true;
	}
}
