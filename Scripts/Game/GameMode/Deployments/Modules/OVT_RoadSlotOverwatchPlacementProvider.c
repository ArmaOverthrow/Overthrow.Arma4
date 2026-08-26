//------------------------------------------------------------------------------------------------
//! Posts BESIDE the road slots of the nearest base - "where a checkpoint would be, whether or not one
//! is built" - facing back at the road. This is where a base's AT section stands.
//!
//! ================== WHY THE CHECKPOINT SLOTS AND NOT A PATROL ROUTE =======================
//! From the play-test, verbatim: "the AT sections should NOT patrol the perimeter though, they should
//! be placed where checkpoints would be (whether or not there is one) but off to the side with an
//! offset". An AT team is an ambush asset: it wants the vehicle approach, which is exactly what a
//! designer marked when they placed a ROAD_MEDIUM / ROAD_LARGE slot, and it wants to be beside that
//! approach rather than standing in the middle of it.
//!
//! THE SAME TWO SLOT SETS Deployment_BaseCheckpoints.conf builds into - m_LargeRoadSlots and
//! m_MediumRoadSlots on the base controller - and DELIBERATELY WITHOUT consulting m_aSlotsFilled: the
//! ask is "where checkpoints WOULD be", so a base that has not bought its checkpoints yet, or has
//! bought them all, offers the AT section the same posts either way. Nothing here claims a slot, so
//! this provider can never stop a checkpoint being built later.
//! =========================================================================================
//!
//! ================== THE SIDE IS A FUNCTION OF THE SLOT, NOT OF THE ORDER ==================
//! Placement stability across re-materialisations is a promise of the placed-infantry module, and
//! this provider is asked again on every convergence pass, after every load and after every
//! re-discovery of a base's slots. So the left/right pick must not come from the order slots happen
//! to be returned in: a slot destroyed, a query returning in a different order, or a road slot list
//! rebuilt at InitBase would flip a team from one side of the road to the other for no reason a
//! player could understand.
//!
//! SideForSlot() therefore folds the slot's own rounded world X+Z to a parity. That is stable across
//! everything above (a slot does not move), it is pure, and it alternates in practice because
//! neighbouring slots rarely share a parity - which is the "alternate sides" the request asked for,
//! bought without an ordering dependency. Rounding to the metre first is what stops a float wobble
//! flipping a post 30 m sideways between two passes.
//! =========================================================================================
//!
//! THE POST FACES BACK AT THE SLOT, so the team overwatches the approach from the first frame rather
//! than spinning to whatever heading the engine spawned them with. This is the second shipped
//! provider to answer a real heading (the sniper markers are the first); the tower and defend-position
//! providers answer none, on purpose.
//!
//! THE OFFSET IS ALONG THE SLOT'S OWN RIGHT VECTOR, not along world X. A slot carries the road's
//! rotation, so its local +X is "across the road" - which is what "off to the side" means. Stepping
//! along world X would put a team in the middle of a road running north-south.
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class OVT_RoadSlotOverwatchPlacementProvider : OVT_DeploymentPlacementProvider
{
	//! How far to the side of the slot centre the team stands, in metres. 15 clears a LargeCheckpoint's
	//! own footprint (the composition module treats a large slot as ~23 m of occupant range) while
	//! staying close enough to cover the road it is watching.
	[Attribute(defvalue: "15", desc: "How far to the side of each road slot the AT post stands, in metres. Perpendicular to the slot's own facing, so it is always ACROSS the road rather than along it")]
	float m_fSideOffset;

	//! Below this a direction has no usable bearing. STATIC so the two pure statics below can read it.
	static const float DIRECTION_EPSILON = 0.001;

	//------------------------------------------------------------------------------------------------
	//! One post beside every large and medium road slot of the nearest base.
	//! \param[in] deploymentPosition Centre of the search - the deployment's own position.
	//! \param[in] radius How far the base marker, and each individual slot, may be.
	//! \param[in] factionIndex Unused - who holds the base is the config's business (the base-control
	//!            condition module), not the provider's.
	//! \return The posts, each facing its slot. Empty when there is no base in range or it has no road
	//!         slots, which is the ordinary answer away from a base.
	override array<ref OVT_DeploymentPlacement> ResolvePlacements(vector deploymentPosition, float radius, int factionIndex)
	{
		array<ref OVT_DeploymentPlacement> placements = new array<ref OVT_DeploymentPlacement>();

		OVT_BaseControllerComponent controller = OVT_BaseControllerComponent.FindNearestBaseControllerWithin(deploymentPosition, radius);
		if (!controller)
			return placements;

		// A slot can carry more than one size label, so the same entity can appear in both lists. Seen
		// once is enough - two AT teams on one slot would be two teams in the same hedge.
		array<ref EntityID> seen = new array<ref EntityID>();

		AppendSlotPosts(controller.m_LargeRoadSlots, deploymentPosition, radius, seen, placements);
		AppendSlotPosts(controller.m_MediumRoadSlots, deploymentPosition, radius, seen, placements);

		return placements;
	}

	//------------------------------------------------------------------------------------------------
	override string GetProviderName()
	{
		return "road slot overwatch";
	}

	//------------------------------------------------------------------------------------------------
	//! Which side of a slot a post goes on, from the slot's own position and nothing else.
	//!
	//! PURE AND STABLE - see the class header for why the answer must not depend on the order slots
	//! were found in. Rounded to the metre so a float wobble cannot flip a team across the road between
	//! two convergence passes.
	//! \param[in] slotPosition The slot's world position.
	//! \return +1 or -1, to multiply the side offset by.
	static int SideForSlot(vector slotPosition)
	{
		int x = Math.Round(slotPosition[0]);
		int z = Math.Round(slotPosition[2]);

		// EnforceScript's % keeps the sign of its LEFT operand, so a slot at a negative coordinate
		// would answer -1 for the modulo itself and read as the wrong branch.
		int parity = (x + z) % 2;
		if (parity < 0)
			parity = -parity;

		if (parity == 0)
			return 1;

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! The post beside one slot, as a position and a facing, from a slot transform and nothing else.
	//!
	//! PURE, so "the post is exactly m_fSideOffset from the slot, across it, looking back at it" is an
	//! assertion a test can make without a base, a slot entity or a world. The production path routes
	//! through here.
	//! \param[in] slotMat The slot's world transform. [0] is its right vector, [3] its position.
	//! \param[in] sideOffset How far to the side, in metres.
	//! \return The post. Its Y is the slot's - the caller ground-snaps.
	static OVT_DeploymentPlacement PostBesideSlot(vector slotMat[4], float sideOffset)
	{
		vector across = slotMat[0];
		across[1] = 0;

		// A slot whose transform is degenerate in the XZ plane (it should never be) keeps world X, so a
		// post is still offered rather than being silently dropped.
		if (across.Length() < DIRECTION_EPSILON)
			across = Vector(1, 0, 0);

		across.Normalize();

		vector post = slotMat[3] + (across * (sideOffset * SideForSlot(slotMat[3])));
		post[1] = slotMat[3][1];

		return new OVT_DeploymentPlacement(post, FacingTowards(post, slotMat[3]));
	}

	//------------------------------------------------------------------------------------------------
	//! Yaw/pitch/roll that looks from one position at another, flat.
	//! \param[in] from Where the occupant stands.
	//! \param[in] to What they look at.
	//! \return Angles for OVT_DeploymentPlacement, or vector.Zero when the two share a spot.
	static vector FacingTowards(vector from, vector to)
	{
		vector direction = to - from;
		direction[1] = 0;

		if (direction.Length() < DIRECTION_EPSILON)
			return vector.Zero;

		direction.Normalize();

		vector faceMat[4];
		Math3D.DirectionAndUpMatrix(direction, vector.Up, faceMat);

		return Math3D.MatrixToAngles(faceMat);
	}

	//------------------------------------------------------------------------------------------------
	//! Adds one post per slot in a list that is close enough and has not been seen already.
	//! \param[in] slots A base controller's road slot list. Null or empty is a no-op.
	//! \param[in] deploymentPosition The deployment's own position, for the range check.
	//! \param[in] radius How far a slot may be from the deployment.
	//! \param[in] seen Slot ids already used, added to in place.
	//! \param[in] placements The list being built, added to in place.
	protected void AppendSlotPosts(array<ref EntityID> slots, vector deploymentPosition, float radius, notnull array<ref EntityID> seen, notnull array<ref OVT_DeploymentPlacement> placements)
	{
		if (!slots || slots.IsEmpty())
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		foreach (EntityID id : slots)
		{
			if (seen.Contains(id))
				continue;

			IEntity slot = world.FindEntityByID(id);
			if (!slot)
				continue;

			vector slotMat[4];
			slot.GetWorldTransform(slotMat);

			// ⚠ THE RADIUS IS A REAL BOUND. The base controller discovers its slots within baseRange of
			// the BASE MARKER, which is not the deployment's position: a deployment created off-centre
			// would otherwise be handed posts it has no business standing anybody on.
			if (vector.Distance(slotMat[3], deploymentPosition) > radius)
				continue;

			seen.Insert(id);

			OVT_DeploymentPlacement post = PostBesideSlot(slotMat, m_fSideOffset);

			// The square-corner story again: the offset is flat geometry taken at the slot's own Y, so on
			// a verge that falls away from the road it arrives in the air or in the bank. Core clamps the
			// spawned entity, but the teleport that puts the team on the post reads THIS position.
			vector position = post.m_vPosition;
			position[1] = world.GetSurfaceY(position[0], position[2]);
			post.m_vPosition = position;

			placements.Insert(post);
		}
	}
}
