//------------------------------------------------------------------------------------------------
//! Informational logging for the deployment manager, the objective director and every module of
//! either. These lines were built as debugging aids while those systems were being written; the
//! systems now work, and at 30 s evaluator passes plus a 10 s hunter-killer tick they were the bulk
//! of a server's log.
//!
//! ⚠ LogLevel.VERBOSE is NOT a sink - it prints to console.log like everything else (proven against
//! a real Workbench log, where the VERBOSE cooldown refusals appear 24 times). Silencing has to be a
//! gate in script, which is what this is.
//!
//! Gated on the game mode's existing OVT_OverthrowConfigComponent.m_bDebugMode, the same flag the
//! director's own LogSelectionRound already used. Warnings, errors and the on-demand
//! Print*DebugInfo() console dumps are deliberately NOT routed through here.
class OVT_DeploymentLog
{
	//------------------------------------------------------------------------------------------------
	static void Debug(string message)
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config || !config.m_bDebugMode)
			return;

		Print(message, LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! For a caller that carries a level through from its own callers - the director's refusal helpers
	//! do. WARNING and above are never gated; anything below is a debug line like any other.
	static void Log(string message, LogLevel level)
	{
		if (level == LogLevel.WARNING || level == LogLevel.ERROR || level == LogLevel.FATAL)
		{
			Print(message, level);
			return;
		}

		Debug(message);
	}
}
