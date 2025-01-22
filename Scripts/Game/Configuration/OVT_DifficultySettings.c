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
	[Attribute(defvalue: "3000", desc: "OF starting resources", category: "Occupying Faction")]
	int startingResources;
	[Attribute(defvalue: "250", desc: "OF resources per 6 hrs", category: "Occupying Faction")]
	int baseResourcesPerTick;
	[Attribute(defvalue: "500", desc: "Additional OF resources per 6 hrs (* threat)", category: "Occupying Faction")]
	int resourcesPerTick;
	[Attribute(defvalue: "10", desc: "Base resource cost", category: "Occupying Faction")]
	int baseResourceCost;	
	[Attribute(defvalue: "1500", desc: "Radio Tower Range", category: "Occupying Faction")]
	float radioTowerRange;
	[Attribute(defvalue: "750", desc: "Base Support Range", category: "Occupying Faction")]
	float baseSupportRange;
	[Attribute(defvalue: "2", desc: "Minimum number of defense groups at radio towers", category: "Occupying Faction")]
	int radioTowerGroupsMin;
	[Attribute(defvalue: "3", desc: "Maximum number of defense groups at radio towers", category: "Occupying Faction")]
	int radioTowerGroupsMax;	
	[Attribute("280")]
	float baseRange;
	[Attribute("220")]
	float baseCloseRange;
	[Attribute("120")]
	float counterAttackTimeout;
	
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
	[Attribute(defvalue: "0.5", desc: "Multiplier when selling to a gun dealer", category: "Economy")]
	float gunDealerSellPriceMultiplier;
	[Attribute(defvalue: "0.8", desc: "Multiplier when buying vehicles at an owned base", category: "Economy")]
	float procurementMultiplier;
	
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
	
	[Attribute(defvalue: "1000", desc: "Max size of QRF in resources", category: "QRF")]
	int maxQRF;
	
	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Items given to player when first spawned in")]
	ref array<ResourceName> startingItems;
}