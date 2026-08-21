//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "Overthrow/Components", description: "Marks an entity as a dropped resource crate pile")]
class OVT_ResourcePileComponentClass : OVT_ComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Identity marker for a dropped resource crate pile. It holds no state and does no work.
//!
//! WHY IT EXISTS (D16). An entity gets exactly ONE EntityPersistenceConfig, so a
//! ComponentClassPersistenceConfigRule on OVT_ResourceStoreComponent would hijack every truck,
//! warehouse and building away from the configuration it already matches. This component exists on
//! exactly one prefab, so a rule may safely name it - and it is also the cheapest "is that a pile?"
//! test for the merge query and the supply query, neither of which may match a parked truck.
//!
//! The construction site deliberately gets no equivalent: it carries OVT_BuildableComponent, and the
//! Overthrow Buildable configuration already claims it.
//------------------------------------------------------------------------------------------------
class OVT_ResourcePileComponent : OVT_Component
{
}
