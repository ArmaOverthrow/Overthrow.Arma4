[EntityEditorProps(category: "Overthrow", description: "Defines where to park a car", color: "0 0 255 255")]
class OVT_ParkingComponentClass: ScriptComponentClass
{
};

class OVT_ParkingComponent : ScriptComponent
{
	[Attribute("", UIWidgets.Object)]
	ref array<vector> m_aParkingSpots;
	ref array<vector> m_aParkingSpotsPacked = new array<vector>();
	
}