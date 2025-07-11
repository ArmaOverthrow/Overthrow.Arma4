[EntityEditorProps(category: "Overthrow", description: "Defines where to spawn a player, must be placed on a house entity", color: "0 0 255 255")]
class OVT_SpawnPointComponentClass: ScriptComponentClass
{
};

class OVT_SpawnPointComponent : ScriptComponent
{
	[Attribute()]
	ref PointInfo m_vPoint;
	
	[Attribute()]
	ref array<ref PointInfo> m_aPoints;
	
	[Attribute()]
	ref array<ref PointInfo> m_aVehiclePoints;

#ifdef WORKBENCH
	protected ref array<ref Shape> m_aSpawnBoxes = {};
#endif
		
	vector GetSpawnPoint()
	{		
		vector outMat[4];
		vector offsetMat[4];
					
		//Get building transform
		GetOwner().GetTransform(outMat);
		
		// Use random point from array if available
		if (m_aPoints && m_aPoints.Count() > 0)
		{
			PointInfo selectedPoint = m_aPoints.GetRandomElement();
			if (selectedPoint)
			{
				selectedPoint.GetTransform(offsetMat);
				
				// offset the item locally with building rotation
				outMat[3] = offsetMat[3].Multiply4(outMat);
				
				//Set ground height to Y + 0.5m
				outMat[3][1] = GetGame().GetWorld().GetSurfaceY(outMat[3][0],outMat[3][2]) + 0.5;	
				
				return outMat[3];
			}
		}
		
		// Fallback to single point if array is empty or not set
		if(!m_vPoint) return outMat[3];
		
		m_vPoint.GetTransform(offsetMat);
		
		// offset the item locally with building rotation
		outMat[3] = offsetMat[3].Multiply4(outMat);
		
		//Set ground height to Y + 0.5m
		outMat[3][1] = GetGame().GetWorld().GetSurfaceY(outMat[3][0],outMat[3][2]) + 0.5;	
		
		return outMat[3];
	}
	
	bool GetVehicleSpawnPoint(out vector position, out vector angles)
	{
		vector outMat[4];
		vector offsetMat[4];
					
		//Get building transform
		GetOwner().GetTransform(outMat);
		
		// Use random point from vehicle array if available
		if (m_aVehiclePoints && m_aVehiclePoints.Count() > 0)
		{
			PointInfo selectedPoint = m_aVehiclePoints.GetRandomElement();
			if (selectedPoint)
			{
				selectedPoint.GetTransform(offsetMat);
				
				// offset the item locally with building rotation
				outMat[3] = offsetMat[3].Multiply4(outMat);
				
				// Apply rotation
				float qt[4];
				float q[4];
				Math3D.MatrixToQuat(outMat, qt);
				Math3D.MatrixToQuat(offsetMat, q);
				Math3D.QuatMultiply(qt, q, qt);
				Math3D.QuatToMatrix(qt, outMat);
				
				//Set ground height to Y + 0.5m
				outMat[3][1] = GetGame().GetWorld().GetSurfaceY(outMat[3][0],outMat[3][2]) + 0.5;	
				
				position = outMat[3];
				angles = Math3D.MatrixToAngles(outMat);
				return true;
			}
		}
		
		return false;
	}
	
	bool HasVehicleSpawnPoints()
	{
		return m_aVehiclePoints && m_aVehiclePoints.Count() > 0;
	}

#ifdef WORKBENCH
	//------------------------------------------------------------------------------------------------
	override void _WB_SetTransform(IEntity owner, inout vector mat[4], IEntitySource src)
	{
		DrawSpawnPoints(owner);
	}
	
	//------------------------------------------------------------------------------------------------
	override void _WB_OnInit(IEntity owner, inout vector mat[4], IEntitySource src)
	{
		DrawSpawnPoints(owner);
	}
	
	//------------------------------------------------------------------------------------------------
	protected void DrawSpawnPoints(IEntity owner)
	{
		// Clear existing shapes
		m_aSpawnBoxes.Clear();
		
		// Draw pedestrian spawn points
		if (m_aPoints && m_aPoints.Count() > 0)
		{
			foreach(PointInfo point : m_aPoints)
			{
				if (point)
					DrawSpawnPoint(owner, point, false);
			}
		}
		else if (m_vPoint)
		{
			// Draw single spawn point as fallback
			DrawSpawnPoint(owner, m_vPoint, false);
		}
		
		// Draw vehicle spawn points
		if (m_aVehiclePoints && m_aVehiclePoints.Count() > 0)
		{
			foreach(PointInfo point : m_aVehiclePoints)
			{
				if (point)
					DrawSpawnPoint(owner, point, true);
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected void DrawSpawnPoint(IEntity owner, PointInfo point, bool isVehiclePoint = false)
	{
		// Get building transform
		vector buildingMat[4];
		owner.GetTransform(buildingMat);
		
		// Get spawn point transform
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
						
		// Create spawn point dimensions and color based on type
		vector mins, maxs;
		int color;
		
		if (isVehiclePoint)
		{
			// Vehicle spawn point dimensions (larger)
			mins = "-1.5 0 -3";
			maxs = "1.5 2.5 3";
			// Orange color for vehicle spawn points
			color = Color.FromRGBA(255, 165, 0, 128).PackToInt();
		}
		else
		{
			// Player spawn point dimensions
			mins = "-0.5 0 -0.5";
			maxs = "0.5 2 0.5";
			// Cyan color for player spawn points
			color = Color.FromRGBA(0, 200, 255, 128).PackToInt();
		}
		
		// Create the shape
		ref Shape shape = Shape.Create(ShapeType.BBOX, color, ShapeFlags.TRANSP | ShapeFlags.DOUBLESIDE | ShapeFlags.NOZBUFFER | ShapeFlags.WIREFRAME, mins, maxs);
		shape.SetMatrix(finalMat);
		m_aSpawnBoxes.Insert(shape);
	}
#endif
}