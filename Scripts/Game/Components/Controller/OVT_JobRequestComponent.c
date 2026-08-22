[ComponentEditorProps(category: "Overthrow/Components/Controller", description: "Server-authoritative job accept/decline requests for one player")]
class OVT_JobRequestComponentClass : OVT_ControllerRequestComponentClass {};

//------------------------------------------------------------------------------------------------
//! Server-authoritative job acceptance, on the per-player OVT_OverthrowController entity.
//!
//! Phase 8 of the controller migration (docs/features/core/controller-migration/implementation.md §4).
//! Replaced two handlers on the legacy comms monolith (deleted in Phase 10): accept a job and decline a
//! job. Project rule (overthrow-controller.md): every client->server RPC lives on a controller
//! component like this one.
//!
//! WHAT CHANGED IN THE MOVE:
//!
//! 1. NO IDENTITY ARGUMENT (plan G3/D3). Both handlers took `int playerId` and then overwrote it with
//!    ResolveSenderPlayerId(). The parameter is DELETED, not ignored - the accepting player is the owner
//!    of the controller entity the request arrived on, and nothing else can name them. That matters here
//!    more than the shape of the change suggests: `job.owner` is written from this id, so a spoofed id
//!    assigned another player's name to a job and its reward.
//!
//! 2. THE JOB IS STILL IDENTIFIED BY (jobIndex, townId, baseId), NOT BY A HANDLE. Carried verbatim: jobs
//!    are streamed to clients as records, not as replicated entities, so the triple is the only reference
//!    both machines agree on. The server re-finds the job in its OWN m_aJobs and applies its own
//!    public/private ownership rule to it - the client's copy is never trusted for anything but naming.
//!
//! 3. jobIndex IS BOUNDS-CHECKED. OVT_JobManagerComponent.GetConfig() is a bare m_aJobConfigs[index],
//!    so an out-of-range index from a modified client was an array-out-of-bounds on the server rather
//!    than a rejected request.
//!
//! 4. THE PUBLIC ENTRY POINTS BRANCH ON Replication.IsServer(). The manager only reaches this seam from
//!    its client branch today, so the server twin is unreachable from that call site - it exists because
//!    an unconditional Rpc() from the authority is delivered to nobody (P2-5 / BUG-045 family) and a
//!    future server-side caller must not silently do nothing.
//------------------------------------------------------------------------------------------------
class OVT_JobRequestComponent : OVT_ControllerRequestComponent
{
	//------------------------------------------------------------------------------------------------
	// PUBLIC ENTRY POINTS - client side.
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Accept a job for the local player.
	//! \param[in] job The job record the menu/marker acted on.
	void AcceptJob(OVT_Job job)
	{
		if(!job) return;

		if(Replication.IsServer())
		{
			RpcAsk_AcceptJob(job.jobIndex, job.townId, job.baseId);
		}else{
			Rpc(RpcAsk_AcceptJob, job.jobIndex, job.townId, job.baseId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Decline a job for the local player.
	//! \param[in] job The job record the menu/marker acted on.
	void DeclineJob(OVT_Job job)
	{
		if(!job) return;

		if(Replication.IsServer())
		{
			RpcAsk_DeclineJob(job.jobIndex, job.townId, job.baseId);
		}else{
			Rpc(RpcAsk_DeclineJob, job.jobIndex, job.townId, job.baseId);
		}
	}

	//------------------------------------------------------------------------------------------------
	// SERVER HANDLERS
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Server: accept the named job for the caller.
	//!
	//! The public/private ownership branch is carried verbatim: a PUBLIC job may be taken by anybody who
	//! has not already got it, a PRIVATE one only by the player it was offered to (job.owner holds their
	//! persistent id). Both are re-derived here from the server's own job list.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AcceptJob(int jobIndex, int townId, int baseId)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		OVT_JobManagerComponent jobs = OVT_Global.GetJobs();
		if(!jobs) return;

		if(jobIndex < 0 || jobIndex >= jobs.GetJobConfigCount())
		{
			RejectJobRequest(playerId, "accept", "job index out of range");
			return;
		}

		OVT_JobConfig config = jobs.GetConfig(jobIndex);
		if(!config) return;

		string persId = "";
		if(!config.m_bPublic)
		{
			OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
			if(!players) return;

			persId = players.GetPersistentIDFromPlayerID(playerId);
			if(persId == "") return;
		}

		foreach(OVT_Job job : jobs.m_aJobs)
		{
			if(job.jobIndex != jobIndex) continue;
			if(job.townId != townId) continue;
			if(job.baseId != baseId) continue;
			if(job.accepted) continue;

			if(config.m_bPublic)
			{
				if(job.owner == "")
					jobs.AcceptJob(job, playerId);
			}else{
				if(job.owner == persId)
					jobs.AcceptJob(job, playerId);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: decline the named job for the caller.
	//!
	//! Same ownership branch as accept, and deliberately the same asymmetry the monolith had: declining a
	//! PUBLIC job does not require that the caller had not already accepted it (the manager records the
	//! decline against their persistent id), while a PRIVATE job may only be declined by its owner -
	//! declining it removes the record outright, so anyone else doing it would be destroying another
	//! player's job.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_DeclineJob(int jobIndex, int townId, int baseId)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		OVT_JobManagerComponent jobs = OVT_Global.GetJobs();
		if(!jobs) return;

		if(jobIndex < 0 || jobIndex >= jobs.GetJobConfigCount())
		{
			RejectJobRequest(playerId, "decline", "job index out of range");
			return;
		}

		OVT_JobConfig config = jobs.GetConfig(jobIndex);
		if(!config) return;

		string persId = "";
		if(!config.m_bPublic)
		{
			OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
			if(!players) return;

			persId = players.GetPersistentIDFromPlayerID(playerId);
			if(persId == "") return;
		}

		// The manager mutates m_aJobs while declining a private job (it removes the record), so iterate a
		// snapshot of the matches rather than the live array.
		array<OVT_Job> matches = {};
		foreach(OVT_Job job : jobs.m_aJobs)
		{
			if(job.jobIndex != jobIndex) continue;
			if(job.townId != townId) continue;
			if(job.baseId != baseId) continue;

			if(config.m_bPublic)
			{
				matches.Insert(job);
			}else{
				if(job.owner == persId)
					matches.Insert(job);
			}
		}

		foreach(OVT_Job job : matches)
		{
			jobs.DeclineJob(job, playerId);
		}
	}

	//------------------------------------------------------------------------------------------------
	// HELPERS
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Logs a rejected job request with its reason (quality bar Q9: a rejection is never
	//! indistinguishable from a dropped packet).
	//! \param[in] playerId The rejected player.
	//! \param[in] request Which request was rejected.
	//! \param[in] reason Why.
	protected void RejectJobRequest(int playerId, string request, string reason)
	{
		Print(string.Format("[OVT_JobRequestComponent] Rejected %1 request from player %2: %3", request, playerId.ToString(), reason), LogLevel.WARNING);
	}
}
