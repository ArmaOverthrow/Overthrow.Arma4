[EntityEditorProps(category: "Overthrow", description: "Defines where to park a car", color: "0 0 255 255")]
class OVT_ParkingComponentClass: ScriptComponentClass
{
};

class OVT_ParkingComponent : ScriptComponent
{
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_ParkingPointInfo> m_aParkingSpots;
	
	protected bool m_bFoundObstacle;
	
#ifdef WORKBENCH
	protected ref array<ref Shape> m_aParkingBoxes = {};
#endif
	
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
								
			if(!skipObstructionCheck && type != OVT_ParkingType.PARKING_HELI)
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
	
#ifdef WORKBENCH
	//------------------------------------------------------------------------------------------------
	override void _WB_SetTransform(IEntity owner, inout vector mat[4], IEntitySource src)
	{
		DrawParkingSpots(owner);
	}
	
	//------------------------------------------------------------------------------------------------
	override void _WB_OnInit(IEntity owner, inout vector mat[4], IEntitySource src)
	{
		DrawParkingSpots(owner);
	}
	
	//------------------------------------------------------------------------------------------------
	protected void DrawParkingSpots(IEntity owner)
	{
		// Clear existing shapes
		m_aParkingBoxes.Clear();
		
		if(m_aParkingSpots.Count() == 0) return;
		
		foreach(OVT_ParkingPointInfo point : m_aParkingSpots)
		{
			// Get building transform
			vector buildingMat[4];
			owner.GetTransform(buildingMat);
			
			// Get parking spot transform
			vector offsetMat[4];
			point.GetTransform(offsetMat);
			
			// Calculate world position
			vector worldPos = offsetMat[3].Multiply4(buildingMat);
			
			// Apply rotation
			float qt[4];
			float q[4];
			Math3D.MatrixToQuat(buildingMat, qt);
			Math3D.MatrixToQuat(offsetMat, q);
			Math3D.QuatMultiply(qt, q, qt);
			
			vector finalMat[4];
			Math3D.QuatToMatrix(qt, finalMat);
			finalMat[3] = worldPos;
			
			// Get dimensions based on parking type
			vector mins, maxs;
			GetParkingDimensions(point.m_Type, mins, maxs);
			
			// Create the shape
			ref Shape shape = Shape.Create(ShapeType.BBOX, GetParkingColor(point.m_Type), ShapeFlags.TRANSP | ShapeFlags.DOUBLESIDE | ShapeFlags.NOZBUFFER | ShapeFlags.WIREFRAME, mins, maxs);
			shape.SetMatrix(finalMat);
			m_aParkingBoxes.Insert(shape);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected void GetParkingDimensions(OVT_ParkingType type, out vector mins, out vector maxs)
	{
		switch(type)
		{
			case OVT_ParkingType.PARKING_CAR:
				mins = "-1.25 0 -2.5";
				maxs = "1.25 2 2.5";
				break;
			case OVT_ParkingType.PARKING_TRUCK:
				mins = "-1.5 0 -3.5";
				maxs = "1.5 3 3.5";
				break;
			case OVT_ParkingType.PARKING_LIGHT:
				mins = "-1.25 0 -2.5";
				maxs = "1.25 2 2.5";
				break;
			case OVT_ParkingType.PARKING_HEAVY:
				mins = "-2 0 -4";
				maxs = "2 4 4";
				break;
			case OVT_ParkingType.PARKING_HELI:
				mins = "-8 0 -8";
				maxs = "8 5 8";
				break;
			case OVT_ParkingType.PARKING_PLANE:
				mins = "-5 0 -10";
				maxs = "5 5 10";
				break;
			default:
				mins = "-1 0 -2";
				maxs = "1 2 2";
				break;
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected int GetParkingColor(OVT_ParkingType type)
	{
		switch(type)
		{
			case OVT_ParkingType.PARKING_CAR:
				return Color.FromRGBA(0, 255, 0, 128).PackToInt(); // Green
			case OVT_ParkingType.PARKING_TRUCK:
				return Color.FromRGBA(0, 200, 255, 128).PackToInt(); // Cyan
			case OVT_ParkingType.PARKING_LIGHT:
				return Color.FromRGBA(255, 255, 0, 128).PackToInt(); // Yellow
			case OVT_ParkingType.PARKING_HEAVY:
				return Color.FromRGBA(255, 128, 0, 128).PackToInt(); // Orange
			case OVT_ParkingType.PARKING_HELI:
				return Color.FromRGBA(255, 0, 255, 128).PackToInt(); // Magenta
			case OVT_ParkingType.PARKING_PLANE:
				return Color.FromRGBA(128, 0, 255, 128).PackToInt(); // Purple
			default:
				return Color.FromRGBA(128, 128, 128, 128).PackToInt(); // Gray
		}
		
		return Color.FromRGBA(128, 128, 128, 128).PackToInt(); // Fallback
	}
#endif
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