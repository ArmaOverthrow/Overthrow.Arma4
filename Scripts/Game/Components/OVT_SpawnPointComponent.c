[EntityEditorProps(category: "Overthrow", description: "Defines where to spawn a player, must be placed on a house entity", color: "0 0 255 255")]
class OVT_SpawnPointComponentClass: ScriptComponentClass
{
};

class OVT_SpawnPointComponent : ScriptComponent
{
	[Attribute()]
	ref PointInfo m_vPoint;
		
	vector GetSpawnPoint()
	{		
		vector outMat[4];
		vector offsetMat[4];
					
		//Get building transform
		GetOwner().GetTransform(outMat);
		if(!m_vPoint) return outMat[3];
		
		m_vPoint.GetTransform(offsetMat);
		
		// offset the item locally with building rotation
		outMat[3] = offsetMat[3].Multiply4(outMat);
		
		//Set ground height to Y + 1m
		outMat[3][1] = GetGame().GetWorld().GetSurfaceY(outMat[3][0],outMat[3][2]) + 1;	
		
		return outMat[3];
	}	
}