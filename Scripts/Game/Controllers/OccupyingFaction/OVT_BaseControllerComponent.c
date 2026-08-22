class OVT_BaseControllerComponentClass: OVT_ComponentClass
{
};

//------------------------------------------------------------------------------------------------
//! The marker component on a military base: WHERE ITS BUILDABLE GROUND IS, who holds it, and where a
//! QRF should come from.
//!
//! ⚠ IT NO LONGER BUYS, SPENDS, TICKS OR SPAWNS ANYTHING. Until the base-defense migration this
//! component also owned a whole parallel economy - a runtime list of upgrade objects copied from a
//! config, a 10 s timer that ticked them, a 1..19 priority sweep the occupying faction handed a
//! per-base budget to, and a by-class-name lookup the save path replayed old records into. All of it
//! is gone: base defense is nine Configs/Deployment/Deployment_Base*.conf deployments, bought out of
//! the deployment framework's own resource pool and virtualized by the core, and NOTHING may
//! reintroduce a second spender here (that is decision G2 - one accounting path, grep-enforced).
//!
//! WHAT SURVIVES, AND WHY IT IS STILL THE ONLY PLACE THAT KNOWS IT:
//!  - THE SLOT REGISTRY (m_AllSlots / m_AllCloseSlots / the six sized lists / m_Parking /
//!    m_aSlotsFilled / m_aDefendPositions / m_aVehiclePatrolSpawns). This is discovered by one world
//!    query per base at init and read by the composition, parked-vehicle and defend-position
//!    deployment modules and by QRF placement. m_aSlotsFilled
//!    in particular ROUND-TRIPS THROUGH THE SAVE and is what stops a deployment re-using a slot a
//!    structure is already standing in;
//!  - the faction/flag half, which is what a capture actually changes;
//!  - the QRF spawn geometry attributes, read by OVT_QRFControllerComponent.
//------------------------------------------------------------------------------------------------
class OVT_BaseControllerComponent: OVT_Component
{
	[Attribute("")]
	string m_sName;
	
	[Attribute(defvalue: "1", UIWidgets.EditBox, desc: "Initial Resource Multiplier")]
	float m_fStartingResourcesMultiplier;

	[Attribute("400", UIWidgets.Slider, "Minimum distance to spawn QRF", "50 1000 25")]
	int m_iAttackDistanceMin;
	
	[Attribute("800", UIWidgets.Slider, "Maximum distance to spawn QRF", "100 1000 25")]
	int m_iAttackDistanceMax;
	
	[Attribute("-1", UIWidgets.Slider, "Preferred direction to spawn QRF (randomized slightly, -1 means any direction)", "-1 359 1")]
	int m_iAttackPreferredDirection;
	
	[Attribute("30", UIWidgets.Slider, "Direction variance in degrees (QRF can spawn within +/- this many degrees from preferred direction)", "0 180 5")]
	int m_iAttackDirectionVariance;

	//! ================== THE AUTHORED PATROL SQUARE (amendment A1, 2026-08-18) ==================
	//! Every deployment at this base whose behaviour module authors OVT_PatrolType.PERIMETER_BASE walks
	//! THIS square: four corners at m_fPerimeterRadius from the base marker, the first at
	//! m_fPerimeterRotation degrees and the others at +90/+180/+270, each patrol's rotation jittered by
	//! up to OVT_PatrolBehaviorDeploymentModule.PERIMETER_ROTATION_JITTER_DEG so successive garrisons
	//! do not tread one line.
	//!
	//! WHY IT IS AUTHORED RATHER THAN ROLLED. The road-snapped ring (plain PERIMETER) is right for a
	//! town, whose roads run AROUND it, and wrong for a base, whose roads run THROUGH it - a snapped
	//! base "perimeter" collapses onto the access road. From the play-test: "the garrison waypoints
	//! aren't great... I'd actually like to make them a little authored".
	//!
	//! ⚠ SELECT THIS COMPONENT'S ENTITY IN WORKBENCH TO SEE THE SQUARE. _WB_AfterWorldUpdate draws it
	//! in cyan, edge arrowheads showing the walk direction (the runtime ±jitter is NOT drawn - one
	//! square, by request). A corner over water or inside a building is a designer problem: nothing
	//! moves a corner at runtime, because moving one would stop the square being the square that was
	//! authored.
	//! ===========================================================================================
	[Attribute("280", UIWidgets.Slider, "Radius of the authored patrol square for PERIMETER deployments at this base, in metres (280 = baseRange, the legacy patrol radius)", "10 600 5")]
	float m_fPerimeterRadius;

	[Attribute("0", UIWidgets.Slider, "Rotation of the authored patrol square for PERIMETER deployments at this base, in degrees. 0 puts the first corner due north; each patrol jitters this by a few degrees", "0 359 1")]
	float m_fPerimeterRotation;

	ref array<ref EntityID> m_AllSlots;
	ref array<ref EntityID> m_AllCloseSlots;
	ref array<ref EntityID> m_SmallSlots;
	ref array<ref EntityID> m_MediumSlots;
	ref array<ref EntityID> m_LargeSlots;
	ref array<ref EntityID> m_SmallRoadSlots;
	ref array<ref EntityID> m_MediumRoadSlots;
	ref array<ref EntityID> m_LargeRoadSlots;
	ref array<ref EntityID> m_Parking;
	ref array<ref EntityID> m_aSlotsFilled;
	ref array<ref vector> m_aDefendPositions;
	ref array<ref EntityID> m_aVehiclePatrolSpawns;

	protected OVT_OccupyingFactionManager m_occupyingFactionManager;

	void InitBaseClient()
	{
		if(Replication.IsServer()) return;
		SCR_FactionAffiliationComponent affiliation = OVT_ComponentFinder<SCR_FactionAffiliationComponent>.Find(GetOwner());
		if(affiliation)
		{
			affiliation.GetOnFactionChanged().Insert(OnFactionChanged);
		}
	}
		
	void InitBase()
	{
		if(!Replication.IsServer()) return;
		if (SCR_Global.IsEditMode()) return;

		m_occupyingFactionManager = OVT_Global.GetOccupyingFaction();
		
		SCR_FactionAffiliationComponent affiliation = OVT_ComponentFinder<SCR_FactionAffiliationComponent>.Find(GetOwner());
		if(affiliation)
		{
			affiliation.GetOnFactionChanged().Insert(OnFactionChanged);
		}

		InitializeBase();
	}
	
	OVT_BaseData GetData()
	{
		OVT_OccupyingFactionManager of = OVT_Global.GetOccupyingFaction();
		return of.GetNearestBase(GetOwner().GetOrigin());
	}

	void OnFactionChanged(FactionAffiliationComponent owner, Faction previousFaction, Faction newFaction)
	{
		// Get the faction index
		FactionManager factionManager = GetGame().GetFactionManager();
		int factionIndex = factionManager.GetFactionIndex(newFaction);
						
		// Update flag
		UpdateFlagMaterial(factionIndex);	
	}
	
	void UpdateFlagMaterial(int factionIndex)
	{
		FactionManager factionManager = GetGame().GetFactionManager();
		Faction faction = factionManager.GetFactionByIndex(factionIndex);
		if (!faction)
			return;
		
		SCR_Faction scrFaction = SCR_Faction.Cast(faction);
		if (!scrFaction)
			return;
		
		SCR_FlagComponent flag = OVT_ComponentFinder<SCR_FlagComponent>.Find(GetOwner());
		if (!flag)
			return;
		
		flag.ChangeMaterial(scrFaction.GetFactionFlagMaterial());
	}

	bool IsOccupyingFaction()
	{
		SCR_FactionAffiliationComponent affiliation = OVT_ComponentFinder<SCR_FactionAffiliationComponent>.Find(GetOwner());
		Faction occupyingFactionData = OVT_Global.GetConfig().GetOccupyingFactionData();
		FactionKey occupyingFaction = occupyingFactionData.GetFactionKey();
		
		Faction affiliatedFactionData = affiliation.GetAffiliatedFaction();
		FactionKey affiliatedFaction = affiliatedFactionData.GetFactionKey();
		return affiliatedFaction == occupyingFaction;
	}

	int GetControllingFaction()
	{
		SCR_FactionAffiliationComponent affiliation = OVT_ComponentFinder<SCR_FactionAffiliationComponent>.Find(GetOwner());

		return GetGame().GetFactionManager().GetFactionIndex(affiliation.GetAffiliatedFaction());
	}

	void SetControllingFaction(string key, bool suppressEvents = false)
	{
		FactionManager mgr = GetGame().GetFactionManager();
		Faction faction = mgr.GetFactionByKey(key);
		int index = mgr.GetFactionIndex(faction);
		SetControllingFaction(index, suppressEvents);
	}

	void SetControllingFaction(int index, bool suppressEvents = false)
	{
		if(!suppressEvents)
			m_occupyingFactionManager.OnBaseControlChange(this);

		Faction fac = GetGame().GetFactionManager().GetFactionByIndex(index);
		SCR_FactionAffiliationComponent affiliation = OVT_ComponentFinder<SCR_FactionAffiliationComponent>.Find(GetOwner());
		affiliation.SetAffiliatedFaction(fac);
	}

	void InitializeBase()
	{
		m_AllSlots = new array<ref EntityID>;
		m_AllCloseSlots = new array<ref EntityID>;
		m_SmallSlots = new array<ref EntityID>;
		m_MediumSlots = new array<ref EntityID>;
		m_LargeSlots = new array<ref EntityID>;
		m_SmallRoadSlots = new array<ref EntityID>;
		m_MediumRoadSlots = new array<ref EntityID>;
		m_LargeRoadSlots = new array<ref EntityID>;
		m_Parking = new array<ref EntityID>;
		m_aSlotsFilled = new array<ref EntityID>;
		m_aDefendPositions = new array<ref vector>;
		m_aVehiclePatrolSpawns = new array<ref EntityID>;

		FindSlots();
		FindParking();
	}

	void FindSlots()
	{
		GetGame().GetWorld().QueryEntitiesBySphere(GetOwner().GetOrigin(),  OVT_Global.GetConfig().m_Difficulty.baseRange, CheckSlotAddToArray, FilterSlotEntities);

		// ⚠ THE SLOT INVENTORY IS OTHERWISE INVISIBLE UNTIL SOMETHING FAILS TO USE IT, which is how a
		// base with no ROAD_LARGE slot at all looked identical to one whose road slots were merely taken,
		// and how "all the vanilla slots are missing" went unnoticed until a checkpoint was bought and
		// never built (author, 2026-08-20). This is the one moment the answer is known, it is one line
		// per base at init, and it turns "is the discovery working" from a rebuild into a log read.
		//
		// EVERY SIZE IS PRINTED, INCLUDING THE ZEROES. A missing size is exactly the interesting case, so
		// it must not be the one that prints nothing.
		// ⚠ APPENDED IN STEPS, NOT BUILT AS ONE EXPRESSION, AND NOT string.Format. Format caps its
		// parameter count and rejects this many outright ("Too many parameters for 'Format' method");
		// a single long `+` chain then fails differently, with "Formula too complex" - EnforceScript
		// caps expression size too. Successive `+=` clears both, and the whole value of the line is that
		// it carries every size at once, so splitting it across two Prints would be worse than this.
		string inventory = "[Overthrow] Base '" + m_sName + "' slot inventory within ";
		inventory += OVT_Global.GetConfig().m_Difficulty.baseRange.ToString() + " m:";
		inventory += " SMALL " + m_SmallSlots.Count().ToString();
		inventory += ", MEDIUM " + m_MediumSlots.Count().ToString();
		inventory += ", LARGE " + m_LargeSlots.Count().ToString();
		inventory += ", ROAD_SMALL " + m_SmallRoadSlots.Count().ToString();
		inventory += ", ROAD_MEDIUM " + m_MediumRoadSlots.Count().ToString();
		inventory += ", ROAD_LARGE " + m_LargeRoadSlots.Count().ToString();
		inventory += " (total " + m_AllSlots.Count().ToString();
		inventory += ", defend posts " + m_aDefendPositions.Count().ToString();
		inventory += ", vehicle spawns " + m_aVehiclePatrolSpawns.Count().ToString() + ")";

		Print(inventory, LogLevel.NORMAL);
	}

	bool FilterSlotEntities(IEntity entity)
	{
		OVT_VehiclePatrolSpawn vehicleSpawn = OVT_VehiclePatrolSpawn.Cast(entity);
		if(vehicleSpawn)
		{
			m_aVehiclePatrolSpawns.Insert(entity.GetID());
			return true;
		}
		
		SCR_EditableEntityComponent editable = OVT_ComponentFinder<SCR_EditableEntityComponent>.Find(entity);
		if(editable && editable.GetEntityType() == EEditableEntityType.SLOT)
		{
			return true;
		}

		SCR_AISmartActionSentinelComponent action = OVT_ComponentFinder<SCR_AISmartActionSentinelComponent>.Find(entity);
		if(action) {
			SCR_MapDescriptorComponent mapdes = OVT_ComponentFinder<SCR_MapDescriptorComponent>.Find(entity);
			if(mapdes)
			{
				EMapDescriptorType type = mapdes.GetBaseType();
				//Towers are handled by OVT_TowerCoverPostPlacementProvider (Deployment_BaseTowerGuards.conf),
				//which finds them by map descriptor itself - a tower left in this sweep would be manned twice
				if(type == EMapDescriptorType.MDT_TOWER) return false;
			}
			return true;
		}
		return false;
	}

	bool CheckSlotAddToArray(IEntity entity)
	{
		SCR_AISmartActionSentinelComponent action = OVT_ComponentFinder<SCR_AISmartActionSentinelComponent>.Find(entity);
		if(action)
		{
			vector pos = entity.GetOrigin();
			if(!m_aDefendPositions.Contains(pos))
				m_aDefendPositions.Insert(entity.GetOrigin());
			return true;
		}

		SCR_EditableEntityComponent editable = OVT_ComponentFinder<SCR_EditableEntityComponent>.Find(entity);
		if(editable && editable.GetEntityType() == EEditableEntityType.SLOT)
		{
			SCR_EditableEntityUIInfo uiinfo = SCR_EditableEntityUIInfo.Cast(editable.GetInfo());
			if(!uiinfo) return true;

			m_AllSlots.Insert(entity.GetID());

			float distance = vector.Distance(entity.GetOrigin(), GetOwner().GetOrigin());
			if(distance <  OVT_Global.GetConfig().m_Difficulty.baseCloseRange)
			{
				m_AllCloseSlots.Insert(entity.GetID());
			}

			string name = entity.GetPrefabData().GetPrefabName();
			if(uiinfo.HasEntityLabel(EEditableEntityLabel.SLOT_FLAT_SMALL)) m_SmallSlots.Insert(entity.GetID());
			if(uiinfo.HasEntityLabel(EEditableEntityLabel.SLOT_FLAT_MEDIUM)) m_MediumSlots.Insert(entity.GetID());
			if(uiinfo.HasEntityLabel(EEditableEntityLabel.SLOT_FLAT_LARGE)) m_LargeSlots.Insert(entity.GetID());
			if(uiinfo.HasEntityLabel(EEditableEntityLabel.SLOT_ROAD_SMALL)) m_SmallRoadSlots.Insert(entity.GetID());
			if(uiinfo.HasEntityLabel(EEditableEntityLabel.SLOT_ROAD_MEDIUM)) m_MediumRoadSlots.Insert(entity.GetID());
			if(uiinfo.HasEntityLabel(EEditableEntityLabel.SLOT_ROAD_LARGE)) m_LargeRoadSlots.Insert(entity.GetID());
		}

		return true;
	}

	void FindParking()
	{
		GetGame().GetWorld().QueryEntitiesBySphere(GetOwner().GetOrigin(), OVT_Global.GetConfig().m_Difficulty.baseCloseRange, null, FilterParkingEntities, EQueryEntitiesFlags.ALL);
	}

	bool FilterParkingEntities(IEntity entity)
	{
		if(entity.FindComponent(OVT_ParkingComponent)) {
			m_Parking.Insert(entity.GetID());
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The base controller nearest a position, if it is close enough to be that position's own base.
	//!
	//! THE ONE LOOKUP TWO CONSUMERS OUTSIDE THIS FILE NEED, and it lives here because the answer is a
	//! base controller: the PERIMETER_BASE patrol branch reads the authored square off it, and
	//! OVT_RoadSlotOverwatchPlacementProvider reads the road slots off it. Both are asked at 250 m -
	//! OVT_DeploymentManagerComponent.BASE_CLASSIFICATION_RADIUS, the same radius within which the
	//! evaluator considers a position to BE a base.
	//!
	//! EVERY DEREFERENCE IS GUARDED. This is legal to call off a config template in a world with no
	//! occupying faction manager at all (which is exactly what the Init tier does), and "no base here"
	//! is an ordinary answer rather than an error.
	//!
	//! (OVT_BaseDefendPositionPlacementProvider carries its own protected copy of this walk, written
	//! before this static existed. It is left alone on purpose - it is shipped, working Phase 4 code
	//! and the duplication costs nothing but four lines.)
	//! \param[in] position The position to search around.
	//! \param[in] radius How far the base marker may be, in metres.
	//! \return The controller, or null when there is no base in range.
	static OVT_BaseControllerComponent FindNearestBaseControllerWithin(vector position, float radius)
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
			return null;

		OVT_BaseData nearest = occupying.GetNearestBase(position);
		if (!nearest)
			return null;

		if (vector.Distance(nearest.location, position) > radius)
			return null;

		IEntity marker = GetGame().GetWorld().FindEntityByID(nearest.entId);
		if (!marker)
			return null;

		return OVT_BaseControllerComponent.Cast(marker.FindComponent(OVT_BaseControllerComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! One corner of this base's authored patrol square, in world space, at the base marker's own Y.
	//!
	//! PURE, so the Workbench viz and the runtime plan cannot disagree about where the square is:
	//! _WB_AfterWorldUpdate draws these points and OVT_VirtualPlanFactory.BuildSquarePerimeterPlan
	//! builds the same ones from the same two numbers.
	//! \param[in] centre The base marker position.
	//! \param[in] radius Distance from the centre to each corner.
	//! \param[in] rotationDeg Yaw of corner 0, in degrees.
	//! \param[in] corner 0..3; the corners run clockwise at +90 degrees each.
	//! \return The corner position.
	static vector PerimeterCorner(vector centre, float radius, float rotationDeg, int corner)
	{
		vector position = centre + (vector.FromYaw(rotationDeg + (corner * 90)) * radius);
		position[1] = centre[1];

		return position;
	}

	IEntity GetNearestSlot(vector pos)
	{
		IEntity nearest;
		float nearestDist = -1;
		foreach(EntityID id : m_AllSlots)
		{
			IEntity ent = GetGame().GetWorld().FindEntityByID(id);
			float dist = vector.Distance(pos, ent.GetOrigin());
			if(nearestDist == -1 || dist < nearestDist)
			{
				nearest = ent;
				nearestDist = dist;
			}
		}
		return nearest;
	}

#ifdef WORKBENCH
	protected ref Shape m_aDirectionArrowCenter;
	protected ref Shape m_aDirectionArrowMin;
	protected ref Shape m_aDirectionArrowMax;

	//! ==========================================================================================
	//! ⚠⚠ THIS VIZ USES Shape.CreateArrow ONLY. DO NOT REWRITE IT WITH THE CreateLines FAMILY. ⚠⚠
	//!
	//! CreateArrow COPIES its two vectors by value - which is why the QRF attack-direction arrows
	//! above have always been safe built from locals, and why every edge below is one. The
	//! CreateLines/CreateLinesLoop/CreateTris family instead REFERENCES the caller's vertex array,
	//! and this viz crashed Workbench TWICE (2026-08-18, amendment A1) when built on it: first with
	//! local buffers (render thread read a dead stack frame - jittering vertices, then an access
	//! violation), then STILL crashed with member buffers sized and filled per the vanilla
	//! SCR_PowerLineJointEntity precedent. Root cause of the second crash was never symbolised;
	//! rather than keep gambling on that family's exact contract, the viz was rebuilt on the one
	//! primitive with years of proven per-frame use three methods above. Bonus: the edge arrowheads
	//! show the patrol's walk direction.
	//! ==========================================================================================

	//! How high above the marker's own Y the square is drawn, so it is not buried in the terrain the
	//! base sits on. The runtime plan is ground-snapped per corner; this is a drawing offset only.
	protected const float PERIMETER_DRAW_LIFT = 2;

	//! Length of the little arrow marking corner 0, in metres.
	protected const float PERIMETER_START_ARROW = 25;

	//Draw attack preferred direction as arrows showing variance
	override int _WB_GetAfterWorldUpdateSpecs(IEntity owner, IEntitySource src)
	{
		return EEntityFrameUpdateSpecs.CALL_WHEN_ENTITY_SELECTED;
	}
	
	protected override void _WB_AfterWorldUpdate(IEntity owner, float timeSlice)
	{
		if (m_iAttackPreferredDirection != -1)
		{
			vector basePos = owner.GetOrigin();
			
			// Draw center arrow (main direction)
			float centerRad = m_iAttackPreferredDirection * Math.DEG2RAD;
			vector fromCenter = basePos + Vector(Math.Sin(centerRad) * m_iAttackDistanceMax, 0, -Math.Cos(centerRad) * m_iAttackDistanceMax);
			vector toCenter = basePos + Vector(Math.Sin(centerRad) * m_iAttackDistanceMin, 0, -Math.Cos(centerRad) * m_iAttackDistanceMin);
			m_aDirectionArrowCenter = Shape.CreateArrow(fromCenter, toCenter, 10, Color.FromRGBA(255, 0, 0, 255).PackToInt(), ShapeFlags.ONCE | ShapeFlags.NOZBUFFER | ShapeFlags.TRANSP | ShapeFlags.DOUBLESIDE | ShapeFlags.NOOUTLINE);
			
			// Draw variance arrows (showing the extremes)
			float minRad = (m_iAttackPreferredDirection - m_iAttackDirectionVariance) * Math.DEG2RAD;
			vector fromMin = basePos + Vector(Math.Sin(minRad) * m_iAttackDistanceMax, 0, -Math.Cos(minRad) * m_iAttackDistanceMax);
			vector toMin = basePos + Vector(Math.Sin(minRad) * m_iAttackDistanceMin, 0, -Math.Cos(minRad) * m_iAttackDistanceMin);
			m_aDirectionArrowMin = Shape.CreateArrow(fromMin, toMin, 6, Color.FromRGBA(255, 0, 0, 128).PackToInt(), ShapeFlags.ONCE | ShapeFlags.NOZBUFFER | ShapeFlags.TRANSP | ShapeFlags.DOUBLESIDE | ShapeFlags.NOOUTLINE);
			
			float maxRad = (m_iAttackPreferredDirection + m_iAttackDirectionVariance) * Math.DEG2RAD;
			vector fromMax = basePos + Vector(Math.Sin(maxRad) * m_iAttackDistanceMax, 0, -Math.Cos(maxRad) * m_iAttackDistanceMax);
			vector toMax = basePos + Vector(Math.Sin(maxRad) * m_iAttackDistanceMin, 0, -Math.Cos(maxRad) * m_iAttackDistanceMin);
			m_aDirectionArrowMax = Shape.CreateArrow(fromMax, toMax, 6, Color.FromRGBA(255, 0, 0, 128).PackToInt(), ShapeFlags.ONCE | ShapeFlags.NOZBUFFER | ShapeFlags.TRANSP | ShapeFlags.DOUBLESIDE | ShapeFlags.NOOUTLINE);
		}

		DrawPerimeterSquare(owner);

		super._WB_AfterWorldUpdate(owner, timeSlice);
	}

	//------------------------------------------------------------------------------------------------
	//! Draws the authored PERIMETER_BASE patrol square, in CYAN so it never reads as an attack arrow.
	//!
	//! ONE SQUARE, the authored rotation, edge arrowheads showing the walk direction. The runtime's
	//! per-patrol ±jitter is deliberately NOT drawn (user request, 2026-08-18): a garrison's real
	//! corners land within a few degrees of what is shown.
	//!
	//! The little arrow points from the base marker at CORNER 0, which is where m_fPerimeterRotation
	//! puts the square's "start" - the corner every walk is measured from.
	//!
	//! Corner geometry comes from PerimeterCorner(), the same static
	//! OVT_VirtualPlanFactory.BuildSquarePerimeterPlan agrees with, so the picture cannot drift away
	//! from the plan.
	//!
	//! ⚠ NOTHING HERE IS ROLLED - PerimeterCorner() is pure, so the drawn square is identical every
	//! frame. A shimmering or jittering square therefore always means native memory corruption, which
	//! is how the 2026-08-18 crashes were recognised (see the hard rule above).
	//! \param[in] owner The base marker entity.
	protected void DrawPerimeterSquare(IEntity owner)
	{
		if (!owner || m_fPerimeterRadius <= 0)
			return;

		vector centre = owner.GetOrigin();
		centre[1] = centre[1] + PERIMETER_DRAW_LIFT;

		int flags = ShapeFlags.ONCE | ShapeFlags.NOZBUFFER | ShapeFlags.TRANSP | ShapeFlags.DOUBLESIDE | ShapeFlags.NOOUTLINE;
		int solid = Color.FromRGBA(0, 200, 255, 255).PackToInt();

		// One arrow per edge, corner N -> corner N+1, so the arrowheads read as the walk direction.
		// ONCE shapes drawn as bare calls, no handle kept: the engine owns a ONCE shape for the frame
		// (vanilla precedent: SCR_PowerLineJointEntity.c:163 does exactly this per frame).
		// The runtime's ±jitter band is deliberately NOT drawn - one square only, by request.
		DrawSquareEdges(centre, m_fPerimeterRotation, 8, solid, flags);

		// Where the square starts: an arrow from the marker towards corner 0.
		vector towardsStart = vector.Direction(centre, PerimeterCorner(centre, m_fPerimeterRadius, m_fPerimeterRotation, 0));
		towardsStart[1] = 0;
		if (towardsStart.Length() < 0.001)
			return;

		towardsStart.Normalize();
		Shape.CreateArrow(centre, centre + (towardsStart * PERIMETER_START_ARROW), 8, solid, flags);
	}

	//------------------------------------------------------------------------------------------------
	//! Draws one square as four CreateArrow edges (copy-safe - see the hard rule above).
	//! \param[in] centre The (already lifted) square centre.
	//! \param[in] rotationDeg Yaw of corner 0.
	//! \param[in] headSize Arrowhead size in metres; also what tells solid and faint squares apart at a glance.
	//! \param[in] color Packed RGBA.
	//! \param[in] flags Shape flags shared with the attack arrows.
	protected void DrawSquareEdges(vector centre, float rotationDeg, float headSize, int color, int flags)
	{
		for (int corner = 0; corner < 4; corner++)
		{
			vector from = PerimeterCorner(centre, m_fPerimeterRadius, rotationDeg, corner);
			vector to = PerimeterCorner(centre, m_fPerimeterRadius, rotationDeg, (corner + 1) % 4);
			Shape.CreateArrow(from, to, headSize, color, flags);
		}
	}
#endif

	//RPC methods

	//------------------------------------------------------------------------------------------------
	//! Get a random vehicle patrol spawn point from the base
	//!
	//! Answers the marker's HEADING ONLY, as a plain float, and that is deliberate on both counts.
	//!
	//! UPRIGHT: a patrol vehicle has exactly one correct attitude - level, pointing the way the marker
	//! points. The authored markers already carry a few degrees of terrain pitch (Eden has several
	//! between 1.6 and 4.7 degrees) and nothing stops a hand-placed one carrying much more, so any
	//! contract that could hand a caller a pitch or a roll is a contract that can put a vehicle on its
	//! nose. Discarding them here means no consumer has to remember to.
	//!
	//! A FLOAT, NOT AN ANGLE VECTOR: the two engine APIs a caller would reach for disagree on ordering -
	//! IEntity.GetAngles()/SetAngles() are (X=pitch, Y=yaw, Z=roll) while Math3D.AnglesToMatrix() wants
	//! (yaw, pitch, roll). A vector crossing that boundary in the wrong order silently turns this
	//! marker's heading into a pitch, which is precisely a vehicle standing on its nose. A float named
	//! for what it is cannot be put in the wrong slot.
	//!
	//! \param[out] outPosition The spawn position
	//! \param[out] outYaw The marker's heading in degrees (rotation about the world Y axis)
	//! \return True if a spawn point was found, false if none exist
	bool GetRandomVehiclePatrolSpawn(out vector outPosition, out float outYaw)
	{
		// Check if we have any vehicle patrol spawns
		if (m_aVehiclePatrolSpawns.IsEmpty())
			return false;
		
		// Get a random spawn
		int randomIndex = Math.RandomInt(0, m_aVehiclePatrolSpawns.Count());
		EntityID spawnID = m_aVehiclePatrolSpawns[randomIndex];
		
		IEntity spawnEntity = GetGame().GetWorld().FindEntityByID(spawnID);
		if (!spawnEntity)
		{
			// Clean up invalid entity reference
			m_aVehiclePatrolSpawns.Remove(randomIndex);
			
			// Try again if we still have spawns
			if (!m_aVehiclePatrolSpawns.IsEmpty())
				return GetRandomVehiclePatrolSpawn(outPosition, outYaw);

			return false;
		}

		// Get position and heading. GetYawPitchRoll()[0] IS the yaw - the pitch and roll the marker
		// carries are dropped here rather than passed on, see the contract note above.
		outPosition = spawnEntity.GetOrigin();
		outYaw = spawnEntity.GetYawPitchRoll()[0];

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Every authored OVT_VehiclePatrolSpawn marker at this base, resolved to live entities.
	//!
	//! ⚠ THE LIST IS ALREADY DISCOVERED - do not re-query the world for these. FindSlots() sweeps
	//! baseRange around the base marker once at base init and FilterSlotEntities() caches every marker
	//! it finds; this is that cache, resolved and pruned. A caller that wants to CHOOSE between the
	//! markers (rather than take a random one, which GetRandomVehiclePatrolSpawn does) needs them all.
	//!
	//! Stale ids are dropped as they are found, exactly as GetRandomVehiclePatrolSpawn does: a marker
	//! is an ordinary world entity and nothing tells this component when one goes away.
	//! \param[in] results Filled with the live marker entities. Cleared first.
	void CollectVehiclePatrolSpawns(notnull array<IEntity> results)
	{
		results.Clear();

		if (!m_aVehiclePatrolSpawns)
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		// FORWARD, so the results keep the order the markers were discovered in - a caller that breaks a
		// tie by index (OVT_InsertionGeometry.ChooseSpawnMarker does) would otherwise silently prefer the
		// other one of a symmetric pair.
		int i = 0;
		while (i < m_aVehiclePatrolSpawns.Count())
		{
			IEntity marker = world.FindEntityByID(m_aVehiclePatrolSpawns[i]);
			if (!marker)
			{
				m_aVehiclePatrolSpawns.Remove(i);
				continue;
			}

			results.Insert(marker);
			i++;
		}
	}


}