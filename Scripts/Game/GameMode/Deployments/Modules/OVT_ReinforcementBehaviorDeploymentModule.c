//! Reinforcement behavior module for deployments
//! Handles decision-making about when and how to reinforce spawned units
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_ReinforcementBehaviorDeploymentModule : OVT_BaseBehaviorDeploymentModule
{
	[Attribute(desc: "Name of this module")]
	string m_sModuleName;
	
	[Attribute(defvalue: "true", desc: "Enable reinforcement")]
	bool m_bEnableReinforcement;
	
	[Attribute(defvalue: "60000", desc: "Check interval in milliseconds")]
	float m_fCheckInterval;
	
	
	[Attribute(defvalue: "300000", desc: "Delay after deployment activation before reinforcement is allowed (milliseconds)")]
	float m_fInitialDelay;
	
	[Attribute(defvalue: "120000", desc: "Minimum time between reinforcement attempts (milliseconds)")]
	float m_fReinforcementCooldown;
	
	[Attribute(defvalue: "false", desc: "Require town size > 1 for reinforcement (villages don't get reinforced)")]
	bool m_bRequireTownSizeCheck;
	
	[Attribute(defvalue: "false", desc: "Delete deployment if conditions are no longer met")]
	bool m_bDeleteOnConditionFail;

	//! HOW FAR OUT A DEFENDER STOPS THIS DEPLOYMENT BUYING ITS FORCE BACK, AS A MULTIPLE OF THE BASE'S
	//! RESTRICTED ZONE.
	//!
	//! ==========================================================================================
	//! 🔴 "I just wiped out an FOB garrison and while standing there looking at it, a sabotage team
	//! spawned in front of me. I thought that wasn't possible?" (author, 2026-08-21.)
	//! ==========================================================================================
	//! The rebuy had NO proximity test of any kind: it asked whether the force was short, whether the
	//! faction could pay, and nothing else. So a player who cleared a position was met by a replacement
	//! force bought on the next behaviour update - the log of that play-test has three rebuys inside two
	//! seconds (20:47:11-12) immediately after he wiped the garrison at 20:47:08.
	//!
	//! ==========================================================================================
	//! ⚠ A MULTIPLE AND NOT A METRE VALUE, BECAUSE THE FIRST ATTEMPT WAS 320 AND THAT WAS WORSE THAN
	//! USELESS - IT TRACKED THE EDGE OF THE ZONE THE PLAYER WAS STANDING ON.
	//! ==========================================================================================
	//! 320 was the framework's existing "a player would notice that" number - baseCloseRange (220) plus
	//! 100 - and a base's RESTRICTED ZONE is baseCloseRange plus FOB_DEPLOY_BASE_BUFFER (50), i.e. 270.
	//! Those two edges are fifty metres apart. A player who stands just outside the restricted zone,
	//! which is exactly what a player attacking a base does, is therefore also just outside the rebuy
	//! gate - so he can shoot into the base freely and farm reinforcements for as long as he likes
	//! (author play-test, 2026-08-21).
	//!
	//! Expressing it as a MULTIPLE OF THE ZONE is what stops that coming back. The zone is derived, not
	//! typed: ResolveNoRebuyRadius() reads difficulty.baseCloseRange and
	//! OVT_ResistanceFactionManager.FOB_DEPLOY_BASE_BUFFER, the same two terms OVT_MapRestrictedAreas
	//! draws the circle from, so a server that retunes baseCloseRange moves the zone AND this gate
	//! together. A bare 540 in this field would silently mean "twice the zone" until somebody changed
	//! the zone.
	//!
	//! 2 IS DOUBLE THE ZONE - about 540 m on the shipped difficulty - which puts the gate's edge far
	//! enough outside the zone's edge that standing on one is nowhere near the other.
	//!
	//! ⚠ AND IT IS ONLY HALF THE ANSWER. Proximity is a PROXY for "this position is being fought over",
	//! and no radius reaches a marksman at 600 m or a mortar. See m_fContactCooldown, which is the other
	//! half and the more important one.
	//!
	//! 0 disables the proximity gate entirely.
	[Attribute(defvalue: "2", desc: "How far out a living resistance force stops this deployment buying its own force back, AS A MULTIPLE OF the base restricted zone (difficulty baseCloseRange + 50 m, so ~270 m shipped; 2 = ~540 m). Expressed as a multiple so it cannot drift when the zone is retuned. The force is NOT cancelled - the rebuy is retried once nobody is that close. 0 disables this gate")]
	float m_fNoRebuyZoneMultiple;

	//! HOW LONG AFTER LOSING MEN THIS DEPLOYMENT REFUSES TO BUY THEM BACK, in milliseconds.
	//!
	//! ==========================================================================================
	//! 🔴 THIS IS THE HALF THAT MAKES ATTRITION MEAN ANYTHING.
	//! ==========================================================================================
	//! Proximity answers "somebody is standing on this position". It cannot answer "this position is
	//! being fought over", and those come apart the moment the attacker has range: a marksman at 600 m,
	//! a mortar, a machine gun on a ridge. No radius large enough to cover those is small enough to be a
	//! sane gate.
	//!
	//! ⚠ WITHOUT IT, KILLING A GARRISON ACHIEVES NOTHING AND THE FIGHT IS POINTLESS RATHER THAN HARD.
	//! A position rebought twenty seconds after it was cleared cannot be worn down, so sustained
	//! pressure - the thing an attacker actually has to spend to take a place - buys the attacker
	//! exactly nothing. With a contact cooldown the loop finally closes the way a player expects:
	//! keeping the pressure on holds the position down, and DISENGAGING is what lets it recover. That is
	//! attrition, and it is the reason this attribute matters more than the radius above.
	//!
	//! 90 SECONDS, AND THE NUMBER IS CHOSEN AGAINST m_fCheckInterval RATHER THAN BY FEEL. That interval
	//! defaults to 60 s, so a cooldown of 90 s guarantees that a single casualty costs the deployment AT
	//! LEAST ONE WHOLE REINFORCEMENT CHECK - a shorter one could expire inside the gap between two checks
	//! and change nothing. It is long enough that a real firefight, which produces casualties every few
	//! seconds, keeps it permanently re-armed for as long as the fight lasts; and short enough that a
	//! player who breaks contact and leaves sees the position start recovering within a minute and a half
	//! rather than concluding the campaign has stopped working.
	//!
	//! ⚠ IT IS INDEPENDENT OF THE PROXIMITY GATE AND THE TWO ARE OR-ED. Either one refusing is a
	//! refusal: proximity covers the man standing in the compound who has not fired yet, contact covers
	//! the one shooting from a ridge two kilometres away. Neither subsumes the other.
	//!
	//! 0 disables the contact gate entirely.
	[Attribute(defvalue: "90000", desc: "Milliseconds after this deployment last LOST MEN during which it refuses to buy them back. Covers an attacker too far away for the proximity gate to see - a marksman, a mortar. The force is NOT cancelled; the rebuy is retried once the cooldown expires. 0 disables this gate")]
	float m_fContactCooldown;
	
	//! World time this deployment was last seen to have LOST men, or 0 when it never has.
	protected float m_fLastContactTime;

	//! The force's total survivor count at the previous sample, or -1 before the first one. The DIFF is
	//! the casualty signal - see SampleCasualties().
	protected int m_iLastAliveSample = -1;

	protected float m_fLastCheckTime;
	protected float m_fLastReinforcementTime;
	protected float m_fActivationTime;
	
	//------------------------------------------------------------------------------------------------
	void OVT_ReinforcementBehaviorDeploymentModule()
	{
		m_fLastCheckTime = 0;
		m_fLastReinforcementTime = 0;
		m_fActivationTime = 0;
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnActivate()
	{
		super.OnActivate();
		m_fActivationTime = GetGame().GetWorld().GetWorldTime();
		m_fLastCheckTime = m_fActivationTime;
		m_fLastReinforcementTime = 0;
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnUpdate(int deltaTime)
	{
		super.OnUpdate(deltaTime);
		
		if (!m_bEnableReinforcement)
			return;

		// ⚠ BEFORE THE INTERVAL GATE BELOW, AND THAT PLACEMENT IS THE POINT. Everything after it runs
		// once a minute; the casualty signal has to be sampled on every deployment update or a
		// ninety-second cooldown carries a minute of error. See SampleCasualties().
		SampleCasualties();

		float currentTime = GetGame().GetWorld().GetWorldTime();
		
		// Check if enough time has passed since last check
		if (currentTime - m_fLastCheckTime < m_fCheckInterval)
			return;
		
		m_fLastCheckTime = currentTime;
		
		// Check if initial delay has passed
		if (currentTime - m_fActivationTime < m_fInitialDelay)
			return;
		
		// Check if cooldown has passed since last reinforcement
		if (m_fLastReinforcementTime > 0 && currentTime - m_fLastReinforcementTime < m_fReinforcementCooldown)
			return;
		
		CheckReinforcement();
	}
	
	//------------------------------------------------------------------------------------------------
	override OVT_BaseDeploymentModule CloneModule()
	{
		OVT_ReinforcementBehaviorDeploymentModule clone = new OVT_ReinforcementBehaviorDeploymentModule();
		
		// Copy configuration
		clone.m_sModuleName = m_sModuleName;
		clone.m_bEnableReinforcement = m_bEnableReinforcement;
		clone.m_fCheckInterval = m_fCheckInterval;
		clone.m_fInitialDelay = m_fInitialDelay;
		clone.m_fReinforcementCooldown = m_fReinforcementCooldown;
		clone.m_bRequireTownSizeCheck = m_bRequireTownSizeCheck;
		clone.m_bDeleteOnConditionFail = m_bDeleteOnConditionFail;

		// ⚠ DROP THIS LINE AND EVERY DEPLOYMENT SHIPS THE CLASS DEFAULT, WHICH IS ZERO, WHICH DISABLES
		// THE GATE. CloneModule is copy-by-hand and a forgotten field does not warn, does not log and
		// does not fail to parse - it silently restores the 2026-08-21 "a team spawned in front of me"
		// behaviour on every config in the game. Same trap the vehicle module's m_fMaxCruiseSpeed hit.
		clone.m_fNoRebuyZoneMultiple = m_fNoRebuyZoneMultiple;
		clone.m_fContactCooldown = m_fContactCooldown;

		return clone;
	}
	
	//------------------------------------------------------------------------------------------------
	protected void CheckReinforcement()
	{
		if (!m_ParentDeployment)
			return;
				
		// Get all spawning modules
		array<OVT_BaseSpawningDeploymentModule> spawningModules =  m_ParentDeployment.GetSpawningModules();
		
		if (spawningModules.IsEmpty())
		{
			Print("Reinforcement behavior: No spawning modules found", LogLevel.WARNING);
			return;
		}
		
		// Check if reinforcement conditions are met
		if (!EvaluateReinforcementConditions())
		{
			if (m_bDeleteOnConditionFail)
			{
				Print("Reinforcement behavior: Conditions no longer met, requesting deployment deletion", LogLevel.NORMAL);
				RequestDeploymentDeletion();
			}
			return;
		}
		
		// Check each spawning module for reinforcement needs
		// 🔴 NOT WHILE SOMEBODY IS STANDING IN IT, AND NOT WHILE IT IS STILL BEING SHOT AT. Two
		// independent gates, OR-ed: see m_fNoRebuyZoneMultiple and m_fContactCooldown for the play-tests
		// this closes. Deliberately checked here, AFTER the condition evaluation above, so that a
		// deployment whose conditions have genuinely failed is still torn down while a player is present
		// - refusing to REBUY is not the same as refusing to DIE, and holding a dead deployment alive
		// because somebody is nearby would strand it.
		if (IsRebuyBlockedByDefender() || IsRebuyBlockedByContact())
			return;

		bool anyReinforced = false;
		foreach (OVT_BaseSpawningDeploymentModule spawningModule : spawningModules)
		{
			if (ShouldReinforceModule(spawningModule))
			{
				int missingUnits = GetMissingUnitsCount(spawningModule);
				if (missingUnits > 0 && CanAffordReinforcement(spawningModule, missingUnits))
				{
					if (TryReinforceModule(spawningModule, missingUnits))
					{
						anyReinforced = true;
					}
				}
			}
		}
		
		if (anyReinforced)
		{
			m_fLastReinforcementTime = GetGame().GetWorld().GetWorldTime();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------
	//! THE REBUY RADIUS IN METRES, derived from the base restricted zone rather than typed.
	//!
	//! ⚠ THE ZONE IS baseCloseRange + FOB_DEPLOY_BASE_BUFFER AND BOTH TERMS ARE READ, NOT ASSUMED. That
	//! is the same sum OVT_MapRestrictedAreas draws the circle on the player's map from and the same one
	//! OVT_ResistanceFactionManager refuses FOB placement inside, so this gate and the zone the player
	//! can actually SEE move together. See m_fNoRebuyZoneMultiple for the play-test that made deriving
	//! it necessary.
	//! \return The radius, or 0 when the gate is disabled or the campaign cannot be resolved.
	protected float ResolveNoRebuyRadius()
	{
		if (m_fNoRebuyZoneMultiple <= 0)
			return 0;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config || !config.m_Difficulty)
			return 0;

		float zone = config.m_Difficulty.baseCloseRange + OVT_ResistanceFactionManager.FOB_DEPLOY_BASE_BUFFER;

		return zone * m_fNoRebuyZoneMultiple;
	}

	//------------------------------------------------------------------------------------------------
	//! GATE ONE: is the resistance close enough that buying this force back would be an insult?
	//!
	//! ⚠ THE RESISTANCE, NOT PLAYERS. *"There should never be any 'players-only' contest test, it's
	//! resistance always"* (author) - a squad of recruits holding a position their owner cleared is
	//! holding it. See OVT_BaseBehaviorDeploymentModule.DefenderWithin().
	//!
	//! ⚠ THIS IS NOT ONE OF THE TWO "can anybody SEE this" TESTS, despite looking like one. The
	//! exfiltration rule and the abandoned-transport hold ask whether a human would watch something
	//! vanish; this asks whether anybody is HOLDING the ground, which is a contest question and takes
	//! the contest primitive.
	//! \return True when the rebuy must be skipped this update.
	protected bool IsRebuyBlockedByDefender()
	{
		float radius = ResolveNoRebuyRadius();
		if (radius <= 0)
			return false;

		if (!m_ParentDeployment)
			return false;

		return DefenderWithin(m_ParentDeployment.GetPosition(), radius);
	}

	//------------------------------------------------------------------------------------------------
	//! GATE TWO: has this deployment lost men recently enough that it is still in contact?
	//!
	//! See m_fContactCooldown - this is the half that covers an attacker no radius can reach, and the
	//! half that makes sustained pressure actually wear a position down.
	//! \return True when the rebuy must be skipped this update.
	protected bool IsRebuyBlockedByContact()
	{
		if (m_fContactCooldown <= 0)
			return false;

		if (m_fLastContactTime <= 0)
			return false;

		return GetGame().GetWorld().GetWorldTime() - m_fLastContactTime < m_fContactCooldown;
	}

	//------------------------------------------------------------------------------------------------
	//! ONE OBSERVATION OF THE FORCE'S STRENGTH, and the casualty signal is the DIFF between two of them.
	//!
	//! ==========================================================================================
	//! WHY A SAMPLE-AND-DIFF AND NOT AN EVENT, which was the first instinct.
	//! ==========================================================================================
	//! The framework's only casualty event is OnVirtualGroupWiped(), which fires when a WHOLE GROUP is
	//! gone - a garrison that loses three of its four men never raises it, and those three men are
	//! exactly the contact this gate exists to notice. The per-member signal lives inside the
	//! virtualization core (ReportMemberKilled) and is not published to deployments. So the cheapest
	//! honest hook is to read the number that already exists and watch it fall.
	//!
	//! ⚠ IT READS THE SURVIVOR MASK, NEVER AN AGENT COUNT. GetAliveMemberCount() answers off core's
	//! per-slot mask, so a DORMANT garrison - nobody near it, no bodies in the world - reports its real
	//! strength instead of zero. Counting agents here would read every unobserved deployment in the
	//! campaign as freshly wiped, every update.
	//!
	//! ⚠ SAMPLED ON EVERY DEPLOYMENT UPDATE (~10 s), DELIBERATELY BEFORE THE CHECK INTERVAL GATE. The
	//! reinforcement check itself only runs once a minute; sampling at that cadence would put up to a
	//! minute of error on a ninety-second cooldown.
	//!
	//! ⚠ A RISE RE-BASELINES SILENTLY. The count goes UP when a rebuy lands, and that is not news.
	//! ⚠ AND THE ONE FALSE POSITIVE IS NAMED RATHER THAN GUARDED: unregistering a group at teardown also
	//! drops the count, so a deployment being collected books one phantom "contact". It is harmless -
	//! the deployment is going away and will never ask to reinforce again - and guarding it would mean
	//! telling casualties and teardowns apart from outside, which is more machinery than the fault is
	//! worth.
	protected void SampleCasualties()
	{
		if (!m_ParentDeployment)
			return;

		int alive = CountAliveForce();

		if (m_iLastAliveSample >= 0 && alive < m_iLastAliveSample)
			m_fLastContactTime = GetGame().GetWorld().GetWorldTime();

		m_iLastAliveSample = alive;
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many men this deployment's whole force still has, off the survivor mask.
	protected int CountAliveForce()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return 0;

		array<int> handles = new array<int>();

		array<OVT_BaseSpawningDeploymentModule> spawningModules = m_ParentDeployment.GetSpawningModules();
		foreach (OVT_BaseSpawningDeploymentModule spawningModule : spawningModules)
		{
			if (spawningModule)
				spawningModule.CollectRegisteredHandles(handles);
		}

		int alive = 0;

		foreach (int handle : handles)
		{
			if (!virtualization.IsRegistered(handle))
				continue;

			alive += virtualization.GetAliveMemberCount(handle);
		}

		return alive;
	}

	//------------------------------------------------------------------------------------------------
	protected bool EvaluateReinforcementConditions()
	{		
		// Check town size requirement if enabled
		if (m_bRequireTownSizeCheck)
		{
			OVT_TownConditionalDeploymentModule townCondition = OVT_TownConditionalDeploymentModule.Cast(
				m_ParentDeployment.GetModule(OVT_TownConditionalDeploymentModule)
			);
			
			if (townCondition)
			{
				// This is a town deployment, check town size
				OVT_TownData nearestTown = townCondition.GetNearestTown();
				if (!nearestTown || nearestTown.size <= 1)
				{
					return false;
				}
			}
		}
		
		// Check all condition modules
		array<OVT_BaseConditionDeploymentModule> conditionModules = m_ParentDeployment.GetConditionModules();
		
		foreach (OVT_BaseConditionDeploymentModule conditionModule : conditionModules)
		{
			if (!conditionModule.EvaluateCondition())
			{
				Print(string.Format("Reinforcement denied: Condition module %1 failed", conditionModule.Type().ToString()), LogLevel.VERBOSE);
				return false;
			}
		}
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool ShouldReinforceModule(OVT_BaseSpawningDeploymentModule spawningModule)
	{
		// Only reinforce if the spawning module is completely eliminated
		return spawningModule.AreSpawnedUnitsEliminated();
	}
	
	//------------------------------------------------------------------------------------------------
	protected int GetMissingUnitsCount(OVT_BaseSpawningDeploymentModule spawningModule)
	{
		// When reinforcing eliminated units, we need to reinforce the full capacity
		OVT_InfantrySpawningDeploymentModule infantryModule = OVT_InfantrySpawningDeploymentModule.Cast(spawningModule);
		if (infantryModule)
		{
			return infantryModule.GetMaxGroupCount();
		}
		
		// For other spawning module types, add support here
		return 0;
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool CanAffordReinforcement(OVT_BaseSpawningDeploymentModule spawningModule, int unitsNeeded)
	{
		OVT_InfantrySpawningDeploymentModule infantryModule = OVT_InfantrySpawningDeploymentModule.Cast(spawningModule);
		if (infantryModule)
		{
			return infantryModule.CanReinforce(unitsNeeded);
		}
		
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool TryReinforceModule(OVT_BaseSpawningDeploymentModule spawningModule, int unitsNeeded)
	{
		OVT_InfantrySpawningDeploymentModule infantryModule = OVT_InfantrySpawningDeploymentModule.Cast(spawningModule);
		if (infantryModule)
		{
			bool success = infantryModule.Reinforce(unitsNeeded);
			if (success)
			{
				Print(string.Format("Reinforcement behavior: Successfully reinforced %1 with %2 groups", 
					infantryModule.Type().ToString(), unitsNeeded), LogLevel.NORMAL);
			}
			else
			{
				Print(string.Format("Reinforcement behavior: Failed to reinforce %1", 
					infantryModule.Type().ToString()), LogLevel.WARNING);
			}
			return success;
		}
		
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Takes the deployment down because the thing it existed for is no longer true.
	//!
	//! ⚠ CollectDeployment, NOT DeleteDeployment (2026-08-20). A condition failing is not "this force no
	//! longer exists" - it is "the REASON for this force no longer exists", and the men are very often
	//! still standing when it happens. The clearest case is the one that prompted the change: a recapture
	//! team takes its radio tower, the tower-control condition it was deployed under turns false, and this
	//! is the path that collects it. Under DeleteDeployment the team that had just SUCCEEDED was written
	//! off in full - author, play-test: "they took the radio tower, but was not collected due to 'Failed
	//! conditions'".
	//!
	//! ⚠ IT DOES NOT MAKE EVERY CONDITION-FAIL PAY OUT, WHICH IS THE OBJECTION IT HAS TO ANSWER.
	//! CollectDeployment refunds only groups still at FULL strength, so the common condition-fail - a
	//! tower or base garrison whose location changed hands, which is how it changed hands - has no intact
	//! groups and refunds exactly nothing. What it does pay for is a force that came through whatever
	//! ended its posting untouched, which is the same rule the behaviour modules' own success path uses.
	//! See OVT_DeploymentManagerComponent.CollectDeployment for what is and is not returned.
	//!
	//! ⚠ AND IT IS WHY THE RECAPTURE MODULE DOES NOT REQUEST ITS OWN COLLECTION. It runs before this
	//! module in every config that carries both, so its deferred one-frame request would be beaten by
	//! this module's INLINE delete in the same pass and would find the deployment already gone. Fixing
	//! the teardown that actually runs is the only arrangement in which the refund cannot be raced.
	protected void RequestDeploymentDeletion()
	{
		if (!m_ParentDeployment)
			return;

		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (manager)
		{
			Print(string.Format("Requesting deletion of deployment %1 due to failed conditions",
				m_ParentDeployment.GetDeploymentName()), LogLevel.NORMAL);
			manager.CollectDeployment(m_ParentDeployment);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	// Debug methods
	//------------------------------------------------------------------------------------------------
	void PrintDebugInfo()
	{
		Print(string.Format("Reinforcement Behavior Module: %1", m_sModuleName));
		
		string enabled = "No";
		if (m_bEnableReinforcement)
			enabled = "Yes";
		Print(string.Format("  Enabled: %1", enabled));
		Print(string.Format("  Check Interval: %1s", m_fCheckInterval));
		Print("  Reinforcement Trigger: Only when units eliminated");
		
		string townSizeCheck = "No";
		if (m_bRequireTownSizeCheck)
			townSizeCheck = "Yes";
		Print(string.Format("  Town Size Check: %1", townSizeCheck));
		
		string deleteOnFail = "No";
		if (m_bDeleteOnConditionFail)
			deleteOnFail = "Yes";
		Print(string.Format("  Delete on Fail: %1", deleteOnFail));
		
		float currentTime = GetGame().GetWorld().GetWorldTime();
		Print(string.Format("  Time since activation: %1s", currentTime - m_fActivationTime));
		
		if (m_fLastReinforcementTime > 0)
		{
			Print(string.Format("  Time since last reinforcement: %1s", currentTime - m_fLastReinforcementTime));
		}
		else
		{
			Print("  No reinforcements yet");
		}
	}
}