[EntityEditorProps(category: "Overthrow", description: "Defines where to park a car", color: "0 0 255 255")]
class OVT_ParkingComponentClass: ScriptComponentClass
{
};

class OVT_ParkingComponent : ScriptComponent
{
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_ParkingPointInfo> m_aParkingSpots;
	
	protected bool m_bFoundObstacle;
	
	bool GetParkingSpot(out vector outMat[4], OVT_ParkingType type = OVT_ParkingType.PARKING_CAR, bool skipObstructionCheck = false)
	{
		if(m_aParkingSpots.Count() == 0) return false;
		
		foreach(OVT_ParkingPointInfo point : m_aParkingSpots)
		{
			if(point.m_Type != type) continue;
			
			//Get building transform
			GetOwner().GetTransform(outMat);
			
			// offset the item locally with building rotation
			outMat[3] = point.m_vPosition.Multiply4(outMat);
			
			//Set ground height to Y + 1m
			outMat[3][1] = GetGame().GetWorld().GetSurfaceY(outMat[3][0],outMat[3][2]) + 1;	
			
			//Add PointInfo Yaw (ignore pitch and roll)
			vector angles = Math3D.MatrixToAngles(outMat);
			angles[0] = angles[0] + point.m_vAngles[1];
			Math3D.AnglesToMatrix(angles, outMat);
					
			if(!skipObstructionCheck)
			{
				//Check for obstructions
				vector mins = "-1 -1 -1";
				vector maxs = "1 1 1";
				autoptr TraceBox trace = new TraceBox;
				trace.Flags = TraceFlags.ENTS;
				trace.Start = outMat[3];
				trace.Mins = mins;
				trace.Maxs = maxs;
				trace.Exclude = GetOwner();
				
				float result = GetOwner().GetWorld().TracePosition(trace, null);
					
				if (result < 0)
				{				
					continue;
				}			
			}
			
			return true;
		}		
		return false;
	}	
}

enum OVT_ParkingType
{
	PARKING_CAR,
	PARKING_TRUCK,
	PARKING_LIGHT,
	PARKING_HEAVY,
	PARKING_HELI,
	PARKING_PLANE
}

class OVT_ParkingPointInfo : ScriptAndConfig
{
	[Attribute("0 0 0", UIWidgets.EditBox, desc: "Parking Position", params: "inf inf 0 purposeCoords spaceEntity")]
	vector m_vPosition;
	
	[Attribute("0 0 0")]
	vector m_vAngles;
	
	[Attribute("1", UIWidgets.ComboBox, "Parking type", "", ParamEnumArray.FromEnum(OVT_ParkingType) )]
	OVT_ParkingType m_Type;
	
	
}