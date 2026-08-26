//! Draws a dashed line from each High Command group to its destination, until it arrives.
//!
//! A CANVAS layer, not a widget one: OVT_MapHighCommandLayer owns the marker widgets and cannot draw
//! between two points. Both read the SAME records, so this layer never dereferences a group entity and
//! never talks to its sibling - the record is the shared truth.
[BaseContainerProps()]
class OVT_MapHighCommandLineLayer : OVT_MapCanvasLayer
{
	protected const float DASH_LENGTH_PX = 12.0;
	protected const float GAP_LENGTH_PX = 8.0;
	//! 4 px, not 2: at 2 the dashes were hard to pick out against terrain (play-test 2026-08-22).
	protected const float LINE_WIDTH_PX = 4.0;

	//! 0.45 * 255 - matches OVT_MapHighCommandLayer.OTHER_OWNER_OPACITY, so a line dims with its marker.
	protected const int OTHER_OWNER_ALPHA = 115;

	protected string m_sLocalPersistentId;

	//------------------------------------------------------------------------------------------------
	override void Draw()
	{
		if (!m_Commands)
			m_Commands = new array<ref CanvasWidgetCommand>();

		m_Commands.Clear();

		OVT_HighCommandManagerComponent highCommand = OVT_Global.GetHighCommand();
		if (!highCommand || !highCommand.m_mGroups)
			return;

		if (m_sLocalPersistentId.IsEmpty())
			m_sLocalPersistentId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(SCR_PlayerController.GetLocalPlayerId());

		CacheProjection();

		for (int i = 0; i < highCommand.m_mGroups.Count(); i++)
		{
			OVT_HighCommandRecord record = highCommand.m_mGroups.GetElement(i);
			if (!record)
				continue;

			if (!ShouldDraw(record))
				continue;

			int alpha = 255;
			if (record.m_sOwnerPersistentId != m_sLocalPersistentId)
				alpha = OTHER_OWNER_ALPHA;

			// PackColor, never Color.PackToInt - the latter bakes the palette colour's own alpha and
			// would ignore the dimming above.
			int packed = GetFactionPalette().GetArgbForRole(OVT_FactionType.RESISTANCE_FACTION, alpha);

			EmitDashedLine(record.m_vLastKnownPosition, record.m_vDestination, packed);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Whether this group still has a journey worth drawing.
	//! \param[in] record The group.
	//! \return True when it has a legal destination it has not reached.
	protected bool ShouldDraw(notnull OVT_HighCommandRecord record)
	{
		if (!OVT_HighCommandRules.IsDestinationLegal(record.m_vDestination))
			return false;

		// The SAME arrival test the status flags use, so the line disappears on the tick the marker
		// stops reading "moving" rather than a frame either side of it.
		return OVT_HighCommandRules.IsMoving(record.m_vLastKnownPosition, record.m_vDestination);
	}

	//------------------------------------------------------------------------------------------------
	//! One dashed polyline between two world points, in screen space.
	//!
	//! Dash length is a SCREEN quantity, so the rhythm stays readable at every zoom instead of
	//! collapsing to a solid line when zoomed out.
	//! \param[in] fromWorld Start.
	//! \param[in] toWorld End.
	//! \param[in] colour Packed ARGB.
	protected void EmitDashedLine(vector fromWorld, vector toWorld, int colour)
	{
		int startX, startY, endX, endY;
		ProjectWorld(fromWorld[0], fromWorld[2], startX, startY);
		ProjectWorld(toWorld[0], toWorld[2], endX, endY);

		float dx = endX - startX;
		float dy = endY - startY;
		float length = Math.Sqrt(dx * dx + dy * dy);
		if (length < 1)
			return;

		float ux = dx / length;
		float uy = dy / length;

		float step = DASH_LENGTH_PX + GAP_LENGTH_PX;
		float travelled = 0;

		while (travelled < length)
		{
			float dashEnd = travelled + DASH_LENGTH_PX;
			if (dashEnd > length)
				dashEnd = length;

			EmitSegment(colour, LINE_WIDTH_PX,
				startX + ux * travelled, startY + uy * travelled,
				startX + ux * dashEnd, startY + uy * dashEnd);

			travelled += step;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! One two-vertex line command, in screen pixels. LineDrawCommand.m_Vertices is a flat POLYLINE, so
	//! one command per dash is the only way to leave gaps.
	//! \param[in] colour Packed ARGB.
	//! \param[in] width Line width in screen pixels.
	//! \param[in] startX Screen X of the first vertex.
	//! \param[in] startY Screen Y of the first vertex.
	//! \param[in] endX Screen X of the second vertex.
	//! \param[in] endY Screen Y of the second vertex.
	protected void EmitSegment(int colour, float width, float startX, float startY, float endX, float endY)
	{
		LineDrawCommand cmd = new LineDrawCommand();
		cmd.m_iColor = colour;
		cmd.m_fWidth = width;
		cmd.m_fOutlineWidth = 0;

		cmd.m_Vertices = new array<float>();
		cmd.m_Vertices.Insert(startX);
		cmd.m_Vertices.Insert(startY);
		cmd.m_Vertices.Insert(endX);
		cmd.m_Vertices.Insert(endY);

		m_Commands.Insert(cmd);
	}
}
