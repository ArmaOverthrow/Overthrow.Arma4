//------------------------------------------------------------------------------------------------
//! Composition slot condition: "is there anywhere at this base to actually PUT the thing this
//! deployment would build?"
//!
//! ===========================================================================================
//! WHY IT EXISTS: A DEPLOYMENT'S PRICE IS PAID AT CREATION, AND THE SLOT IS NOT LOOKED UP UNTIL
//! CONVERGENCE. Those are two different moments, and nothing used to connect them - so the evaluator
//! would buy a Base Checkpoints deployment at a base with no ROAD_LARGE slot anywhere, charge the
//! faction in full, and only then discover there was nowhere to build. It then latched
//! m_bCompositionAttempted so it never even retried. The money was simply gone, and it went again on
//! the next pass, and the next.
//!
//! That was survivable while the world's own slots were everywhere. It stopped being survivable on
//! 2026-08-20, when the author found that the vanilla base slots are not present in this world at all
//! and only the ones authored into slots.layer exist - so until every base has been given slots by
//! hand, "this base has no slot of that kind" is the NORMAL case rather than the exception.
//! ===========================================================================================
//!
//! 🔴 IT IS A CREATION GATE ONLY, AND EvaluateCondition() IS DELIBERATELY LEFT INHERITED (always
//! true). This is not an oversight and must not be "fixed": the moment the deployment builds its
//! composition, that composition CLAIMS the slot - so a runtime re-ask would immediately answer "no
//! free slot" and, on every config that authors m_bDeleteOnConditionFail 1, the reinforcement module
//! would tear down the deployment it had just successfully built. The condition is asking "is it worth
//! buying", and that question only has meaning before it is bought.
//!
//! ⚠ ANY, NOT ALL. A config carries several composition modules - Base Fortifications has three - and
//! one free slot is enough for the deployment to do something useful. Requiring a free slot for every
//! module would refuse a partially-buildable deployment, which is worse than building what fits.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_CompositionSlotConditionDeploymentModule : OVT_BaseConditionDeploymentModule
{
	[Attribute(desc: "Name of this module")]
	string m_sModuleName;

	[Attribute(defvalue: "500", desc: "How far from the candidate position to look for the base controller whose slots are being asked about. Matches OVT_BaseControlConditionDeploymentModule's own default so the two agree about which base a position belongs to")]
	float m_fMaxDistance;

	//! ⚠ AUTHORED AS RAW OVT_EDeploymentSlotType INTEGERS, and it MUST list exactly the types this
	//! config's composition modules author - 0 SMALL, 1 MEDIUM, 2 LARGE, 3 ROAD_SMALL, 4 ROAD_MEDIUM,
	//! 5 ROAD_LARGE.
	//!
	//! ⚠ IT IS A SECOND COPY OF SOMETHING THE CONFIG ALREADY SAYS, and that is a real cost accepted for
	//! a real reason: EvaluateStaticCondition() runs against the config TEMPLATE, with no deployment and
	//! no way to reach its sibling modules, so it cannot read their m_eSlotType itself. The drift this
	//! invites - a composition module's slot type changed and this list forgotten, leaving the gate
	//! testing the wrong pool - is closed by an Init case that asserts the two match on every shipped
	//! composition config, rather than by hoping.
	[Attribute(desc: "Which slot types this deployment can use, as OVT_EDeploymentSlotType integers (0 SMALL, 1 MEDIUM, 2 LARGE, 3 ROAD_SMALL, 4 ROAD_MEDIUM, 5 ROAD_LARGE). The deployment is refused when the base has a free slot of NONE of them. Must match the m_eSlotType of this config's composition modules")]
	ref array<int> m_aAcceptedSlotTypes;

	//------------------------------------------------------------------------------------------------
	//! Creation gate: asked by the evaluator about a candidate position, before anything is bought.
	//! \param[in] position The candidate position.
	//! \param[in] factionIndex The faction deploying. Unused: a slot is a property of the ground, and
	//!            whoever holds the base is already answered by the base-control condition beside this.
	//! \param[in] threatLevel The candidate's scored threat. Unused.
	//! \return True when the base has a free slot of at least one accepted type.
	override bool EvaluateStaticCondition(vector position, int factionIndex, float threatLevel)
	{
		if (!m_aAcceptedSlotTypes || m_aAcceptedSlotTypes.IsEmpty())
		{
			// ⚠ AN UNAUTHORED LIST ALLOWS EVERYTHING rather than refusing everything. A misauthored gate
			// that silently stopped a base ever fortifying would be far harder to notice than one that
			// simply does not gate - and the Init case that pins this list against the config's
			// composition modules is what catches the omission properly.
			return true;
		}

		OVT_BaseControllerComponent controller = OVT_BaseControllerComponent.FindNearestBaseControllerWithin(position, m_fMaxDistance);
		if (!controller)
			return false;

		foreach (int slotType : m_aAcceptedSlotTypes)
		{
			array<ref EntityID> slots = OVT_CompositionSpawningDeploymentModule.GetSlotListFor(controller, slotType);

			if (OVT_CompositionSpawningDeploymentModule.HasFreeSlot(slots, controller.m_aSlotsFilled))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! EVERY attribute has to appear here - CloneModule copies by hand, so a forgotten line ships the
	//! class default instead of the authored value. What a dropped line costs here: drop
	//! m_aAcceptedSlotTypes and the clone reads an empty list, which the gate above treats as "allow
	//! everything" - the deployment goes back to being bought at bases that cannot build it.
	override OVT_BaseDeploymentModule CloneModule()
	{
		OVT_CompositionSlotConditionDeploymentModule clone = new OVT_CompositionSlotConditionDeploymentModule();

		clone.m_sModuleName = m_sModuleName;
		clone.m_fMaxDistance = m_fMaxDistance;

		// ⚠ A COPY OF THE LIST, NOT THE REFERENCE. Sharing it would let a clone's list be mutated
		// through the template that every other deployment of this config also clones from.
		if (m_aAcceptedSlotTypes)
		{
			clone.m_aAcceptedSlotTypes = new array<int>();
			foreach (int slotType : m_aAcceptedSlotTypes)
			{
				clone.m_aAcceptedSlotTypes.Insert(slotType);
			}
		}

		return clone;
	}

	//------------------------------------------------------------------------------------------------
	void PrintDebugInfo()
	{
		Print(string.Format("Composition Slot Condition Module: %1", m_sModuleName));

		if (!m_aAcceptedSlotTypes)
			return;

		foreach (int slotType : m_aAcceptedSlotTypes)
		{
			Print(string.Format("  Accepts slot type: %1", typename.EnumToString(OVT_EDeploymentSlotType, slotType)));
		}
	}
}
