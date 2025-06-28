[BaseContainerProps(configRoot: true)]
class OVT_BaseDeploymentModule
{
	protected OVT_DeploymentComponent m_ParentDeployment;
	protected bool m_bInitialized;
	protected bool m_bActive;
	
	//------------------------------------------------------------------------------------------------
	void Initialize(OVT_DeploymentComponent parent)
	{
		m_ParentDeployment = parent;
		m_bInitialized = true;
		OnInitialize();
	}
	
	//------------------------------------------------------------------------------------------------
	void Activate()
	{
		if (m_bActive)
			return;
			
		m_bActive = true;
		OnActivate();
	}
	
	//------------------------------------------------------------------------------------------------
	void Deactivate()
	{
		if (!m_bActive)
			return;
			
		m_bActive = false;
		OnDeactivate();
	}
	
	//------------------------------------------------------------------------------------------------
	void Update(int deltaTime)
	{
		if (!m_bActive)
			return;
			
		OnUpdate(deltaTime);
	}
	
	//------------------------------------------------------------------------------------------------
	void Cleanup()
	{
		if (m_bActive)
			Deactivate();
			
		OnCleanup();
		m_bInitialized = false;
	}
	
	//------------------------------------------------------------------------------------------------
	// Resource management
	//------------------------------------------------------------------------------------------------
	int GetResourceCost()
	{
		return 0; // Override in derived classes
	}
	
	//------------------------------------------------------------------------------------------------
	bool CanAfford(int availableResources)
	{
		return availableResources >= GetResourceCost();
	}
	
	//------------------------------------------------------------------------------------------------
	// Virtual methods for module-specific behavior
	//------------------------------------------------------------------------------------------------
	protected void OnInitialize() {}
	protected void OnActivate() {}
	protected void OnDeactivate() {}
	protected void OnUpdate(int deltaTime) {}
	protected void OnCleanup() {}
	
	//------------------------------------------------------------------------------------------------
	// Utility methods
	//------------------------------------------------------------------------------------------------
	protected OVT_DeploymentComponent GetDeployment()
	{
		return m_ParentDeployment;
	}
	
	//------------------------------------------------------------------------------------------------
	protected vector GetDeploymentPosition()
	{
		if (m_ParentDeployment)
			return m_ParentDeployment.GetPosition();
			
		return vector.Zero;
	}
	
	//------------------------------------------------------------------------------------------------
	protected int GetControllingFaction()
	{
		if (m_ParentDeployment)
			return m_ParentDeployment.GetControllingFaction();
			
		return -1;
	}
	
	//------------------------------------------------------------------------------------------------
	protected float GetThreatLevel()
	{
		if (m_ParentDeployment)
			return m_ParentDeployment.GetThreatLevel();
			
		return 0;
	}
	
	//------------------------------------------------------------------------------------------------
	// Cloning support for instantiation from config
	//------------------------------------------------------------------------------------------------
	OVT_BaseDeploymentModule CloneModule()
	{
		// Create new instance of the same type
		typename type = Type();
		OVT_BaseDeploymentModule newModule = OVT_BaseDeploymentModule.Cast(type.Spawn());
		
		if (newModule)
			CopyTo(newModule);
			
		return newModule;
	}
	
	//------------------------------------------------------------------------------------------------
	protected void CopyTo(OVT_BaseDeploymentModule target)
	{
		// Override in derived classes to copy attributes
	}
}