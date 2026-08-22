//------------------------------------------------------------------------------------------------
//! The player controller prefab carries OVT_RecruitCommandComponent.
//!
//! THE TRAP THIS EXISTS FOR. A controller component that exists in script but is not listed in
//! Prefabs/GameMode/OVT_OverthrowController.et is a SILENT no-op: everything compiles, every call
//! site type-checks, OVT_ControllerComponent<OVT_RecruitCommandComponent>.Get() simply answers null forever, and the whole
//! feature - the held actions on the body, the roster's activate/deactivate button, Phase 4's
//! status push, Phase 7's loadout swap - does nothing at all, on every machine, with no error. The
//! project has lost features to exactly this before, which is why the fail-proof is a test and not
//! a code review.
//!
//! WHY IT SPAWNS THE PREFAB INSTEAD OF READING THE LIVE CONTROLLER. The Init tier has no started
//! campaign, so OVT_PlayerManagerComponent.SetupPlayer() - the only thing that ever spawns a
//! player's controller entity - has not necessarily run, and OVT_ControllerComponent<OVT_RecruitCommandComponent>.Get() would
//! answer null for a reason that has nothing to do with the prefab. The suite header says the same
//! thing about GetController() and excludes it for it. Spawning the manager's OWN configured prefab
//! resource asks the question the trap is actually about - "does this prefab carry the component" -
//! and asks it deterministically, with no player and no campaign involved. The live accessor is
//! still checked opportunistically when a controller happens to exist, because a passing live check
//! is free evidence that the accessor's cast matches the prefab entry.
//!
//! PROVEN ABLE TO FAIL (by construction, reasoned not run): the only assertion is
//! FindComponent(OVT_RecruitCommandComponent) on a fresh instance of the configured controller
//! prefab. Delete the four-line OVT_RecruitCommandComponent "{6B4C000000000003}" block from
//! Prefabs/GameMode/OVT_OverthrowController.et - the literal defect - and FindComponent answers
//! null on that instance, so the case fails with "the controller prefab does not carry
//! OVT_RecruitCommandComponent". Nothing else in the case can produce that message: the spawn and
//! the resource lookup have their own distinct failure strings, and the live-controller branch only
//! ever runs when a controller already exists. No polling, no timing, no maxAttempts.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_RecruitCommands_OnController : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players)
		{
			SetFailure("OVT_Global.GetPlayers() is null, so the controller prefab cannot be resolved");
			return true;
		}

		if (players.m_OverthrowControllerPrefab == ResourceName.Empty)
		{
			SetFailure("OVT_PlayerManagerComponent.m_OverthrowControllerPrefab is empty - no player would ever get a controller entity at all");
			return true;
		}

		Resource controllerResource = Resource.Load(players.m_OverthrowControllerPrefab);
		if (!controllerResource || !controllerResource.IsValid())
		{
			SetFailure("The configured controller prefab could not be loaded: %1", players.m_OverthrowControllerPrefab);
			return true;
		}

		EntitySpawnParams params = EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;

		IEntity controller = GetGame().SpawnEntityPrefab(controllerResource, null, params);
		if (!controller)
		{
			SetFailure("The configured controller prefab failed to spawn: %1", players.m_OverthrowControllerPrefab);
			return true;
		}

		bool hasComponent = controller.FindComponent(OVT_RecruitCommandComponent) != null;

		// Deleted before the assertion so a failure cannot also leak a controller entity into the
		// rest of the suite's world.
		SCR_EntityHelper.DeleteEntityAndChildren(controller);

		if (!hasComponent)
		{
			SetFailure("The controller prefab does not carry OVT_RecruitCommandComponent, so OVT_ControllerComponent<OVT_RecruitCommandComponent>.Get() answers null on every machine and every recruit command - the held actions, the roster button, the status push and the loadout swap - is a silent no-op. Add it back to Prefabs/GameMode/OVT_OverthrowController.et");
			return true;
		}

		// Opportunistic, never asserted-on when absent: in this tier the campaign is not started, so
		// a live controller may legitimately not exist yet.
		if (OVT_Global.GetController() && !OVT_ControllerComponent<OVT_RecruitCommandComponent>.Get())
		{
			SetFailure("A live OVT_OverthrowController exists but OVT_ControllerComponent<OVT_RecruitCommandComponent>.Get() is null on it, although the prefab carries the component - the accessor and the prefab entry disagree");
			return true;
		}

		Print("The controller prefab carries OVT_RecruitCommandComponent");
		return true;
	}
}
