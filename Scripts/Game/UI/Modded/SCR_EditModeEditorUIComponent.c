//------------------------------------------------------------------------------------------------
//! Injects Overthrow's read-only GM panel into the Game Master editor's EDIT-mode root.
//!
//! WHY THIS CLASS. SCR_EditModeEditorUIComponent is EMPTY in the base game, its parent's only member
//! is the HandlerAttachedScripted(Widget) hook that receives the Mode_Edit root, and it is referenced
//! by exactly one layout in the whole base tree (Mode_Edit.layout:4). Modding it forks nothing.
//!
//! WHY THE PANEL IS EDIT-MODE-ONLY FOR FREE. Mode_Edit.layout is the hideable layout of
//! EditorModeEdit.et, the only editor mode prefab that does NOT set m_bIsLimited 1. Photo, Building
//! and Screenshot modes use different layouts, so they never reach this hook. No IsLimited() check,
//! no role check and no visibility toggle is needed here - and none should be added.
//!
//! WHY THE PANEL ANCHORS TO THE MODE ROOT, NOT THE LEFT STACK. The bottom-left stack
//! (Mode_Edit_Element_Left) is where the base game's contextual help renders, and hints drew over
//! the panel there (observed in the 2026-08-15 Workbench check). The panel therefore carries its
//! own FrameWidgetSlot (top-left, 100 px down) and is created directly under the Mode_Edit root,
//! which is a FrameWidgetClass (Mode_Edit.layout:1).
//------------------------------------------------------------------------------------------------
modded class SCR_EditModeEditorUIComponent
{
	//! Overthrow's GM panel layout, anchored top-left under the EDIT-mode root.
	protected const ResourceName PANEL_LAYOUT = "{6B08D3A17C4B0000}UI/Layouts/GM/GMPanel.layout";

	//------------------------------------------------------------------------------------------------
	//! Fires once per EDIT-mode activation, one frame after the mode root is built
	//! (MenuRootSubComponent.c:82 defers it), so the full tree exists when the panel is created.
	//! \param[in] w Mode_Edit layout root.
	override protected void HandlerAttachedScripted(Widget w)
	{
		// SCR_BaseModeEditorUIComponent hides the active hint here - keep it.
		super.HandlerAttachedScripted(w);

		if (!w)
			return;

		Widget panel = GetGame().GetWorkspace().CreateWidgets(PANEL_LAYOUT, w);
		if (!panel)
			Print("OVT: could not create " + PANEL_LAYOUT + " - the GM panel will be absent this session (missing .layout.meta?)", LogLevel.WARNING);
	}
}
