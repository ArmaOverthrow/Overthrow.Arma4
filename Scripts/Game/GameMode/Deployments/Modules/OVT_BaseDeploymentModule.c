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
	//! WHAT THIS MODULE COSTS AT A PARTICULAR PLACE, which for almost every module is just what it
	//! costs.
	//!
	//! ==========================================================================================
	//! 🔴 A DEPLOYMENT MUST NEVER PAY FOR SOMETHING IT CANNOT PLACE (author, 2026-08-22).
	//! ==========================================================================================
	//! *"They shouldn't have to pay for unplaceable positions, some bases simply don't have the slots or
	//! anywhere to put them."*
	//!
	//! A deployment's price is computed from the CONFIG TEMPLATE - OVT_DeploymentConfig
	//! .GetTotalResourceCost() walks authored modules, not a live deployment - and the template knows
	//! nothing about the base it is about to be built at. For everything that arrives by truck or on
	//! foot that is correct and deliberate: a force is delivered wherever it is sent. It is NOT correct
	//! for a structure that needs an authored slot, because a base with none simply cannot take it, and
	//! the charge went through anyway with nothing to show for it and nothing logged as an error.
	//!
	//! ⚠ THE DEFAULT IS "the same as anywhere else", SO THIS COSTS EXISTING MODULES NOTHING. Only
	//! OVT_CompositionSpawningDeploymentModule overrides it, because it is the only module whose
	//! delivery can be refused by the ground itself. A future module with the same property overrides
	//! this and nothing else changes.
	//! \param[in] position Where the deployment would be created.
	//! \return The cost to charge at that position.
	int GetResourceCostAt(vector position)
	{
		return GetResourceCost();
	}

	//------------------------------------------------------------------------------------------------
	//! Whether this module wants to build a composition AND could actually place it here.
	//!
	//! ⚠ FALSE IS THE ANSWER FOR "I DO NOT BUILD COMPOSITIONS AT ALL", which is every module but one -
	//! so this must never be read as "this module is unhappy". Its one consumer,
	//! OVT_DeploymentConfig.CanPlaceCompositionsAt(), counts how many modules WANT a slot and how many
	//! of those got one, and a module that wants none is invisible to both counts.
	//! \param[in] position Where the deployment would be created.
	//! \return True only for a composition module that has somewhere to put its structure.
	bool CanPlaceCompositionAt(vector position)
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when this module builds a composition and therefore needs a slot at all.
	bool WantsCompositionSlot()
	{
		return false;
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