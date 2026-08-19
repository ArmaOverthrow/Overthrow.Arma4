[EntityEditorProps(description: "A curated site for an occupying-faction forward operating base", dynamicBox: true)]
class OVT_FOBPositionClass : GenericEntityClass
{

}

//! Editor-placeable marker entity for curated forward-operating-base sites. The queryable marker is
//! the OVT_FOBPositionComponent it carries (the component can also be attached to other entities);
//! this class only supplies the Workbench bound box and a facing arrow, so a map author can see which
//! way the structure will look and how much room it wants.
//!
//! ⚠ THE ONLY PER-FRAME SHAPE CALL PERMITTED IN THIS REPOSITORY IS Shape.CreateArrow. The CreateLines
//! family has hard-crashed Workbench twice here - with buffers held in locals AND in members - so it is
//! banned outright rather than used carefully. If a future author wants a footprint outline, the answer
//! is several arrows, not a line list.
class OVT_FOBPosition : GenericEntity
{
#ifdef WORKBENCH
	override void _WB_GetBoundBox(inout vector min, inout vector max, IEntitySource src)
	{
		// Roughly the footprint the raised structure wants, so a marker dropped against a wall looks
		// wrong in the editor rather than at runtime.
		min = "-6 0 -6";
		max = "6 4 6";
	}

	protected ref Shape m_DirectionArrow;

	override int _WB_GetAfterWorldUpdateSpecs(IEntitySource src)
	{
		return EEntityFrameUpdateSpecs.CALL_WHEN_ENTITY_VISIBLE;
	}

	override void _WB_AfterWorldUpdate(float timeSlice)
	{
		vector centerPos = GetOrigin();

		// An arrow showing which way the forward base will face.
		vector transform[4];
		GetTransform(transform);

		vector fromCenter = centerPos + Vector(0, 1, 0);
		vector toCenter = centerPos + transform[2] * 6 + Vector(0, 1, 0);
		m_DirectionArrow = Shape.CreateArrow(fromCenter, toCenter, 1, Color.FromRGBA(255, 120, 0, 255).PackToInt(), ShapeFlags.ONCE | ShapeFlags.NOZBUFFER | ShapeFlags.TRANSP | ShapeFlags.DOUBLESIDE | ShapeFlags.NOOUTLINE);

		super._WB_AfterWorldUpdate(timeSlice);
	}
#endif
}
