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
			vector offsetMat[4];
			point.GetTransform(offsetMat);			
			outMat[3] = offsetMat[3].Multiply4(outMat);	
			
			float qt[4];
			float q[4];
			Math3D.MatrixToQuat(outMat, qt);
			Math3D.MatrixToQuat(offsetMat, q); 
			Math3D.QuatMultiply(qt, q, qt);
			Math3D.QuatToMatrix(qt, outMat);
								
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
	
	bool GetParkingTypes(array<OVT_ParkingType> outTypes)
	{
		foreach(OVT_ParkingPointInfo point : m_aParkingSpots)
		{
			if(!outTypes.Contains(point.m_Type))
			{
				outTypes.Insert(point.m_Type);
			}
		}		
		return true;
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

class OVT_ParkingPointInfo : PointInfo
{	
	[Attribute("1", UIWidgets.ComboBox, "Parking type", "", ParamEnumArray.FromEnum(OVT_ParkingType) )]
	OVT_ParkingType m_Type;	
}