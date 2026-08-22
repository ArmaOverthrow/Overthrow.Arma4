//------------------------------------------------------------------------------------------------
//! The occupying faction's current intent, as plain data.
//!
//! Everything here is a Managed record in the OVT_FOBData shape (OVT_ResistanceFactionManager.c:22-38):
//! public fields, no behaviour, no back-references. They are owned by OVT_ObjectiveDirectorComponent -
//! more precisely by the OVT_ObjectiveInstance inside it, which shares these very objects with the
//! director rather than copying them - written by its own version-first serializer, and read by the
//! Game Master snapshot builder.
//!
//! ⚠ THE "ENUM INTEGERS MAY NEVER BE RENUMBERED" RULE THAT USED TO BE HERE IS DEAD, AND ITS REMOVAL
//! IS DELIBERATE (occupying/objectives D2). It said the phase enum travelled in the save payload
//! positionally, so renumbering a member would silently re-label every saved objective. That is no
//! longer true of either wire format: THE SAVE PAYLOAD CARRIES THE AUTHORED PHASE NAME AND THE
//! AUTHORED PLAN NAME AS STRINGS (version 2 - see OVT_ObjectiveDirectorSerializer), and the Game
//! Master snapshot carries the phase NAME too. A name the running build does not recognise is
//! detected, logged and the objective abandoned, which is a thing an integer could never do.
//!
//! Leaving the old warning in place would have been worse than useless: it would have told the next
//! reader to preserve a format nothing reads, and preserving it was the ONLY thing keeping the phase
//! enum from being deleted along with the last hard-coded phase handler in build Phase 6.
//!
//! ⚠ WHAT REPLACES IT: THE NAMES ARE THE KEYS NOW. OVT_ObjectiveConfig.m_sObjectiveName and
//! OVT_ObjectivePhase.m_sPhaseName are the persistence keys, and renaming either abandons every save
//! that carries it - loudly, by name, on the next load. That is the constraint to respect. The phase
//! enum that used to live in this file was the last consumer of the old rule and was deleted in build
//! phase 7, with the Game Master wire that carried it.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! What kind of place the current objective is.
//!
//! Villages, radio towers and forward bases are deliberately NOT kinds: villages fall as collateral,
//! towers are handled WITHIN an objective, and a forward base is the occupying faction's own.
//!
//! ⚠ IT IS NO LONGER A WIRE FORMAT, but it is still saved as an integer and is still the cheapest
//! honest way to say what kind of place a target is, so it survives the framework. A new member is
//! APPENDED with the next free integer - which costs nothing now that an unrecognised value simply
//! reads as "no objective" (KindFromInt) rather than as whatever member sits at that position.
//------------------------------------------------------------------------------------------------
enum OVT_EObjectiveKind
{
	NONE = 0,
	TOWN = 1,
	BASE = 2
}

//------------------------------------------------------------------------------------------------
//! The current objective: what it is, where it is, how far the ramp has got, and every timer.
//!
//! ⚠ EVERY TIMER HERE IS A TICK COUNT, NOT A DEADLINE (D4). One director tick decrements each of
//! them by one, so an early return in the tick freezes them all by construction and they serialize as
//! plain integers that still mean the same thing in the session that reads them back.
//------------------------------------------------------------------------------------------------
class OVT_ObjectiveRecord : Managed
{
	//! What kind of place this is. NONE means there is no objective, whatever the other fields say.
	OVT_EObjectiveKind kind;

	//! Where it is. THE KEY: positions are what this epic already persists and match back reliably,
	//! and a town id or a base index is neither stable across worlds nor meaningful in a save.
	vector position;

	//! Display name, for logs and for the GM panel. Resolved from the campaign when the objective is
	//! chosen and re-resolved on the first tick after a restore - it is a label, never an identifier,
	//! and is deliberately NOT part of the save payload.
	string name;

	// ⚠ THE PHASE THAT USED TO BE HERE IS GONE, AND SO IS THE ENUM IT WAS. Which phase an objective is
	// in is an INDEX into its plan plus that phase's authored m_sPhaseName, both held on the
	// OVT_ObjectiveInstance that owns this record - one place, written by one funnel
	// (OVT_ObjectiveDirectorComponent.EnterPhase). A copy here would be a second source of truth for
	// the one fact the whole machine steps on, and the integer form could not describe a modded plan's
	// phases at all: it was the last thing keeping the ramp's three shipped phases special.

	//! Ticks left before this phase gives up and the objective is abandoned. The wedge-breaker: every
	//! phase has an exit, and this is the one that does not depend on anything going right.
	int phaseTicks;

	//! Ticks left before the next operation is sent at this objective.
	int nextOpTicks;

	// ⚠ THE TWO SUCCESS COUNTERS THAT USED TO BE HERE ARE NOW BAG KEYS ON THE INSTANCE -
	// OVT_ObjectiveInstance.BAG_HARASSMENT_SUCCESSES and BAG_SABOTAGE_SUCCESSES. They moved because
	// they were never properties of the TARGET, they were state a module accumulated about it, and
	// leaving them here would have meant one save format for two counters and another for every
	// counter a module adds after them. GetHarassmentSuccesses() and GetSabotageSuccesses() on the
	// director keep their names and their meanings and now read the bag.
}

//------------------------------------------------------------------------------------------------
//! Something the occupying faction has standing in the field, reduced to the two questions anyone
//! outside the director ever asks: is it there, and where.
//!
//! THE BASE OF EVERY ASSET RECORD, AND THE ONLY PART THE KEYED API READS. The director holds its
//! assets in a map<string, ref OVT_ObjectiveAssetRecord> and answers IsAssetUp(key) /
//! GetAssetPosition(key) from these two fields alone, so a new asset - a checkpoint, a supply cache -
//! is a new key and a new subclass, never a new method pair.
//------------------------------------------------------------------------------------------------
class OVT_ObjectiveAssetRecord : Managed
{
	//! Whether the asset is standing. False leaves every other field on the record meaningless rather
	//! than wrong - they are only read while this is true.
	bool up;

	//! Where the asset stands.
	vector position;

	//! The base it is supplied from. Losing that base is one of the three starvation inputs.
	vector sourceBasePosition;

	//! What has been spent from the deployment pool on this asset and everything sourced from it,
	//! counted against the ceiling its module declares.
	//!
	//! ⚠ IT IS A COUNTER, NOT A WALLET (G5). Every resource it records left the ONE deployment pool
	//! at the moment its deployment was created; nothing is held here. The ceiling it is counted
	//! against comes from OVT_ObjectivePhaseRules.FOBBudgetCeiling().
	int spent;

	//! Consecutive ticks this asset has been cut off. Counts UP - the one counter in the machine that
	//! does, because recovery has to zero it and a countdown cannot express that. The freeze covers it
	//! identically: no tick, no increment.
	int starvationTicks;

	//! Config name of the deployment that carries the asset. THE RE-LINK KEY: after a load the
	//! director finds the live deployment again by this name plus the position above, on its first
	//! tick, never during deserialization.
	string deploymentName;
}

//------------------------------------------------------------------------------------------------
//! The occupying faction's forward operating base for the current objective.
//!
//! 🔴 EVERY FIELD IT USED TO ADD IS NOW ON THE BASE RECORD, AND THAT MOVE IS occupying/objectives
//! BUILD PHASE 5'S. §3.5 of the plan describes ONE asset record carrying up, position, source, spent,
//! starvationTicks and deploymentName, and the checkpoint asset that follows this feature is meant to
//! be a new KEY rather than a new record class. While the six fields were split across a base and a
//! forward-base subclass, every generic reader - the director's keyed spend counter, its keyed
//! starvation writer, the save payload's six parallel arrays - had to cast to the FOB type to see
//! four of them, which is exactly the "one asset API" the rename in build phase 1 removed.
//!
//! IT SURVIVES AS A NAMED TYPE, deliberately: OVT_ObjectiveDirectorComponent allocates one under the
//! ASSET_FOB key and OVT_ObjectiveDirectorSerializer builds one from a payload, and a named type is
//! what makes those two signatures say WHICH asset they are about. A second asset subclasses this the
//! same way, with its own fields if it has any that these six cannot express.
//------------------------------------------------------------------------------------------------
class OVT_ObjectiveFOBRecord : OVT_ObjectiveAssetRecord
{
}

//------------------------------------------------------------------------------------------------
//! One place the director has agreed to leave alone for a while.
//!
//! An objective that could not be built toward - no forward-base site anywhere in its band, most
//! often - sits out ONE selection round rather than being picked again immediately and failing
//! again. It is a cooldown, not a ban.
//------------------------------------------------------------------------------------------------
class OVT_ObjectiveBlacklistEntry : Managed
{
	//! Where the blacklisted objective was. Matched through OVT_ObjectiveSelection.PositionKey().
	vector position;

	//! Selection rounds still to serve. Zero means served; the entry is pruned on the next selection.
	int roundsLeft;
}

//------------------------------------------------------------------------------------------------
//! One deployment the director created for the current objective, so the reset path can take it back
//! down again.
//!
//! NAME PLUS POSITION, NOT AN ID. Deployment entity ids do not survive a session and the deployment
//! framework's own lookup is name-plus-position (GetDeploymentNearPosition), so this record is
//! exactly the pair that lookup wants. It is runtime-only and is NOT persisted: a restored campaign
//! re-links only the forward base, and anything else the director had standing is either still
//! findable at the objective or already gone.
//------------------------------------------------------------------------------------------------
class OVT_ObjectiveDeploymentRef : Managed
{
	//! The deployment config's name, as the framework registers it.
	string configName;

	//! Where it was created.
	vector position;
}
