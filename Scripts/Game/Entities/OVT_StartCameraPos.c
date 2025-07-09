[EntityEditorProps(category: "Overthrow", description: "A position for the start game camera in single player")]
class OVT_StartCameraPosClass : GenericEntityClass
{

}

//! A position for the start game camera in single player
class OVT_StartCameraPos : GenericEntity
{
    [Attribute(desc: "Show debug shape in World Editor even when the entity is not selected.")]
	protected bool m_bShowDebugViewCone;

#ifdef WORKBENCH
	override int _WB_GetAfterWorldUpdateSpecs(IEntitySource src)
	{
		return EEntityFrameUpdateSpecs.CALL_WHEN_ENTITY_VISIBLE;
	}

	override void _WB_AfterWorldUpdate(float timeSlice)
	{
		WorldEditorAPI api = _WB_GetEditorAPI();
		if (!api || (!api.IsEntitySelected(api.EntityToSource(this)) && !m_bShowDebugViewCone))
			return;
		
		float length = 100;
		float height = Math.Tan(70 / 2 * Math.DEG2RAD) * length;
		float width = height * (1920 / 1080);
		
		vector transform[4];
		GetTransform(transform);
		
		vector points[12];
		
		//--- Top
		points[0] = transform[3];
		points[1] = transform[3] + transform[2] * length + transform[1] * height + transform[0] * width;
		points[2] = transform[3] + transform[2] * length + transform[1] * height + transform[0] * -width;

		//--- Right
		points[3] = points[0];
		points[4] = transform[3] + transform[2] * length + transform[1] * -height + transform[0] * width;
		points[5] = points[1];

		//--- Bottom
		points[6] = points[0];
		points[7] = transform[3] + transform[2] * length + transform[1] * -height + transform[0] * -width;
		points[8] = points[4];

		//--- Left
		points[9] = points[0];
		points[10] = points[2];
		points[11] = points[7];

		Shape.CreateTris(ARGBF(0.25, 0.5, 0, 1), ShapeFlags.ONCE | ShapeFlags.TRANSP | ShapeFlags.DOUBLESIDE, points, 4);
	}
#endif
}