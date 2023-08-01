//------------------------------------------------------------------------------------------------
class OVT_TimeAndWeatherHandlerComponentClass: SCR_TimeAndWeatherHandlerComponentClass
{
};

//------------------------------------------------------------------------------------------------
class OVT_TimeAndWeatherHandlerComponent : SCR_TimeAndWeatherHandlerComponent
{
	float GetDayTimeMultiplier()
	{
		return m_fDayTimeAcceleration;
	}
}