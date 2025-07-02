[EntityEditorProps(description: "A position for vehicle patrols to spawn at", dynamicBox: true)]
class OVT_VehiclePatrolSpawnClass : GenericEntityClass
{

}

//! A position for the start game camera in single player
class OVT_VehiclePatrolSpawn : GenericEntity
{
#ifdef WORKBENCH
	override void _WB_GetBoundBox(inout vector min, inout vector max, IEntitySource src)
	{
		min = "-1 0 -2";
		max = "1 2 2";
	}
    protected ref Shape m_aDirectionArrow;
	
	//Draw attack preferred direction as arrows showing variance
	override int _WB_GetAfterWorldUpdateSpecs(IEntitySource src)
	{
		return EEntityFrameUpdateSpecs.CALL_WHEN_ENTITY_VISIBLE;
	}
	
	override void _WB_AfterWorldUpdate(float timeSlice)
	{
        vector centerPos = GetOrigin();
        
        // Draw arrow in the direction the entity is facing
        vector transform[4];
        GetTransform(transform);
        
        vector fromCenter = centerPos + Vector(0, 2, 0);
        vector toCenter = centerPos + transform[2] * 4 + Vector(0, 2, 0);
        m_aDirectionArrow = Shape.CreateArrow(fromCenter, toCenter, 1, Color.FromRGBA(255, 0, 0, 255).PackToInt(), ShapeFlags.ONCE | ShapeFlags.NOZBUFFER | ShapeFlags.TRANSP | ShapeFlags.DOUBLESIDE | ShapeFlags.NOOUTLINE);
		
		super._WB_AfterWorldUpdate(owner, timeSlice);
	}
#endif
}