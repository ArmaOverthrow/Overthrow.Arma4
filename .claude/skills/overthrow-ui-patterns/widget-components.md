# Widget Components — Cards, Tabs and Dynamic Rows

For repeated UI elements (a shop card, a category tab, a warehouse row) the
pattern is a small `SCR_ScriptedWidgetComponent` subclass attached to a sub-layout,
instantiated at runtime by the owning `OVT_UIContext`.

Files: `Scripts/Game/UI/Menu/<MenuName>/OVT_<Name>Component.c`
Layout: `UI/Layouts/Menu/<MenuName>/<MenuName>_<Name>.layout` (+ `.meta`)

---

## Fixed Grid vs Dynamic List

Two different problems, two different answers:

**Fixed grid** — a paged browser with a known page size. Author all N cards in
the parent layout with indexed names, then show/hide per page. No allocation per
refresh, and the grid geometry is authored rather than computed.

```cpp
protected const int CARDS_PER_PAGE = 15;   // must equal the count in the layout

Widget w = grid.FindWidget("ShopMenu_Card" + wi);
```

**Dynamic list** — a variable number of rows with no natural page size (category
tabs, warehouse rows). Instantiate from a `ResourceName` attribute:

```cpp
[Attribute("{6A7C4E1C77B31E40}UI/Layouts/Menu/ShopMenu/ShopMenu_Tab.layout", uiwidget: UIWidgets.ResourceNamePicker, desc: "Layout for one category tab", params: "layout")]
ResourceName m_TabLayout;
```

```cpp
protected bool RefreshTabs()
{
    if(!m_wTabs) return false;

    // Clear first — rebuilding without this stacks duplicates every refresh
    while(m_wTabs.GetChildren())
        m_wTabs.RemoveChild(m_wTabs.GetChildren());

    array<OVT_ShopCategory> tabs = new array<OVT_ShopCategory>();
    BuildTabOrder(tabs);

    if(tabs.Count() < 2)
    {
        m_wTabs.SetVisible(false);
        return false;
    }

    m_wTabs.SetVisible(true);

    WorkspaceWidget workspace = GetGame().GetWorkspace();
    if(!workspace || m_TabLayout.IsEmpty()) return false;

    foreach(OVT_ShopCategory category : tabs)
    {
        CreateTab(workspace, category);
    }

    return true;
}

protected void CreateTab(WorkspaceWidget workspace, OVT_ShopCategory category)
{
    if(!workspace || !m_wTabs) return;

    Widget w = workspace.CreateWidgets(m_TabLayout, m_wTabs);   // parent = container
    if(!w) return;

    OVT_ShopMenuTabComponent tab = OVT_ShopMenuTabComponent.Cast(w.FindHandler(OVT_ShopMenuTabComponent));
    if(!tab) return;

    tab.Init(category, this, category == m_eTab);
}
```

The second argument to `CreateWidgets` is the parent. Omit it (as
`ShowLayout` does) and the widget lands on the workspace root instead.

---

## The Component

```cpp
class OVT_ShopMenuTabComponent : SCR_ScriptedWidgetComponent
{
    protected const int COLOR_SELECTED = 0xFFC26414;

    protected OVT_ShopCategory m_eCategory;
    protected OVT_ShopContext m_Context;
    protected bool m_bSelected;
    protected bool m_bWiredToButton;

    //! Fills in the tab, wires its click and remembers who to notify.
    void Init(OVT_ShopCategory category, OVT_ShopContext context, bool selected = false)
    {
        m_eCategory = category;
        m_Context = context;

        if(m_wRoot)
        {
            TextWidget label = TextWidget.Cast(m_wRoot.FindAnyWidget("TabLabel"));
            if(label) label.SetText(OVT_ShopCategoryHelper.GetLabelKey(category));
        }

        WireButton();
        SetSelected(selected);
    }
    ...
}
```

- `m_wRoot` comes from `SCR_ScriptedWidgetComponent` — the widget the handler is
  attached to.
- **`Init()` does the wiring, not `HandlerAttached()`.** `HandlerAttached` runs
  before the context has passed anything in, and its ordering relative to sibling
  handlers (the `SCR_ButtonComponent` you want to subscribe to) is not guaranteed.
- Hold a **back-reference to the context** and call a method on it. Rows do not
  mutate menu state themselves.

### Attaching in the sub-layout

```
ButtonWidgetClass {
 Name "ShopMenu_Tab"
 components {
  SCR_ButtonComponent "{56FAC701265C74AA}" {
   m_bMouseOverToFocus 1
   m_bShowBackgroundOnFocus 1
   m_bShowBorderOnHover 1
  }
  OVT_ShopMenuTabComponent "{6A7C4E1C77B31E45}" {
  }
 }
 style blank
 { ... }
}
```

`m_bMouseOverToFocus` / `m_bShowBackgroundOnFocus` matter for controllers:
focus is how a gamepad indicates which row is selected, so a row with no focus
visual is unusable on a pad.

---

## Clicks: Subscribe to the Button, Don't Override OnClick

Both input paths — mouse via `SCR_ButtonBaseComponent.OnClick`, gamepad via
`MenuSelect` on the focused widget — funnel into the button's `m_OnClicked`
invoker. Subscribing once is the only wiring that fires exactly once on either
device.

```cpp
protected void WireButton()
{
    if(m_bWiredToButton) return;
    if(!m_wRoot) return;

    SCR_ButtonComponent button = SCR_ButtonComponent.Cast(m_wRoot.FindHandler(SCR_ButtonComponent));
    if(!button) return;

    button.m_OnClicked.Insert(OnTabClicked);
    m_bWiredToButton = true;
}

protected void OnTabClicked(SCR_ButtonBaseComponent button)
{
    Activate();
}

//! Fallback only: with a button component present the invoker already handled
//! this click, and handling it twice would rebuild the grid twice per click.
override bool OnClick(Widget w, int x, int y, int button)
{
    super.OnClick(w, x, y, button);
    if (button != 0)
        return false;

    if(m_bWiredToButton)
        return false;

    if(!m_Context)
        return false;

    Activate();
    return true;
}
```

The `m_bWiredToButton` guard is the important part — **override `OnClick`
without it and every mouse click runs twice.** Keeping `OnClick` as a guarded
fallback means a layout that lacks a button component still works instead of
becoming a dead wire.

For a top-level action button use `SCR_InputButtonComponent.m_OnActivated`
instead; see `navigation-buttons.md`.

---

## Drawing Selection

`SCR_ButtonBaseComponent`'s toggle state flips itself on every click and then has
to be corrected from script. Don't use it. Draw the selected look explicitly
after every rebuild:

```cpp
void SetSelected(bool selected)
{
    m_bSelected = selected;
    if(!m_wRoot) return;

    if(selected)
    {
        m_wRoot.SetOpacity(1.0);
    }else{
        m_wRoot.SetOpacity(0.6);
    }

    TextWidget label = TextWidget.Cast(m_wRoot.FindAnyWidget("TabLabel"));
    if(!label) return;

    if(selected)
    {
        label.SetColor(Color.FromInt(COLOR_SELECTED));
    }else{
        label.SetColor(Color.White);
    }
}
```

(No ternaries — EnforceScript has none.)

---

## Blanking Unused Fixed Cards

Hide **and** disable. Opacity 0 alone leaves the widget clickable, so a click on
apparently empty space re-selects whatever the card showed on the previous page:

```cpp
for(; wi < CARDS_PER_PAGE; wi++)
{
    Widget w = grid.FindWidget("ShopMenu_Card" + wi);
    if(!w) continue;

    w.SetOpacity(0);
    w.SetEnabled(false);
}
```

---

## Gamepad Considerations

- Give every interactive row a focus visual (`m_bShowBackgroundOnFocus`).
- Long lists need a **stepper** — a pair of `WLib_NavigationButton`s on
  `shoulder_left`/`shoulder_right` that cycle with wraparound, so a pad never has
  to point at an individual tab:

```cpp
void CycleTab(int delta)
{
    array<OVT_ShopCategory> tabs = new array<OVT_ShopCategory>();
    BuildTabOrder(tabs);

    if(tabs.Count() < 2) return;

    int index = tabs.Find(m_eTab);
    if(index < 0) index = 0;

    index = index + delta;
    if(index < 0) index = tabs.Count() - 1;
    if(index >= tabs.Count()) index = 0;

    SelectTab(tabs[index]);
}
```

- Hide the steppers when there is nothing to cycle. Because a hidden
  `SCR_InputButtonComponent` refuses its keybind, hiding the button is also what
  disables the shortcut:

```cpp
if(m_wPrevCategoryButton) m_wPrevCategoryButton.SetVisible(showTabs);
if(m_wNextCategoryButton) m_wNextCategoryButton.SetVisible(showTabs);
```

- Select something by default. `OVT_ShopContext` selects the first card of the
  page when nothing is selected, so the details pane is never blank on arrival.
