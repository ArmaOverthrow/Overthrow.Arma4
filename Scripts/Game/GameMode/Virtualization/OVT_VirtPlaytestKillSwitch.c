//! [OVT-VIRT-PLAYTEST-ONLY] TEMPORARY - MUST NOT SHIP. Silences every systemic legacy AI spawner so a
//! virtualization/core play-test world contains only what the virtualization layer spawns. Flip
//! DISABLE_LEGACY_AI_SPAWNS to false (or delete this file and its tagged call sites) to restore.
//!
//! Removal set: grep -rn "OVT-VIRT-PLAYTEST-ONLY" Scripts/
class OVT_VirtPlaytestKillSwitch
{
	//! True = no legacy systemic AI is created anywhere in the campaign.
	static const bool DISABLE_LEGACY_AI_SPAWNS = true;
}
