[EntityEditorProps(description: "A curated sniper position for base defense", dynamicBox: true)]
class OVT_SniperPositionClass : GenericEntityClass
{

}

//! Editor-placeable marker entity for curated sniper positions. The queryable marker is the
//! OVT_SniperPositionComponent it carries (the component can also be attached to other entities);
//! this class only supplies the Workbench facing arrow so map authors can aim the position.
class OVT_SniperPosition : GenericEntity
{
#ifdef WORKBENCH
	override void _WB_GetBoundBox(inout vector min, inout vector max, IEntitySource src)
	{
		min = "-1 0 -2";
		max = "1 2 2";
	}
	protected ref Shape m_aDirectionArrow;

	override int _WB_GetAfterWorldUpdateSpecs(IEntitySource src)
	{
		return EEntityFrameUpdateSpecs.CALL_WHEN_ENTITY_VISIBLE;
	}

	override void _WB_AfterWorldUpdate(float timeSlice)
	{
		vector centerPos = GetOrigin();

		// Draw an arrow showing which way the sniper team will face
		vector transform[4];
		GetTransform(transform);

		vector fromCenter = centerPos + Vector(0, 1, 0);
		vector toCenter = centerPos + transform[2] * 4 + Vector(0, 1, 0);
		m_aDirectionArrow = Shape.CreateArrow(fromCenter, toCenter, 1, Color.FromRGBA(0, 160, 255, 255).PackToInt(), ShapeFlags.ONCE | ShapeFlags.NOZBUFFER | ShapeFlags.TRANSP | ShapeFlags.DOUBLESIDE | ShapeFlags.NOOUTLINE);

		super._WB_AfterWorldUpdate(timeSlice);
	}
#endif
}
