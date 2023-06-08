modded class EPF_CharacterControllerComponentSaveData
{
    //------------------------------------------------------------------------------------------------
    override EPF_EApplyResult ApplyTo(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
    {
        PrintFormat("EPF_CharacterControllerComponentSaveData::ApplyTo(%1, %1)", EPF_Utils.GetPrefabName(owner), m_sRightHandItemId);

        return super.ApplyTo(owner, component, attributes);
    }

    //------------------------------------------------------------------------------------------------
    override protected void ListenForWeaponEquipComplete(AnimationEventID animEventType)
    {
        PrintFormat("ListenForWeaponEquipComplete(%1, %2)", GameAnimationUtils.GetEventString(animEventType), m_sRightHandItemId);

        super.ListenForWeaponEquipComplete(animEventType);
    }
};