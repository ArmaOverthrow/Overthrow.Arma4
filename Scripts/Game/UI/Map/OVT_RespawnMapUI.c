//------------------------------------------------------------------------------------------------
//! The respawn screen's map container. Everything the living fullscreen map does, minus travel, plus
//! a "Respawn here" button on the selected marker's info panel.
//!
//! WHY A SUBCLASS AND NOT A FLAG. "Respawn here" versus "Fast Travel" is genuinely different
//! behaviour, and a subclass keeps that difference out of the living map's hot path entirely. There
//! is no runtime mode boolean anywhere in this feature - the living map instantiates
//! OVT_OverthrowMapUI against OverthrowMap.conf and this class is never constructed for it, so there
//! is nothing that can be left armed after a respawn (the defect class BUG-069 was).
//!
//! WHY IT OVERRIDES EXACTLY ONE METHOD, AND DOES NOT CALL SUPER. SetupTravelButton is the only place
//! the base class builds travel affordances. The base's own first act after caching the panel
//! location is "no FastTravelButton widget? return", so omitting the widget from the respawn panel
//! would already disable travel - but only by accident of the layout. Overriding without super makes
//! it structural: dropping a FastTravelButton into OVT_MapInfoPanelRespawn.layout later cannot
//! silently re-enable fast travel, because no code on this screen ever looks for one.
//!
//! THE RESPAWN MAP CONFIG (Configs/Map/OverthrowMapRespawn.conf), and why three of its attributes are
//! zero. Duplicated here because .conf comments are not attested anywhere in either source tree and a
//! Workbench re-save would strip them:
//!  - m_fVisibilityZoom 0 satisfies "every eligible location is visible at every zoom level" through
//!    UNMODIFIED OVT_MapLocationElement.SetVisible. The attribute is per-instance, so the living map
//!    keeps its own thresholds and pays nothing for this screen.
//!  - m_fRefreshInterval 0 stops reconciliation destroying elements underneath a screen the player
//!    cannot leave. The server re-derives eligibility on arrival, so a stale marker costs a
//!    fallback-to-home, never a wrong spawn.
//!  - m_bShowDistance 0 because both marker-side distance paths resolve the local controlled entity
//!    and answer -1 without one. The info panel's distance row is a third path that ignores the
//!    attribute; it is suppressed by the respawn panel layout carrying no "Distance" widget at all.
//------------------------------------------------------------------------------------------------
class OVT_RespawnMapUI : OVT_OverthrowMapUI
{
	//------------------------------------------------------------------------------------------------
	//! Build the respawn control for one location, in place of the base class's travel controls.
	//!
	//! Deliberately does NOT call super - see the class header. Mirrors SetupCloseButton's shape
	//! rather than SetupTravelButton's: there is no verb to choose, no fare to price and no recruit
	//! opt-out, so the button is simply shown and wired. Every lookup is null-guarded so a stale
	//! layout degrades to a missing button rather than a script error on an unskippable screen.
	//! \param[in] location The location the panel is describing.
	override protected void SetupTravelButton(OVT_MapLocationData location)
	{
		if (!m_wInfoPanel || !location)
			return;

		// The base class caches this so the recruit toggle can rebuild in place. Nothing on this
		// screen re-enters, but OnRespawnClicked reads the SELECTED element rather than this field,
		// exactly as OnTravelClicked does, so the two stay in step.
		m_PanelLocation = location;

		ButtonWidget respawnButton = ButtonWidget.Cast(m_wInfoPanel.FindAnyWidget("RespawnButton"));
		if (!respawnButton)
		{
			Print("[Overthrow] Respawn info panel has no RespawnButton - the player cannot pick this location", LogLevel.ERROR);
			return;
		}

		// Every record that reaches this panel already passed CanRespawn, because m_bRespawnOnly
		// filtered the map down to eligible records before any of them were drawn. There is no
		// "shown but refused" state to render, which is why this panel carries no reason text.
		respawnButton.SetVisible(true);

		SCR_InputButtonComponent respawnComp = SCR_InputButtonComponent.Cast(respawnButton.FindHandler(SCR_InputButtonComponent));
		if (!respawnComp)
		{
			Print("[Overthrow] RespawnButton has no SCR_InputButtonComponent - check the inherited component GUID", LogLevel.ERROR);
			return;
		}

		// ShowLocationInfo destroys and recreates the whole panel on every hover and pin, so this is
		// a fresh component instance each time; Clear() is belt-and-braces against a future in-place
		// rebuild. m_OnActivated, not m_OnClicked: it is the one invoker reached by BOTH the mouse
		// path and the bound-action path, so mouse, keyboard and pad all arrive here exactly once.
		respawnComp.m_OnActivated.Clear();
		respawnComp.m_OnActivated.Insert(OnRespawnClicked);
	}

	//------------------------------------------------------------------------------------------------
	//! Ask the server to respawn this player at the selected location.
	//!
	//! Mirrors OnTravelClicked with two deliberate differences: it does NOT close the screen (the
	//! server's result does - a refusal must leave the player looking at the map, not at nothing),
	//! and the position it sends is a LOOKUP KEY, not a destination. The server re-derives the
	//! eligible set from its own managers and spawns at its own recorded vector.
	//!
	//! Nothing is validated here. The button the player pressed was drawn because CanRespawn said so
	//! on THIS machine, which is advisory: the server re-derives the eligible set from its own
	//! managers on arrival and may refuse or fall back to home after this screen has already been
	//! sitting open. That is why the refusal path is a hint and a still-open screen rather than a
	//! disabled button.
	protected void OnRespawnClicked()
	{
		if (!m_SelectedElement)
			return;

		OVT_MapLocationData location = m_SelectedElement.GetLocationData();
		if (!location)
			return;

		OVT_RespawnRequestComponent respawn = OVT_ControllerComponent<OVT_RespawnRequestComponent>.Get();
		if (!respawn)
		{
			// A component that exists in script but not on OVT_OverthrowController.et is a silent
			// no-op - the request never leaves this machine, and on a screen the player cannot
			// dismiss that reads as a frozen game. Say so rather than doing nothing.
			Print("[Overthrow] No OVT_RespawnRequestComponent on the local controller - respawn request dropped", LogLevel.ERROR);
			return;
		}

		respawn.RequestRespawn(OVT_RespawnDestination.LOCATION, location.m_vPosition);
	}
}
