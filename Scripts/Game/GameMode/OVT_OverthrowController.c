[EntityEditorProps(category: "Overthrow", description: "Controller entity for overthrow-specific client-server communication")]
class OVT_OverthrowControllerClass : GenericEntityClass
{
}

//------------------------------------------------------------------------------------------------
//! Controller entity owned by each player for modular client-server communication.
//! Components attached to this entity handle specific domains of functionality.
class OVT_OverthrowController : GenericEntity
{
	// Progress event handler (invoked by all components extending OVT_BaseServerProgressComponent)
	protected ref OVT_ProgressEventHandler m_ProgressEvents = new OVT_ProgressEventHandler();
	
	//------------------------------------------------------------------------------------------------
	//! Get the progress event handler
	OVT_ProgressEventHandler GetProgressEvents()
	{
		return m_ProgressEvents;
	}
}