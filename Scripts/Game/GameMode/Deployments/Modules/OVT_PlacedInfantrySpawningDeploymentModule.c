//------------------------------------------------------------------------------------------------
//! Infantry that stands at EXACT POSTS rather than on a ring: tower walkways, curated sniper markers,
//! a base's defend positions - whatever the authored OVT_DeploymentPlacementProvider answers.
//!
//! ================== WHY IT SUBCLASSES THE INFANTRY MODULE (D6) ==========================
//! Not for code reuse. OVT_ReinforcementBehaviorDeploymentModule.GetMissingUnitsCount() casts to
//! OVT_InfantrySpawningDeploymentModule and returns 0 for anything else (:186-197), so a sibling
//! class would silently NEVER be rebought and nobody would find out until a play-test. Everything
//! else - convergence, reclaim, the two eliminated gates, wipe accounting, GM re-tagging, faction-key
//! resolution - comes free with that decision.
//! =======================================================================================
//!
//! ================== WHAT IS NEW HERE, AND WHY LEGACY DID NOT NEED IT ====================
//! The legacy tower guard and sniper team base upgrades spawned a group, subscribed GetOnAgentAdded,
//! teleported the members onto the post as they arrived, and UNSUBSCRIBED - correct, because that
//! group spawned exactly once in its life and was deleted rather than despawned. (Both classes were
//! deleted in virtualization/base-defense-migration Phase 4, when the configs that replace them
//! shipped; line-exact citations live in that feature's implementation.md §3.3.)
//!
//! A REGISTERED GROUP MATERIALISES AND DE-MATERIALISES FOREVER. The engine deletes its members when
//! the last observer leaves and re-creates them when one comes back, and Event_OnAgentAdded fires
//! again every single time. So:
//!   - the subscription lives as long as the registration, and is only torn down when this module
//!     releases the group. Unsubscribing after the first placement would put the guard on the ground
//!     under his tower the second time you drove up to it - which is exactly the bug F5 tests for;
//!   - the ARRIVAL COUNTER is reset per materialisation, off the group's own
//!     GetOnMembersDespawning() (core api.md §3: subscribe on the GROUP, not the manager), so the
//!     second materialisation does not start offsetting men from where the first one left off. The
//!     modulo wrap in SlotForArrival() is the belt to that braces;
//!   - a RECLAIMED group is re-subscribed, because after a load core re-creates the group ENTITY from
//!     its own payload and every subscription on the old one went with it.
//! =======================================================================================
//!
//! ⚠ GetOnAgentAdded() PASSES ONE ARGUMENT, NOT TWO. The vanilla doc comment (SCR_AIGroup.c:2264-2270)
//! says "AIGroup group, AIAgent agent"; the invocation at :2458-2461 is Invoke(child) - the agent
//! alone. A handler written to the header compiles clean and misbehaves at the wire. The group is
//! recovered with agent.GetParentGroup().
//!
//! ⚠ POSTS ARE ASSIGNED BY HANDLE ORDER, NOT BY IDENTITY. A group's post is the index of its handle in
//! m_aHandles, which means a wipe that removes a handle from the middle shifts the survivors along by
//! one post. That is deliberate and harmless: posts within one concern are interchangeable, and what
//! preserves WHO is alive is core's survivor mask, which is keyed on the registration and not on the
//! post. Trying to pin a post to a handle across a load would need a persisted map, and D7/D10 freeze
//! the payload.
//!
//! ⚠ NO WAYPOINT IS THE POINT. Legacy gave these guards no waypoint on purpose: every post waypoint
//! available parks them at a smart action that is a pose loop with no fire node, while an idle group
//! keeps full threat and attack reactions. Under virtualization the plan is the opt-in, so a config
//! that authors no behaviour module - or a DEFEND one - gets exactly that, and the movement tick never
//! walks them off their posts. Deployment_BaseTowerGuards.conf and Deployment_BaseSniperPositions.conf
//! author NO behaviour module at all for this reason; Deployment_BaseDefensePositions.conf authors a
//! DEFEND one, because the legacy defense-position guard did carry a defend waypoint.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_PlacedInfantrySpawningDeploymentModule : OVT_InfantrySpawningDeploymentModule
{
	[Attribute(desc: "Where this module's groups stand. One provider per config: tower cover posts, sniper markers, base defend positions, or a mod's own")]
	ref OVT_DeploymentPlacementProvider m_Placement;

	//! 280 = m_Difficulty.baseRange, the radius the legacy tower/sniper/defend-position upgrades swept
	//! around their base marker.
	[Attribute(defvalue: "280", desc: "How far from the deployment to look for posts")]
	float m_fSearchRadius;

	//! Sideways step between two members of one group on the same post, along the post's own right
	//! vector. 1.2 m is the legacy sniper team's TEAM_MEMBER_OFFSET.
	//!
	//! STATIC so the pure placement decision below can be asserted against the exact number the
	//! production teleport uses, rather than against a copy of it in a test.
	static const float MEMBER_SPACING = 1.2;

	//! Arrival index wrap when a group's roster size cannot be read. Only a safety net: without a wrap
	//! a missed despawn notification would march each new arrival another step sideways, forever.
	static const int FALLBACK_SPREAD = 8;

	//! The post each handle stands on. Rebuilt from the provider on every registration and reclaim.
	protected ref map<int, ref OVT_DeploymentPlacement> m_mPostByHandle;

	//! How many members have arrived at each handle SINCE ITS LAST MATERIALISATION. Zeroed by the
	//! group's own despawn notification.
	protected ref map<int, int> m_mArrivalsByHandle;

	//! The group entity this module currently has the placement applier hooked to, per handle. Kept
	//! separately from m_aHandles because it is what teardown needs: the handles are cleared by the
	//! base class's cleanup before this module gets a chance to unhook.
	protected ref map<int, ref EntityID> m_mHookedGroupByHandle;

	//! Posts as of this convergence pass. Invalidated at the top of every ReclaimHandles - i.e. once
	//! per pass, on every path into the convergence - so the world is re-asked as often as it matters
	//! without a sphere query per group.
	protected ref array<ref OVT_DeploymentPlacement> m_aPostsThisPass;

	//! Set while this module is releasing its groups, so the reclaim the base class runs inside
	//! OnCleanup() does not hook everything straight back up again.
	protected bool m_bReleasing;

	//------------------------------------------------------------------------------------------------
	void OVT_PlacedInfantrySpawningDeploymentModule()
	{
		m_mPostByHandle = new map<int, ref OVT_DeploymentPlacement>();
		m_mArrivalsByHandle = new map<int, int>();
		m_mHookedGroupByHandle = new map<int, ref EntityID>();
	}

	//------------------------------------------------------------------------------------------------
	//! EVERY inherited attribute plus this module's own. CloneModule is NOT chained - it builds a fresh
	//! instance and copies by hand - so the twelve lines from OVT_InfantrySpawningDeploymentModule have
	//! to be repeated here verbatim. A forgotten one ships the class default in silence: drop
	//! m_eImportance and every tower guard runs at NORMAL instead of HIGH; drop m_Placement and the
	//! module has nowhere to stand anybody and registers nothing at all.
	override OVT_BaseDeploymentModule CloneModule()
	{
		OVT_PlacedInfantrySpawningDeploymentModule clone = new OVT_PlacedInfantrySpawningDeploymentModule();

		// --- inherited from OVT_InfantrySpawningDeploymentModule (all twelve, plus m_bSnapToRoad) ---
		clone.m_sModuleName = m_sModuleName;
		clone.m_sGroupType = m_sGroupType;
		clone.m_iMinGroupCount = m_iMinGroupCount;
		clone.m_iMaxGroupCount = m_iMaxGroupCount;
		clone.m_bScaleByTownSize = m_bScaleByTownSize;
		clone.m_fSpawnRadius = m_fSpawnRadius;
		clone.m_iCostPerGroup = m_iCostPerGroup;
		clone.m_bAllowReinforcement = m_bAllowReinforcement;
		clone.m_iReinforcementCost = m_iReinforcementCost;
		clone.m_bSpawnAtNearestBase = m_bSpawnAtNearestBase;
		clone.m_bReinforceFromNearestBase = m_bReinforceFromNearestBase;
		clone.m_eImportance = m_eImportance;
		clone.m_bSnapToRoad = m_bSnapToRoad;

		// --- this module's own ---
		// The provider is SHARED, not copied: it holds no state between calls (its scratch array is
		// rebuilt at the top of every resolve and dropped at the bottom), and cloning a polymorphic
		// config object by hand would mean a copy method per provider that a mod's provider would not
		// have.
		clone.m_Placement = m_Placement;
		clone.m_fSearchRadius = m_fSearchRadius;

		return clone;
	}

	//------------------------------------------------------------------------------------------------
	// THE PURE PLACEMENT DECISION
	//
	// "The second materialisation puts men on the same posts as the first" is this module's headline
	// promise and the one thing about it a player notices when it breaks. Live, it is unassertable
	// without a real despawn/respawn cycle driven by real distance; so the decision itself - which post
	// a group takes and which step an arrival takes on it - is expressed here as functions of their
	// arguments and NOTHING ELSE. No world, no deployment, no registry, no member state. The
	// re-materialisation claim then reduces to "the same inputs answer the same output twice", which is
	// an Init-tier assertion.
	//
	// ⚠ THESE ARE THE PRODUCTION PATH, NOT A PARALLEL COPY OF IT. AssignPost(), ResolveSpawnPosition()
	// and OnPlacedAgentAdded() all route through them. A second implementation that agreed with these
	// on the day it was written and drifted afterwards would be worse than no test at all.
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Every post the authored provider offers around a position, with no caching and no deployment
	//! behind it.
	//!
	//! The one place the provider is ever called. EnsurePosts() adds the per-pass cache and the
	//! deployment's own position/faction on top; this answers the raw question, which is what makes it
	//! callable off a bare config template.
	//! \param[in] position Centre of the search.
	//! \param[in] radius How far to look.
	//! \param[in] factionIndex The controlling faction, for providers that care.
	//! \return The posts. NEVER null; empty when there is no provider or nothing qualifies.
	array<ref OVT_DeploymentPlacement> ResolvePlacements(vector position, float radius, int factionIndex)
	{
		if (!m_Placement)
			return new array<ref OVT_DeploymentPlacement>();

		array<ref OVT_DeploymentPlacement> posts = m_Placement.ResolvePlacements(position, radius, factionIndex);

		// A provider that breaks its contract and answers null must not take the module down with it.
		if (!posts)
			return new array<ref OVT_DeploymentPlacement>();

		return posts;
	}

	//------------------------------------------------------------------------------------------------
	//! The post the groupIndex-th group of this module stands on.
	//!
	//! WRAPPING IS THE DESIGNED ANSWER TO "more groups than posts", not a bug guard: a base whose
	//! provider offers two posts and whose config asks for three groups doubles up on a post rather than
	//! dropping the third group somewhere the provider never chose. CalculateGroupCount() normally caps
	//! the count at the post count, so this is reached only when the world loses a post between two
	//! passes.
	//! \param[in] posts The posts the provider offered. May be empty.
	//! \param[in] groupIndex Which of this module's groups, 0-based. Negative reads as 0.
	//! \return The post, or null when there are none.
	static OVT_DeploymentPlacement PostForGroup(notnull array<ref OVT_DeploymentPlacement> posts, int groupIndex)
	{
		if (posts.IsEmpty())
			return null;

		// EnforceScript's % keeps the sign of its left operand, so a negative index would answer a
		// negative slot and index out of bounds. No caller passes one; this says so rather than trusting.
		if (groupIndex < 0)
			groupIndex = 0;

		return posts[groupIndex % posts.Count()];
	}

	//------------------------------------------------------------------------------------------------
	//! Which sideways step an arrival takes on its post.
	//! \param[in] arrivalIndex How many members of this group have already arrived THIS materialisation.
	//!            Negative reads as 0.
	//! \param[in] spread How many steps there are before it wraps - the group's roster size. Zero or
	//!            less falls back to FALLBACK_SPREAD.
	//! \return The step index, 0-based and always inside [0, spread).
	static int SlotForArrival(int arrivalIndex, int spread)
	{
		if (spread <= 0)
			spread = FALLBACK_SPREAD;

		if (arrivalIndex < 0)
			arrivalIndex = 0;

		return arrivalIndex % spread;
	}

	//------------------------------------------------------------------------------------------------
	//! The exact world transform one arriving member is teleported onto.
	//!
	//! The step is taken along the POST'S OWN right vector, not along world X: a sniper marker carries
	//! an authored heading, and a spotter placed one step "east" of a marker facing east would be
	//! standing in front of the sniper rather than beside him.
	//! \param[in] post The post the group stands on.
	//! \param[in] arrivalIndex How many members have already arrived this materialisation.
	//! \param[in] spread The group's roster size; see SlotForArrival.
	//! \param[out] outMat The transform, ready for BaseGameEntity.Teleport().
	static void ArrivalTransform(notnull OVT_DeploymentPlacement post, int arrivalIndex, int spread, out vector outMat[4])
	{
		post.GetTransform(outMat);

		int slot = SlotForArrival(arrivalIndex, spread);
		outMat[3] = outMat[3] + (Vector(MEMBER_SPACING * slot, 0, 0)).Multiply3(outMat);
	}

	//------------------------------------------------------------------------------------------------
	//! Where the arrivalIndex-th member of the groupIndex-th group ends up standing, as a position.
	//!
	//! THE WHOLE RE-MATERIALISATION CLAIM IN ONE CALL. Given the same posts, the same group index and
	//! the same arrival index it answers the same point every time - which is what "the men come back to
	//! the same posts" means once core's survivor mask has decided WHO comes back.
	//! \param[in] posts The posts the provider offered.
	//! \param[in] groupIndex Which of this module's groups.
	//! \param[in] arrivalIndex How many members have already arrived this materialisation.
	//! \param[in] spread The group's roster size; see SlotForArrival.
	//! \return The world position, or vector.Zero when the provider offered no post at all.
	static vector PlacementForArrival(notnull array<ref OVT_DeploymentPlacement> posts, int groupIndex, int arrivalIndex, int spread)
	{
		OVT_DeploymentPlacement post = PostForGroup(posts, groupIndex);
		if (!post)
			return vector.Zero;

		vector mat[4];
		ArrivalTransform(post, arrivalIndex, spread, mat);

		return mat[3];
	}

	//------------------------------------------------------------------------------------------------
	// Placement
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! WANT AS MANY GROUPS AS THERE ARE POSTS, capped by the authored maximum.
	//!
	//! Deliberately ignores the difficulty band and the town-size scaling the base class rolls: this
	//! module's force size is a property of the PLACE, not of the campaign settings. A base with two
	//! towers gets two tower guards on every difficulty.
	//!
	//! ⚠ IT ALSO DELIBERATELY IGNORES m_iMinGroupCount. You cannot man three posts when the world only
	//! offers two, and registering the third somewhere else would put a "placed" group in a place the
	//! provider never chose.
	//!
	//! Answering 0 is normal and is NOT cached: GetMaxGroupCount() only remembers a positive roll, so a
	//! deployment created before its towers were discovered re-asks on the next convergence.
	//! \param[in] position Unused - the posts belong to the deployment's own position, which is what
	//!            the provider is asked about.
	//! \return How many groups to hold.
	override protected int CalculateGroupCount(vector position)
	{
		array<ref OVT_DeploymentPlacement> posts = EnsurePosts();

		int wanted = posts.Count();
		if (wanted > m_iMaxGroupCount)
			wanted = m_iMaxGroupCount;

		return wanted;
	}

	//------------------------------------------------------------------------------------------------
	//! The post the group about to be registered will stand on.
	//!
	//! NO RING ROLL AND NO ROAD SNAP - that is the whole difference between this module and its parent.
	//! The index is m_aHandles.Count() rather than the batch index, because the batch only covers the
	//! shortfall: a pass that reclaimed two groups and registers a third must place it on post 2, not
	//! on post 0.
	//! \param[in] anchor The batch anchor. Used only as the fallback when the world offers no post.
	//------------------------------------------------------------------------------------------------
	//! TRUE: every position this module answers is a post a placement provider chose - a tower walkway,
	//! a sniper marker, a base defend position - and holding it is the entire point of the module. See
	//! the base class for what the false answer protects.
	//! \return True, always.
	override bool StationsGroupsDeliberately()
	{
		return true;
	}

	//! \param[in] index Position within this batch. Unused - see above.
	//! \return The post position, or the anchor when there is nothing to stand on.
	override protected vector ResolveSpawnPosition(vector anchor, int index)
	{
		OVT_DeploymentPlacement post = PostForGroup(EnsurePosts(), m_aHandles.Count());
		if (!post)
			return anchor;

		return post.m_vPosition;
	}

	//------------------------------------------------------------------------------------------------
	//! A new group: remember its post and hook up the placement applier.
	//! \param[in] handle The new group's handle, already in m_aHandles.
	//! \param[in] position Where it was registered. Unused - the post carries a heading too, so it is
	//!            re-read from the provider rather than reconstructed from a bare position.
	override protected void OnGroupRegistered(int handle, vector position)
	{
		AssignPost(handle);
		HookPlacementApplier(handle);
	}

	//------------------------------------------------------------------------------------------------
	//! A group found again in the registry: re-resolve its post and RE-HOOK.
	//!
	//! THE RE-HOOK IS THE LOAD PATH. Core re-creates a restored group's entity from its own payload
	//! with a fresh EntityID, so everything the previous session subscribed to is gone; without this a
	//! continued campaign's tower guards would materialise on the ground beneath their towers and stay
	//! there. Within a session it is a cheap no-op that keeps the post assignment in step with a
	//! handle list a wipe may have shifted.
	//! \param[in] handle The reclaimed group's handle, already in m_aHandles.
	override protected void OnGroupReclaimed(int handle)
	{
		if (m_bReleasing)
			return;

		AssignPost(handle);
		HookPlacementApplier(handle);
	}

	//------------------------------------------------------------------------------------------------
	//! One convergence pass = one provider query. Cleared here rather than in EnsureGroups() because
	//! this is the one method BOTH ways into the convergence (activation and the reinforcement rebuy)
	//! pass through first.
	//! \param[in] virtualization The virtualization manager.
	//! \param[in] ownerKey This module's owner key.
	override protected void ReclaimHandles(notnull OVT_VirtualizationManagerComponent virtualization, string ownerKey)
	{
		m_aPostsThisPass = null;

		super.ReclaimHandles(virtualization, ownerKey);
	}

	//------------------------------------------------------------------------------------------------
	//! A wiped group stops being ours: unhook before the base class forgets the handle.
	//! \param[in] handle The wiped group's registry handle.
	override void OnVirtualGroupWiped(int handle)
	{
		UnhookPlacementApplier(handle);

		m_mPostByHandle.Remove(handle);
		m_mArrivalsByHandle.Remove(handle);

		super.OnVirtualGroupWiped(handle);
	}

	//------------------------------------------------------------------------------------------------
	//! The deployment is over. Unhook EVERYTHING, and stop the base class's own reclaim from hooking it
	//! all back up on the way out.
	//!
	//! ⚠ THIS IS NOT OPTIONAL HYGIENE. UnregisterGroup retires a group with held members IN PLACE
	//! rather than deleting it, so a group entity can outlive this module; an invoker still holding a
	//! method on a cloned config object that nothing references any more is the dangling call D7 exists
	//! to avoid.
	override protected void OnCleanup()
	{
		m_bReleasing = true;

		super.OnCleanup();

		UnhookAll();

		m_mPostByHandle.Clear();
		m_mArrivalsByHandle.Clear();
		m_aPostsThisPass = null;
		m_bReleasing = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Every post the provider offers around this deployment, resolved at most once per convergence
	//! pass.
	//!
	//! The cache and the deployment's own position/faction are all this adds over the pure
	//! ResolvePlacements() above, which is where the provider is actually called.
	//! \return The posts. NEVER null; empty when there is no provider or nothing qualifies.
	protected array<ref OVT_DeploymentPlacement> EnsurePosts()
	{
		if (m_aPostsThisPass)
			return m_aPostsThisPass;

		int factionIndex = -1;
		if (m_ParentDeployment)
			factionIndex = m_ParentDeployment.GetControllingFaction();

		m_aPostsThisPass = ResolvePlacements(GetDeploymentPosition(), m_fSearchRadius, factionIndex);

		return m_aPostsThisPass;
	}

	//------------------------------------------------------------------------------------------------
	//! Gives one handle the post at its own index in the handle list.
	//! \param[in] handle The handle to place.
	protected void AssignPost(int handle)
	{
		int index = m_aHandles.Find(handle);
		if (index == -1)
			return;

		OVT_DeploymentPlacement post = PostForGroup(EnsurePosts(), index);
		if (!post)
			return;

		m_mPostByHandle.Set(handle, post);

		// A handle that has never been placed starts its arrival count at zero; one that is merely
		// being re-confirmed mid-materialisation keeps its count, or two members would be teleported
		// onto the same step.
		if (!m_mArrivalsByHandle.Contains(handle))
			m_mArrivalsByHandle.Set(handle, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Subscribes the placement applier to a handle's CURRENT group entity.
	//!
	//! ⚠ ScriptInvoker.Insert DOES NOT DE-DUPLICATE, so every call removes before it inserts. Without
	//! that, a group reclaimed on every 10 s convergence would accumulate one handler per pass and each
	//! arriving member would be teleported - and counted - once per accumulated subscription.
	//! \param[in] handle The handle to hook.
	protected void HookPlacementApplier(int handle)
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return;

		SCR_AIGroup group = virtualization.GetGroup(handle);
		if (!group)
			return;

		group.GetOnAgentAdded().Remove(OnPlacedAgentAdded);
		group.GetOnAgentAdded().Insert(OnPlacedAgentAdded);

		group.GetOnMembersDespawning().Remove(OnPlacedGroupDespawning);
		group.GetOnMembersDespawning().Insert(OnPlacedGroupDespawning);

		m_mHookedGroupByHandle.Set(handle, group.GetID());
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the placement applier back off one handle's group, if that entity still exists.
	//! \param[in] handle The handle to unhook.
	protected void UnhookPlacementApplier(int handle)
	{
		if (!m_mHookedGroupByHandle.Contains(handle))
			return;

		EntityID hooked = m_mHookedGroupByHandle.Get(handle);
		m_mHookedGroupByHandle.Remove(handle);

		IEntity entity = GetGame().GetWorld().FindEntityByID(hooked);
		if (!entity)
			return;

		SCR_AIGroup group = SCR_AIGroup.Cast(entity);
		if (!group)
			return;

		group.GetOnAgentAdded().Remove(OnPlacedAgentAdded);
		group.GetOnMembersDespawning().Remove(OnPlacedGroupDespawning);
	}

	//------------------------------------------------------------------------------------------------
	//! Unhooks every group this module ever hooked, whatever the handle list says now.
	protected void UnhookAll()
	{
		array<int> hooked = new array<int>();
		for (int i = 0; i < m_mHookedGroupByHandle.Count(); i++)
		{
			hooked.Insert(m_mHookedGroupByHandle.GetKey(i));
		}

		foreach (int handle : hooked)
		{
			UnhookPlacementApplier(handle);
		}

		m_mHookedGroupByHandle.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! A member has spawned into one of this module's groups: put him on his post.
	//!
	//! ⚠ ONE ARGUMENT. The invoker passes the AGENT alone, not (group, agent) - see the class header.
	//! \param[in] agent The freshly spawned member.
	protected void OnPlacedAgentAdded(AIAgent agent)
	{
		if (!agent)
			return;

		SCR_AIGroup group = SCR_AIGroup.Cast(agent.GetParentGroup());
		if (!group)
			return;

		int handle = FindHookedHandle(group);
		if (handle == -1)
			return;

		OVT_DeploymentPlacement post = m_mPostByHandle.Get(handle);
		if (!post)
			return;

		int arrival = 0;
		if (m_mArrivalsByHandle.Contains(handle))
			arrival = m_mArrivalsByHandle.Get(handle);

		BaseGameEntity character = BaseGameEntity.Cast(agent.GetControlledEntity());
		if (character)
		{
			// Teleport, not SetOrigin: SCR_AIGroup snaps a spawned member to the closest navmesh point,
			// which is why this can only be done as each member arrives and never synchronously after
			// registering the group.
			vector mat[4];
			ArrivalTransform(post, arrival, ResolveSpread(group), mat);

			character.Teleport(mat);
		}

		m_mArrivalsByHandle.Set(handle, arrival + 1);
	}

	//------------------------------------------------------------------------------------------------
	//! A group is going dormant: its members are about to be deleted, so the next materialisation
	//! starts placing from the first step again.
	//! \param[in] group The group starting its dormant transition.
	protected void OnPlacedGroupDespawning(SCR_AIGroup group)
	{
		if (!group)
			return;

		int handle = FindHookedHandle(group);
		if (handle == -1)
			return;

		m_mArrivalsByHandle.Set(handle, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! How many sideways steps this group has before the arrival index wraps - its roster size.
	//!
	//! The wrap is a belt to the despawn notification's braces: without it, a missed notification would
	//! march each new arrival another MEMBER_SPACING sideways, forever, and a tower guard would walk off
	//! his walkway over a long campaign.
	//! \param[in] group The arriving member's group.
	//! \return The roster size, or FALLBACK_SPREAD when it cannot be read.
	protected int ResolveSpread(notnull SCR_AIGroup group)
	{
		if (group.m_aUnitPrefabSlots && !group.m_aUnitPrefabSlots.IsEmpty())
			return group.m_aUnitPrefabSlots.Count();

		return FALLBACK_SPREAD;
	}

	//------------------------------------------------------------------------------------------------
	//! The handle whose group entity this is.
	//!
	//! Asked of the hooked-group map rather than of the registry, because this runs once per arriving
	//! member and the map is exactly the set this module subscribed to.
	//! \param[in] group The group entity.
	//! \return Its handle, or -1 when it is not one of ours.
	protected int FindHookedHandle(notnull SCR_AIGroup group)
	{
		EntityID id = group.GetID();

		for (int i = 0; i < m_mHookedGroupByHandle.Count(); i++)
		{
			if (m_mHookedGroupByHandle.GetElement(i) == id)
				return m_mHookedGroupByHandle.GetKey(i);
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many posts the provider currently offers around this deployment.
	int GetPostCount()
	{
		return EnsurePosts().Count();
	}
}
