//! Map-gadget helper for the player character: find the map gadget, raise it, stow it. NOT a map UI
//! class - Overthrow's map UI is OVT_OverthrowMapUI (registered from Configs/Map/MapFullscreen.conf),
//! which owns markers, info panels and travel. This context draws nothing and holds no map state.
//! map/legacy-retirement removed its three flag modes (map info, fast travel, bus travel), their
//! MapSelect/MenuBack/GadgetMap listeners and the static map-close invoker subscription behind them.
class OVT_MapContext : OVT_UIContext
{
	SCR_MapGadgetComponent GetMap()
	{
		SCR_GadgetManagerComponent mgr = SCR_GadgetManagerComponent.Cast(m_Owner.FindComponent(SCR_GadgetManagerComponent));
		if(!mgr) return null;
		
		IEntity ent = mgr.GetQuickslotGadgetByType(EGadgetType.MAP);
		if(!ent) {		
			ent = mgr.GetGadgetByType(EGadgetType.MAP);
		}
		
		if(!ent) return null;
				
		SCR_MapGadgetComponent comp = SCR_MapGadgetComponent.Cast(ent.FindComponent(SCR_MapGadgetComponent));
		if(!comp) return null;
		
		return comp;
	}
	
	bool ShowMap()
	{
		SCR_MapGadgetComponent comp = GetMap();
		if(!comp) return false;
		
		comp.SetMapMode(true);
		return true;
	}
	
	//! Hide the map AND stow the map item.
	//!
	//! Root cause: Overthrow closed the map by calling SetMapMode(false) directly, which only toggles the map
	//! *view*. Vanilla stows the gadget (SetGadgetMode(..., IN_STORAGE)) and lets the view close as a
	//! consequence, so closing the view alone left the map raised in the character's hand.
	//!
	//! Focus is cleared first because SetGadgetMode's hand->storage branch early-returns for EGadgetType.MAP
	//! and so never closes the view itself. ToggleFocused(false) calls SetMapMode(false) internally.
	//!
	//! NO CALLERS TODAY: all eight (one mode-exit, seven map-click) went with the legacy modes in
	//! map/legacy-retirement, which retained this method "for map/respawn". That turned out to be wrong
	//! and the claim is corrected here rather than repeated: map/respawn shipped without touching the map
	//! GADGET at all. Its screen is a SPAWNSCREEN map opened with SCR_MapEntity.OpenMap and closed with
	//! CloseMap, and a gadget map the player had raised when they died is already closed by the
	//! FULLSCREEN life-state hook. So this method currently has no consumer, named or otherwise.
	//!
	//! KEPT ANYWAY, deliberately: the ordering above came from play-test, not from any gate, and would be
	//! re-derived wrong if cut. Deleting it is a separate cleanup that should carry its own grep proof;
	//! note the same ordering also lives in OVT_OverthrowMapUI's HideMap/StowMapGadget pair, so the
	//! knowledge is not single-sourced here. Not OVT_OverthrowMapUI's own pair.
	void HideMap()
	{
		SCR_MapGadgetComponent comp = GetMap();
		if(!comp) return;

		comp.ToggleFocused(false);

		IEntity gadget = comp.GetOwner();
		if(!gadget || !m_Owner) return;

		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(m_Owner.FindComponent(SCR_CharacterControllerComponent));
		if(controller && controller.IsDead()) return;

		SCR_GadgetManagerComponent gadgetManager = SCR_GadgetManagerComponent.GetGadgetManager(m_Owner);
		if(!gadgetManager) return;

		// Only stow if the map is what is actually in hand - never stow whatever else the player is holding
		if(gadgetManager.GetHeldGadget() != gadget) return;

		gadgetManager.SetGadgetMode(gadget, EGadgetMode.IN_STORAGE);
	}
	
	//! Open the map and arm nothing at all. OVT_CatchBusAction's entry point since map/fast-travel Phase 4
	//! and this class's only external caller. There is no mode to set: bus eligibility is re-derived from
	//! the player's live position each time OVT_OverthrowMapUI builds an info panel (BUG-069 part 2).
	void OpenMap()
	{
		if(!ShowMap())
		{
			ShowNotification("MustHaveMap");
		}
	}
}
