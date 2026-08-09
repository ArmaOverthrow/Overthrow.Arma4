# UI Context Patterns

Complete guide for UI contexts, layout management, and OVT_UIManagerComponent integration.

---

## Overview

Overthrow UI system uses contexts that extend OVT_UIContext. Each context manages a .layout file and its lifecycle. The OVT_UIManagerComponent on the player entity handles context activation and management.

---

## Basic UI Context Pattern

### When to Use

Create a UI context when you need:
- A new screen/menu in the game
- Modal dialogs or overlays
- Persistent UI elements
- Interactive forms or interfaces

### Pattern Structure

```cpp
class OVT_MyContext : OVT_UIContext
{
    // Layout file (configured in Workbench)
    override ResourceName GetLayout()
    {
        return "{Path}/MyLayout.layout";
    }

    // Called when context is shown
    override void OnShow()
    {
        super.OnShow();

        // Initialize widgets
        // Bind data to UI
        // Set up event handlers
    }

    // Called when context is hidden
    override void OnHide()
    {
        super.OnHide();

        // Clean up
        // Unbind data
        // Remove event handlers
    }

    // Called every frame when visible
    override void OnUpdate(float timeSlice)
    {
        super.OnUpdate(timeSlice);

        // Update dynamic UI elements
    }
}
```

### Key Points

- **Extend OVT_UIContext:** Base class for all Overthrow contexts
- **GetLayout():** Returns path to .layout file
- **OnShow():** Initialize UI when context becomes visible
- **OnHide():** Clean up when context hidden
- **OnUpdate():** Called every frame for dynamic updates

---

## Activating UI Contexts

### Basic Activation

```cpp
void ShowMyUI()
{
    OVT_UIManagerComponent ui = OVT_Global.GetUI();
    if (!ui) return; // No UI manager (server?)

    // Get context instance
    OVT_MyContext context = OVT_MyContext.Cast(ui.GetContext(OVT_MyContext));
    if (!context) return; // Context not registered

    // Set data before showing
    context.SetData(someData);

    // Show the context
    ui.ShowContext(OVT_MyContext);
}
```

### With Data Passing

```cpp
class OVT_ShopContext : OVT_UIContext
{
    protected ref OVT_ShopData m_ShopData;

    void SetShop(OVT_ShopData shop)
    {
        m_ShopData = shop;
    }

    override void OnShow()
    {
        super.OnShow();

        // Use shop data to populate UI
        if (m_ShopData)
        {
            UpdateInventoryDisplay();
            UpdatePrices();
        }
    }
}

// Activating with data
void OpenShop(OVT_ShopData shop)
{
    OVT_UIManagerComponent ui = OVT_Global.GetUI();
    if (!ui) return;

    OVT_ShopContext context = OVT_ShopContext.Cast(ui.GetContext(OVT_ShopContext));
    if (!context) return;

    // Pass data to context
    context.SetShop(shop);

    // Show context
    ui.ShowContext(OVT_ShopContext);
}
```

---

## Context Registration

### Registering Contexts

Contexts must be registered with OVT_UIManagerComponent (typically on the player prefab in Workbench):

1. Open player prefab in Workbench
2. Find OVT_UIManagerComponent
3. Add context class to registered contexts list
4. Save prefab

### Checking Registration

```cpp
OVT_UIManagerComponent ui = OVT_Global.GetUI();
if (!ui) return;

OVT_MyContext context = OVT_MyContext.Cast(ui.GetContext(OVT_MyContext));
if (!context)
{
    Print("ERROR: OVT_MyContext not registered with UI Manager!", LogLevel.ERROR);
    return;
}
```

---

## Widget Access and Binding

### Finding Widgets

```cpp
class OVT_MyContext : OVT_UIContext
{
    protected TextWidget m_TitleText;
    protected ButtonWidget m_ConfirmButton;
    protected ImageWidget m_IconImage;

    override void OnShow()
    {
        super.OnShow();

        // Find widgets by name (defined in .layout file)
        m_TitleText = TextWidget.Cast(m_Root.FindAnyWidget("TitleText"));
        m_ConfirmButton = ButtonWidget.Cast(m_Root.FindAnyWidget("ConfirmButton"));
        m_IconImage = ImageWidget.Cast(m_Root.FindAnyWidget("IconImage"));

        // Check widgets found
        if (!m_TitleText || !m_ConfirmButton || !m_IconImage)
        {
            Print("ERROR: Failed to find required widgets!", LogLevel.ERROR);
            return;
        }

        // Bind event handlers
        m_ConfirmButton.SetHandler(new SCR_ConfirmButtonHandler(this));

        // Set initial values
        m_TitleText.SetText("My Title");
        m_IconImage.LoadImageTexture(0, "{Path}/icon.edds");
    }
}
```

### Widget Event Handlers

```cpp
class SCR_ConfirmButtonHandler : ScriptedWidgetEventHandler
{
    protected OVT_MyContext m_Context;

    void SCR_ConfirmButtonHandler(OVT_MyContext context)
    {
        m_Context = context;
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (button != 0) return false; // Left click only

        m_Context.OnConfirmClicked();
        return true;
    }
}

class OVT_MyContext : OVT_UIContext
{
    void OnConfirmClicked()
    {
        // Handle confirm button click
        ProcessConfirmation();

        // Close UI
        OVT_UIManagerComponent ui = OVT_Global.GetUI();
        if (ui) ui.HideContext(OVT_MyContext);
    }
}
```

---

## Dynamic Widget Creation

### Creating Widgets at Runtime

```cpp
class OVT_ListContext : OVT_UIContext
{
    protected Widget m_ListContainer;
    protected ref array<Widget> m_ListItems;

    override void OnShow()
    {
        super.OnShow();

        m_ListContainer = m_Root.FindAnyWidget("ListContainer");
        if (!m_ListContainer) return;

        m_ListItems = new array<Widget>();

        // Create list items dynamically
        array<string> items = GetListData();
        foreach (string item : items)
        {
            CreateListItem(item);
        }
    }

    protected void CreateListItem(string text)
    {
        // Create widget from template layout
        Widget itemWidget = GetGame().GetWorkspace().CreateWidgets(
            "{Path}/ListItemTemplate.layout",
            m_ListContainer
        );

        if (!itemWidget) return;

        // Configure widget
        TextWidget textWidget = TextWidget.Cast(itemWidget.FindAnyWidget("ItemText"));
        if (textWidget)
        {
            textWidget.SetText(text);
        }

        m_ListItems.Insert(itemWidget);
    }

    override void OnHide()
    {
        // Clean up created widgets
        foreach (Widget item : m_ListItems)
        {
            item.RemoveFromHierarchy();
        }
        m_ListItems.Clear();

        super.OnHide();
    }
}
```

---

## Context Lifecycle Management

### Modal Contexts

```cpp
class OVT_ModalContext : OVT_UIContext
{
    protected OVT_UIContext m_PreviousContext;

    void SetPreviousContext(OVT_UIContext previous)
    {
        m_PreviousContext = previous;
    }

    void CloseModal()
    {
        OVT_UIManagerComponent ui = OVT_Global.GetUI();
        if (!ui) return;

        // Hide this modal
        ui.HideContext(OVT_ModalContext);

        // Restore previous context if needed
        if (m_PreviousContext)
        {
            ui.ShowContext(m_PreviousContext.Type());
        }
    }
}
```

### Context Stack

```cpp
// Show new context, hiding current
void PushContext(typename contextType)
{
    OVT_UIManagerComponent ui = OVT_Global.GetUI();
    if (!ui) return;

    typename currentType = ui.GetActiveContextType();

    // Show new context (automatically hides previous)
    ui.ShowContext(contextType);

    // Store previous for back navigation
    StoreContextHistory(currentType);
}

// Return to previous context
void PopContext()
{
    OVT_UIManagerComponent ui = OVT_Global.GetUI();
    if (!ui) return;

    typename previousType = GetPreviousContextType();
    if (previousType)
    {
        ui.ShowContext(previousType);
    }
}
```

---

## Input Handling

### Keyboard Input

```cpp
class OVT_MyContext : OVT_UIContext
{
    override void OnShow()
    {
        super.OnShow();

        // Register for input events
        GetGame().GetInputManager().AddActionListener("MenuBack", EActionTrigger.DOWN, OnMenuBack);
    }

    override void OnHide()
    {
        // Unregister input events
        GetGame().GetInputManager().RemoveActionListener("MenuBack", EActionTrigger.DOWN, OnMenuBack);

        super.OnHide();
    }

    protected void OnMenuBack()
    {
        // Handle back/escape key
        CloseContext();
    }

    protected void CloseContext()
    {
        OVT_UIManagerComponent ui = OVT_Global.GetUI();
        if (ui) ui.HideContext(OVT_MyContext);
    }
}
```

---

## Server Communication from UI

### Requesting Server Actions

```cpp
class OVT_ShopContext : OVT_UIContext
{
    void OnPurchaseClicked(int itemId)
    {
        // Get player's controller
        OVT_OverthrowController controller = OVT_Global.GetController();
        if (!controller) return;

        // Get shop component on controller
        OVT_ShopServerComponent shopComponent = OVT_ShopServerComponent.Cast(
            controller.FindComponent(OVT_ShopServerComponent)
        );
        if (!shopComponent) return;

        // Request server to process purchase
        shopComponent.RequestPurchase(itemId);

        // UI will be updated via RPC callback when server responds
    }
}

// In OVT_ShopServerComponent on OVT_OverthrowController
void RequestPurchase(int itemId)
{
    if (Replication.IsServer())
    {
        RpcAsk_Purchase(itemId);
    }
    else
    {
        Rpc(RpcAsk_Purchase, itemId);
    }
}

[RplRpc(RplChannel.Reliable, RplRcver.Server)]
void RpcAsk_Purchase(int itemId)
{
    // Server validates and processes purchase
    // ...

    // Send result back to client
    Rpc(RpcDo_PurchaseResult, success, message);
}

[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
void RpcDo_PurchaseResult(bool success, string message)
{
    // Update UI with result
    OVT_UIManagerComponent ui = OVT_Global.GetUI();
    if (!ui) return;

    OVT_ShopContext context = OVT_ShopContext.Cast(ui.GetContext(OVT_ShopContext));
    if (context)
    {
        context.OnPurchaseResult(success, message);
    }
}
```

---

## Best Practices

### ✅ DO:

- **Clean up widgets:** Remove dynamically created widgets in OnHide()
- **Check GetUI():** May return null on server
- **Check GetContext():** May return null if not registered
- **Set data before showing:** Call SetData() before ShowContext()
- **Unregister input:** Remove input listeners in OnHide()
- **Use strong refs:** For widget and data references
- **Validate server requests:** Never trust client-side data

### ❌ DON'T:

- **Store IEntity in context:** Store EntityID instead
- **Skip null checks:** Always verify UI manager and context exist
- **Forget to clean up:** Memory leaks from widgets and event handlers
- **Block main thread:** Keep OnUpdate() lightweight
- **Assume UI exists:** Check GetUI() on both client and server code
- **Trust client data:** Validate all user input on server

---

## Testing UI Contexts

### Manual Testing
1. Open UI context from game
2. Verify layout loads correctly
3. Test all interactive elements (buttons, inputs)
4. Test data display updates
5. Test closing/hiding context
6. Verify cleanup (no lingering widgets)

### Edge Cases
1. Test with null/invalid data
2. Test rapid open/close
3. Test multiple contexts stacking
4. Test on both host and dedicated server
5. Test with missing layout files
6. Test with missing widgets in layout

---

## Common Issues

### Issue: Context doesn't show
**Cause:** Not registered with OVT_UIManagerComponent
**Fix:** Add context to player prefab's UI manager component

### Issue: Widgets null after FindAnyWidget
**Cause:** Widget names in code don't match .layout file
**Fix:** Verify widget names match exactly (case-sensitive)

### Issue: UI shows on dedicated server
**Cause:** Not checking if UI manager exists
**Fix:** Always check `if (!OVT_Global.GetUI()) return;`

### Issue: Memory leak from widgets
**Cause:** Not removing dynamically created widgets
**Fix:** Call RemoveFromHierarchy() in OnHide()

---

## Related Resources

- See `component-patterns.md` for OVT_UIManagerComponent architecture
- See `networking.md` for server communication from UI
- See `memory-management.md` for widget cleanup
- See main `SKILL.md` for overview
