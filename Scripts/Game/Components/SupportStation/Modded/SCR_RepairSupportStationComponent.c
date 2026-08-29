//------------------------------------------------------------------------------------------------
//! Frees field repair from the vanilla supply economy Overthrow does not run - covers the handheld
//! wrench and vehicle-mounted/industrial repair stations alike. Fuel, medical and rearm stations are
//! different classes and are untouched.
modded class SCR_RepairSupportStationComponent : SCR_BaseDamageHealSupportStationComponent
{
	override bool AreSuppliesEnabled()
	{
		return false;
	}
}
