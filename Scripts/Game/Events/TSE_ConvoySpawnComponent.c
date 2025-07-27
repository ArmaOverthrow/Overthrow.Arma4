class TSE_ConvoySpawnComponentClass : ScriptComponentClass {}

class TSE_ConvoySpawnComponent : ScriptComponent
{
    override void OnPostInit(IEntity owner)
    {
        super.OnPostInit(owner);
        OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
        if(!mode) return;
        TSE_ConvoyEventManagerComponent mgr = TSE_ConvoyEventManagerComponent.Cast(mode.FindComponent(TSE_ConvoyEventManagerComponent));
        if(mgr)
        {
            mgr.RegisterConvoySpawnMarker(owner);
        }
    }
} 