//------------------------------------------------------------------------------------------------
//! OVT_ControllerComponent<T>.Get() resolves EVERY component the local player's
//! OVT_OverthrowController is supposed to carry.
//!
//! WHY THIS CASE EXISTS. A controller component fails in the quietest way this codebase has: written,
//! compiled, and simply never added to Prefabs/GameMode/OVT_OverthrowController.et. There is no
//! compile error (the class is fine), no runtime error (the accessor answers null, and every caller
//! null-guards by contract), and no log line. The only symptom is that a request the player makes
//! never happens. This case turns that silence into a named failure, and it is the reason the
//! controller-migration plan adds one Init assertion per new component rather than trusting the
//! prefab edit.
//!
//! IT ALSO PINS THE ACCESSOR ITSELF. OVT_ControllerComponent<Class T> is epic-level API - two other
//! features (core/options, core/player-groups) committed to that exact spelling in prose before it
//! existed - so "the generic composes with OVT_ComponentFinder and returns the component on the local
//! controller" is a claim worth an assertion of its own, not just a side effect of the roster check.
//!
//! THREE INDEPENDENT CLAIMS, DELIBERATELY IN ONE CASE, because they share one expensive precondition
//! (a local player whose controller has been spawned and registered):
//!   1. all seven pre-existing components resolve;
//!   2. what comes back is the SAME instance the controller entity carries - not merely "a component
//!      of that type from somewhere", which a future Get() that searched the wrong entity would also
//!      satisfy;
//!   3. a type the controller does NOT carry resolves NULL. Without this negative control an accessor
//!      that returned something for everything would make claims 1 and 2 pass vacuously.
//!
//! THE POLL IS A PRECONDITION, NOT A RETRY. OVT_PlayerManagerComponent.SetupPlayer() spawns the
//! controller from OVT_SpawnLogic.DoSpawn_S when the player enters the world, which is not
//! instantaneous at world load. The case waits for that to have happened; EXPIRY IS ITSELF A NAMED
//! FAILURE with the diagnosis in it, so nothing here can pass by being asked at a luckier moment.
//! (Same shape as OVT_TEST_Init_Persistence_PlayerCharacterConfigSelfSpawns. No maxAttempts anywhere.)
//!
//! PROVEN ABLE TO FAIL (2026-08-14): with the OVT_TowerSabotageComponent block deleted from
//! Prefabs/GameMode/OVT_OverthrowController.et - the literal "component written but never wired"
//! defect - the case fails with "OVT_ControllerComponent<OVT_TowerSabotageComponent>.Get() returned
//! null"; block restored, it passes.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_Controller_ComponentsResolve : SCR_AutotestCaseBase
{
	//! Frame polls allowed for the local player's controller to be spawned and registered.
	static const int MAX_POLLS = 300;

	protected int m_iPolls;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_OverthrowController controller = OVT_Global.GetController();
		if (!controller)
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_POLLS)
			{
				SetFailure("OVT_Global.GetController() was still null after %1 polls (local player id %2, local controlled entity %3). Nothing on the controller seam is reachable from this machine: either the player never entered the world, or OVT_PlayerManagerComponent.SetupPlayer() never spawned/registered a controller for them.",
					m_iPolls.ToString(),
					SCR_PlayerController.GetLocalPlayerId().ToString(),
					BoolWord(SCR_PlayerController.GetLocalControlledEntity() != null));
				return true;
			}

			return false;
		}

		// Claim 1 - the whole roster.
		string firstMissing = FindFirstUnresolvedComponent();
		if (firstMissing != "")
		{
			SetFailure("OVT_ControllerComponent<%1>.Get() returned null while a controller entity exists. Prefabs/GameMode/OVT_OverthrowController.et is not carrying that component, so every request that rides it silently never happens - no compile error, no runtime error, no log line.",
				firstMissing);
			return true;
		}

		// Claim 2 - identity, not just type. Checked on one component; the accessor has a single body,
		// so one instance comparison covers the mechanism for all of them.
		OVT_TutorialComponent viaAccessor = OVT_ControllerComponent<OVT_TutorialComponent>.Get();
		OVT_TutorialComponent onEntity = OVT_TutorialComponent.Cast(controller.FindComponent(OVT_TutorialComponent));
		if (viaAccessor != onEntity)
		{
			SetFailure("OVT_ControllerComponent<OVT_TutorialComponent>.Get() did not return the instance on the local player's own controller entity. The accessor is resolving a component from somewhere else, which means every client->server request could be sent through another player's seam.");
			return true;
		}

		// Claim 3 - the negative control. A manager component lives on the GAME MODE and must never
		// resolve off a controller.
		if (OVT_ControllerComponent<OVT_TownManagerComponent>.Get())
		{
			SetFailure("OVT_ControllerComponent<OVT_TownManagerComponent>.Get() resolved something. A manager component is not on the controller entity, so the accessor is answering for types it should not - every assertion above would then be vacuous.");
			return true;
		}

		PrintFormat("Controller seam: all 7 controller components resolve through OVT_ControllerComponent<T>.Get() (controller found after %1 poll(s))", m_iPolls.ToString());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Checks every component the controller prefab is supposed to carry and names the FIRST one the
	//! generic accessor could not resolve.
	//! \return The unresolved component's class name, or an empty string when all of them resolved.
	protected string FindFirstUnresolvedComponent()
	{
		if (!OVT_ControllerComponent<OVT_ContainerTransferComponent>.Get()) return "OVT_ContainerTransferComponent";
		if (!OVT_ControllerComponent<OVT_ShopTransactionComponent>.Get()) return "OVT_ShopTransactionComponent";
		if (!OVT_ControllerComponent<OVT_TowerSabotageComponent>.Get()) return "OVT_TowerSabotageComponent";
		if (!OVT_ControllerComponent<OVT_TutorialComponent>.Get()) return "OVT_TutorialComponent";
		if (!OVT_ControllerComponent<OVT_AdminCommandsComponent>.Get()) return "OVT_AdminCommandsComponent";
		if (!OVT_ControllerComponent<OVT_TravelRequestComponent>.Get()) return "OVT_TravelRequestComponent";
		if (!OVT_ControllerComponent<OVT_RespawnRequestComponent>.Get()) return "OVT_RespawnRequestComponent";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Renders a bool for a failure message without a ternary (EnforceScript has none).
	//! \param[in] value The value to describe.
	//! \return "yes" or "no".
	protected string BoolWord(bool value)
	{
		if (value)
			return "yes";

		return "no";
	}
}
