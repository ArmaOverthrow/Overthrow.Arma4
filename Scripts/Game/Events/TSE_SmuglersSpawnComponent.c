class TSE_SmuglersSpawnComponentClass : ScriptComponentClass {}

class TSE_SmuglersSpawnComponent : ScriptComponent
{
    override void OnPostInit(IEntity owner)
    {
        super.OnPostInit(owner);
        OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
        if(!mode) return;
        TSE_SmuglersEventManagerComponent mgr = TSE_SmuglersEventManagerComponent.Cast(mode.FindComponent(TSE_SmuglersEventManagerComponent));
        if(mgr)
        {
            mgr.RegisterSpawnMarker(owner);
        }
    }
} 