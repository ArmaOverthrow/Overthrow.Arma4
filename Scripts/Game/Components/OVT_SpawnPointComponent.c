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

	//! Clearance boxes an authored point is tested against before it is handed out. Same dimensions the
	//! WORKBENCH preview draws, so what the author sees is what gets validated.
	static const vector PERSON_MINS = "-0.5 0 -0.5";
	static const vector PERSON_MAXS = "0.5 2 0.5";
	static const vector VEHICLE_MINS = "-1.5 0 -3";
	static const vector VEHICLE_MAXS = "1.5 2.5 3";

#ifdef WORKBENCH
	protected ref array<ref Shape> m_aSpawnBoxes = {};
#endif
		
	//! A world position at one of the authored pedestrian points, VALIDATED.
	//!
	//! The points are tried in a RANDOM ORDER and the first one clear of geometry wins, which is why
	//! this can no longer just pick a random element: a base controller, a deployed FOB and a bus stop
	//! each author fixed offsets on all four sides, and which of them is inside a wall depends entirely
	//! on where the owner happens to have been placed. Picking blind put players in that wall. Only
	//! when every point is blocked does the first one TRIED come back anyway - a blocked spawn still
	//! beats no answer, and one picked at random is exactly what this method always used to return.
	//!
	//! Height comes from OVT_WorldUtils.ResolveGroundY, not GetSurfaceY: the terrain height ignores
	//! the raised floor, foundation or pad the point was authored on and buries the spawn in it.
	//! \return A world position, or the owner's origin when nothing is authored (see HasSpawnPoints).
	vector GetSpawnPoint()
	{
		vector ownerMat[4];
		GetOwner().GetTransform(ownerMat);

		array<PointInfo> candidates();
		CollectPoints(m_aPoints, m_vPoint, candidates);

		if (candidates.IsEmpty())
			return ownerMat[3];

		vector fallback = vector.Zero;
		int count = candidates.Count();
		int start = s_AIRandomGenerator.RandInt(0, count);

		for (int i = 0; i < count; i++)
		{
			vector candidate = ResolvePointPosition(candidates[(start + i) % count], ownerMat);

			if (i == 0)
				fallback = candidate;

			// The owner is excluded on purpose: an authored point sits ON or ALONGSIDE the thing that
			// authored it (a truck's flank, a tent's doorway), so counting the owner would reject every
			// point on a large prefab and fall through to the blocked-anyway fallback.
			if (OVT_WorldUtils.IsPositionClear(candidate, PERSON_MINS, PERSON_MAXS, GetOwner()))
				return candidate;
		}

		return fallback;
	}

	//------------------------------------------------------------------------------------------------
	//! One authored point in world space, standing on the surface under it.
	protected vector ResolvePointPosition(notnull PointInfo point, vector ownerMat[4])
	{
		vector offsetMat[4];
		point.GetTransform(offsetMat);

		// offset the item locally with building rotation
		vector worldPos = offsetMat[3].Multiply4(ownerMat);
		worldPos[1] = OVT_WorldUtils.ResolveGroundY(worldPos) + OVT_WorldUtils.SPAWN_GROUND_CLEARANCE;

		return worldPos;
	}

	//------------------------------------------------------------------------------------------------
	//! The authored points as one list: the array when it holds anything, else the single point.
	protected void CollectPoints(array<ref PointInfo> points, PointInfo single, notnull array<PointInfo> outPoints)
	{
		outPoints.Clear();

		if (points)
		{
			foreach (PointInfo point : points)
			{
				if (point)
					outPoints.Insert(point);
			}
		}

		if (outPoints.IsEmpty() && single)
			outPoints.Insert(single);
	}
	
	//! A world transform at one of the authored vehicle points, VALIDATED the same way GetSpawnPoint is.
	//!
	//! An occupied arrival spot is worse for a vehicle than for a person - the car lands on top of
	//! whatever is parked there - so a blocked point is skipped rather than used, and the first point
	//! tried is only handed back when every one of them is blocked.
	bool GetVehicleSpawnPoint(out vector position, out vector angles)
	{
		vector ownerMat[4];
		GetOwner().GetTransform(ownerMat);

		array<PointInfo> candidates();
		CollectPoints(m_aVehiclePoints, null, candidates);

		if (candidates.IsEmpty())
			return false;

		vector fallbackPos = vector.Zero;
		vector fallbackAngles = vector.Zero;
		int count = candidates.Count();
		int start = s_AIRandomGenerator.RandInt(0, count);

		for (int i = 0; i < count; i++)
		{
			PointInfo point = candidates[(start + i) % count];

			vector offsetMat[4];
			point.GetTransform(offsetMat);

			vector outMat[4];
			GetOwner().GetTransform(outMat);

			// offset the item locally with building rotation
			outMat[3] = offsetMat[3].Multiply4(ownerMat);

			// Apply rotation (QuatToMatrix writes the 3x3 only, so the position above survives)
			float qt[4];
			float q[4];
			Math3D.MatrixToQuat(ownerMat, qt);
			Math3D.MatrixToQuat(offsetMat, q);
			Math3D.QuatMultiply(qt, q, qt);
			Math3D.QuatToMatrix(qt, outMat);

			vector spot = outMat[3];
			spot[1] = OVT_WorldUtils.ResolveGroundY(spot) + OVT_WorldUtils.SPAWN_GROUND_CLEARANCE;
			outMat[3] = spot;

			vector spotAngles = Math3D.MatrixToAngles(outMat);

			if (i == 0)
			{
				fallbackPos = spot;
				fallbackAngles = spotAngles;
			}

			if (OVT_WorldUtils.IsPositionClear(spot, VEHICLE_MINS, VEHICLE_MAXS, GetOwner()))
			{
				position = spot;
				angles = spotAngles;
				return true;
			}
		}

		position = fallbackPos;
		angles = fallbackAngles;
		return true;
	}
	
	bool HasVehicleSpawnPoints()
	{
		return m_aVehiclePoints && m_aVehiclePoints.Count() > 0;
	}
	
	//! GetSpawnPoint() falls back to the HOLDER'S OWN ORIGIN when nothing is authored, which for a
	//! building is a point inside it. Callers that have somewhere else to put things must ask first.
	bool HasSpawnPoints()
	{
		if (m_aPoints && m_aPoints.Count() > 0)
			return true;
		
		if (m_vPoint)
			return true;
		
		return false;
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
			// Vehicle spawn point dimensions (larger) - the box the runtime clearance test uses
			mins = VEHICLE_MINS;
			maxs = VEHICLE_MAXS;
			// Orange color for vehicle spawn points
			color = Color.FromRGBA(255, 165, 0, 128).PackToInt();
		}
		else
		{
			// Player spawn point dimensions - the box the runtime clearance test uses
			mins = PERSON_MINS;
			maxs = PERSON_MAXS;
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