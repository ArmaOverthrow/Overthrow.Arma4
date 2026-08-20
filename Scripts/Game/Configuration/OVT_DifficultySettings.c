enum OVT_QRFFastTravelMode
{
	FREE,
	NOQRF,
	DISABLED
}
[BaseContainerProps(configRoot: true)]
class OVT_DifficultySettings : ScriptAndConfig
{	
	[Attribute()]
	string name;
	
	[Attribute()]
	string description;
	
	[Attribute()]
	bool showPlayerOnMap;
	
	//Wanted system
	[Attribute(defvalue: "30000", desc: "Timeout in ms for wanted levels 2-5 (per level)", category: "Wanted System")]
	int wantedTimeout;
	[Attribute(defvalue: "120000", desc: "Timeout in ms for wanted level 1", category: "Wanted System")]
	int wantedOneTimeout;
	
	//OF
	//! ⚠ PER BASE, NOT PER CAMPAIGN, AND THE MULTIPLICATION IS WHY THIS WAS RE-SCALED (2026-08-20).
	//! CalculateOpeningDeploymentSeed() sums `floor(startingResources * base.m_fStartingResourcesMultiplier)`
	//! over EVERY base on the map, so on a ten-base world the campaign's opening credit is roughly eleven
	//! times this number. Normal's authored 850 produced **9 940** into the deployment pool, which the
	//! author reasonably read as a leak.
	//!
	//! IT WAS NOT A LEAK - IT WAS SIZED FOR A MODEL THAT NO LONGER EXISTS. 9 940 is very close to the
	//! cost of fortifying all ten bases outright (the five non-baseline base configs total ~820 apiece),
	//! which is exactly what the deleted per-base distribution used to do with it on the first tick. Two
	//! changes since have made that wrong: base defence is now bought concern-by-concern by the evaluator
	//! as threat allows, and MIN_LOCAL_THREAT_TO_DEPLOY refuses escalation at a quiet place - and at t0
	//! EVERY place is quiet. So the old figure could not be spent when it was granted and simply banked,
	//! turning into a burst the moment the player made noise anywhere.
	//!
	//! THE NUMBERS ARE NOW SIZED AS "a few early deployments", not "fortify the map": 150 on Normal is
	//! about 1 960 into the pool, or roughly two bases' worth of escalation. The Easy -> Insane ordering
	//! and rough ratios are preserved.
	//!
	//! ⚠ THE DEFAULT WAS 3000 AND Difficulty_Insane AUTHORED NOTHING, so Insane silently inherited it and
	//! opened on a ~35 000 credit. It now authors 500 explicitly, and this default is lowered so the next
	//! preset that forgets inherits something sane rather than the largest number in the file.
	[Attribute(defvalue: "150", desc: "OF starting resources PER BASE, credited once into the deployment pool at campaign start. The campaign total is this times the number of bases (times each base's own multiplier), so a ten-base world opens on roughly eleven times this figure", category: "Occupying Faction")]
	int startingResources;
	[Attribute(defvalue: "250", desc: "OF resources per 6 hrs", category: "Occupying Faction")]
	int baseResourcesPerTick;
	[Attribute(defvalue: "500", desc: "Additional OF resources per 6 hrs (* threat)", category: "Occupying Faction")]
	int resourcesPerTick;
	[Attribute(defvalue: "10", desc: "Base resource cost", category: "Occupying Faction")]
	int baseResourceCost;	
	[Attribute(defvalue: "1500", desc: "Radio Tower Range", category: "Occupying Faction")]
	float radioTowerRange;
	[Attribute(defvalue: "1800", desc: "Radio tower sabotage downtime (seconds)", category: "Occupying Faction")]
	float radioTowerSabotageTime;
	[Attribute(defvalue: "750", desc: "Base Support Range", category: "Occupying Faction")]
	float baseSupportRange;
	[Attribute(defvalue: "2", desc: "Minimum number of defense groups at radio towers", category: "Occupying Faction")]
	int patrolGroupsMin;
	[Attribute(defvalue: "3", desc: "Maximum number of defense groups at radio towers", category: "Occupying Faction")]
	int patrolGroupsMax;	
	[Attribute(defvalue: "5", desc: "Maximum number of defense positions at bases", category: "Occupying Faction")]
	int defenseGroupsBaseMax;
	[Attribute("280", category: "Occupying Faction")]
	float baseRange;
	[Attribute("220", category: "Occupying Faction")]
	float baseCloseRange;
	//------------------------------------------------------------------------------------------------
	// OBJECTIVE DIRECTOR (occupying/counter-attacks)
	//
	// These replace the retired counter-attack cooldown field. The occupying faction no longer rolls a
	// random hourly counter-attack; it picks ONE objective and works toward it in three phases, and
	// every knob below tunes some part of that ramp. Larger numbers generally mean a SLOWER, more
	// readable build-up - except objectiveSabotageMissionsRequired, which is deliberately INVERTED
	// (easier difficulties demand MORE successful sabotage missions, so a new player gets more
	// warning before the counter-attack lands).
	//------------------------------------------------------------------------------------------------
	[Attribute(defvalue: "45", desc: "In-game minutes between operations sent at the current objective. Lower = the occupying faction presses harder", category: "Occupying Faction")]
	int objectiveHarassmentIntervalMinutes;
	[Attribute(defvalue: "2", desc: "Maximum harassment/sabotage operations alive at the current objective at once", category: "Occupying Faction")]
	int objectiveHarassmentMaxConcurrent;
	[Attribute(defvalue: "180", desc: "Seconds a harassment group must hold a town centre before it applies its support debuff", category: "Occupying Faction")]
	int objectiveHarassmentHoldSeconds;
	[Attribute(defvalue: "4", desc: "Successful sabotage missions required before a base objective may be counter-attacked. INVERTED: easier difficulties require MORE, giving the player more warning", category: "Occupying Faction")]
	int objectiveSabotageMissionsRequired;
	[Attribute(defvalue: "120", desc: "Seconds a sabotage team must hold a base uncontested before it demolishes a structure", category: "Occupying Faction")]
	int objectiveSabotageHoldSeconds;
	[Attribute(defvalue: "2", desc: "Structures demolished per completed sabotage mission (smallest first)", category: "Occupying Faction")]
	int objectiveSabotageStructuresPerMission;
	[Attribute(defvalue: "600", desc: "Seconds a specops team must hold a radio tower before control flips back to the occupying faction", category: "Occupying Faction")]
	int objectiveTowerRecaptureHoldSeconds;
	[Attribute(defvalue: "3", desc: "Maximum garrison groups at an occupying forward operating base", category: "Occupying Faction")]
	int objectiveFOBGarrisonMax;
	[Attribute(defvalue: "400", desc: "Resource cost of raising an occupying forward operating base. Its spend ceiling is three times this, and dismantling one costs the occupying faction this much again", category: "Occupying Faction")]
	int objectiveFOBCost;
	[Attribute(defvalue: "2", desc: "Maximum live insertion convoys (trucks driving reinforcements to an objective) at once", category: "Occupying Faction")]
	int objectiveMaxConcurrentInsertions;
	[Attribute(defvalue: "30", desc: "In-game minutes an occupying FOB may stay cut off (source base lost, garrison dead or players camped on it) before it is abandoned", category: "Occupying Faction")]
	int objectiveStarvationMinutes;
	[Attribute(defvalue: "1500", desc: "Reserve resources the occupying faction must hold before it will launch the counter-attack QRF", category: "Occupying Faction")]
	int objectiveQRFResourceGate;
	
	//Economy
	[Attribute(defvalue: "100", desc: "Player starting cash", category: "Economy")]
	int startingCash;
	[Attribute(defvalue: "5", desc: "Money taken from player per respawn", category: "Economy")]
	int respawnCost;
	[Attribute(defvalue: "5", desc: "Cost to fast travel", category: "Economy")]
	int fastTravelCost;
	[Attribute(defvalue: "1", desc: "Cost of placeables is multiplied by this value", category: "Economy")]
	float placeableCostMultiplier;
	[Attribute(defvalue: "1", desc: "Cost of buildables is multiplied by this value", category: "Economy")]
	float buildableCostMultiplier;
	//! Repairing a ruined structure costs its build price multiplied by this again, so a repair is
	//! never dearer than a rebuild. Authored explicitly in all five ladder presets - nothing depends
	//! on this default (implementation.md D11/G6).
	[Attribute(defvalue: "0.5", desc: "Repair cost as a fraction of the build cost", category: "Economy")]
	float repairCostMultiplier;
	[Attribute(defvalue: "0.5", desc: "Cost of Real Estate is multiplied by this value", category: "Economy")]
	float realEstateCostMultiplier;
	[Attribute(defvalue: "10", desc: "Donation income per civilian supporter", category: "Economy")]
	int donationIncome;
	[Attribute(defvalue: "25", desc: "Tax income per civilian", category: "Economy")]
	int taxIncome;
	[Attribute(defvalue: "5", desc: "Bus ticket price per km", category: "Economy")]
	int busTicketPrice;
	[Attribute(defvalue: "250", desc: "Base price for AI recruit", category: "Economy")]
	int baseRecruitCost;
	//! Convenience fee on the gear when buying an already-equipped recruit at a recruitment tent.
	//! The kit is priced item-by-item at the local shop buy price and multiplied by this; the ordinary
	//! tent recruit cost is then added on top. SERVER-SIDE ONLY - it is never replicated, because a
	//! client cannot price a loadout in the first place (its copy has no items) and the server quotes.
	//! A value of 0 or less is treated as UNSET and resolves to 1.5 - see
	//! OVT_RecruitPurchaseRules.ResolveFeeMultiplier: free gear on a money path is not a supported
	//! setting, and 0.01 is available for anyone who wants it to be nearly free.
	[Attribute(defvalue: "1.5", desc: "Gear price multiplier when buying an equipped recruit at a tent (0 = use default 1.5)", category: "Economy")]
	float recruitLoadoutFeeMultiplier;
	[Attribute(defvalue: "0.5", desc: "Multiplier when selling to a gun dealer", category: "Economy")]
	float gunDealerSellPriceMultiplier;
	[Attribute(defvalue: "0.8", desc: "Multiplier when buying vehicles at an owned base", category: "Economy")]
	float procurementMultiplier;
	[Attribute(defvalue: "1.0", desc: "Multiplier for vehicle purchase prices", category: "Economy")]
	float vehiclePriceMultiplier;
	[Attribute(defvalue: "1.0", desc: "Fuel price per litre at static fuel stations (0 disables fuel charging)", category: "Economy")]
	float fuelPricePerLitre;

	//RF
	[Attribute(defvalue: "0", desc: "Base RF threat", category: "Resistance Faction")]
	int baseThreat;	
	
	[Attribute(defvalue: "0.004", desc: "Threat reduction factor", category: "Resistance Faction")]
	float threatReductionFactor;	
	
	[Attribute(defvalue: "500", desc: "Minimum fast travel distance", category: "Resistance Faction")]
	float minFastTravelDistance;
	
	//QRF
	[Attribute("1", UIWidgets.ComboBox, "QRF Fast Travel Mode", "", ParamEnumArray.FromEnum(OVT_QRFFastTravelMode), category: "QRF" )]
	OVT_QRFFastTravelMode QRFFastTravelMode;
	
	[Attribute(defvalue: "100", desc: "QRF Points needed to win a battle (lower = shorter battles)", category: "QRF")]
	int QRFPointsToWin;

	[Attribute(defvalue: "1", desc: "Deployed FOBs stay valid respawn and fast-travel destinations during an active QRF, overriding QRF Fast Travel Mode (a FOB is the forward spawn for exactly that battle). Turn off to make FOBs obey the QRF restrictions like every other location", category: "QRF")]
	bool allowFOBDuringQRF;
	
	[Attribute(defvalue: "1000", desc: "Max size of QRF in resources", category: "QRF")]
	int maxQRF;
	
	//Undercover System
	[Attribute(defvalue: "15", desc: "Distance at which enemies can see through disguises (meters)", category: "Undercover System")]
	float disguiseDetectionDistance;
	[Attribute(defvalue: "0.8", desc: "Base disguise effectiveness when fully disguised", category: "Undercover System")]
	float baseDisguiseEffectiveness;
	[Attribute(defvalue: "0.7", desc: "Maximum wanted level reduction from disguises", category: "Undercover System")]
	float wantedReductionMultiplier;
	[Attribute(defvalue: "0.6", desc: "Detection range multiplier when disguised", category: "Undercover System")]
	float detectionRangeMultiplier;
	
	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Items given to player when first spawned in")]
	ref array<ResourceName> startingItems;
}