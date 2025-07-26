class TSE_CivDriverSpawnComponentClass : ScriptComponentClass {}

class TSE_CivDriverSpawnComponent : ScriptComponent
{
    override void OnPostInit(IEntity owner)
    {
        super.OnPostInit(owner);
        // Найдём менеджер и зарегистрируемся
        OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
        if(!mode) return;
        TSE_CivDriversManagerComponent mgr = TSE_CivDriversManagerComponent.Cast(mode.FindComponent(TSE_CivDriversManagerComponent));
        if(mgr)
        {
            mgr.RegisterSpawnMarker(owner);
        }
    }
} 