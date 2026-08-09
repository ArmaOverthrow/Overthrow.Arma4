# Navigation Buttons — WLib_NavigationButton + SCR_InputButtonComponent

`WLib_NavigationButton.layout` is the default button for **every** menu action in
Overthrow. It renders the bound key or pad glyph next to the label, so the
control is discoverable without a manual and legible on a controller. Prefer it
over `WLib_ButtonText` unless you have a specific reason not to.

Base game path and GUID:

```
"{08CF3B69CB1ACBC4}UI/layouts/WidgetLibrary/Buttons/WLib_NavigationButton.layout"
```

Variants, same component and same wiring: `WLib_NavigationButtonSmall`,
`WLib_NavigationButtonSuperSmall`, `WLib_NavigationButtonPaging`.

---

## In the Layout

```
ButtonWidgetClass "{6A7C4E1B39D0A5C2}" : "{08CF3B69CB1ACBC4}UI/layouts/WidgetLibrary/Buttons/WLib_NavigationButton.layout" {
 Name "ModeBuyButton"
 Slot LayoutSlot "{56EEDE01982D053A}" {
  Padding 0 0 10 0
 }
 components {
  SCR_InputButtonComponent "{5D346C3DD81D95CD}" {
   m_sActionName "OverthrowShopModeBuy"
   m_sLabel "#OVT-Shop_ModeBuy"
  }
 }
}
```

Four things must be right:

1. **The widget GUID is fresh and unique in this file** (`{6A7C4E1B39D0A5C2}`).
2. **The component GUID is `{5D346C3DD81D95CD}` — copied, not generated.** You
   are *overriding* the `SCR_InputButtonComponent` that already exists in
   `WLib_NavigationButton.layout`, and the GUID is how the engine matches it. A
   fresh GUID here adds a second component instead of configuring the real one,
   and the button silently does nothing. This is the most common layout bug.
3. **`m_sActionName`** matches an `Action` in `chimeraInputCommon.conf`, and that
   action is listed in the screen's `ActionContext`. Without the context entry
   the glyph still draws but the key never fires.
4. **`m_sLabel`** is an `#OVT-` localization key, never a literal.

---

## Wiring It Up

```cpp
protected Widget m_wBuyButton;
protected SCR_InputButtonComponent m_BuyAction;

// OnShow
m_wBuyButton = m_wRoot.FindAnyWidget("BuyButton");
if(m_wBuyButton)
{
    m_BuyAction = SCR_InputButtonComponent.Cast(m_wBuyButton.FindHandler(SCR_InputButtonComponent));
    if(m_BuyAction) m_BuyAction.m_OnActivated.Insert(Buy);
}

// OnClose
if(m_BuyAction) m_BuyAction.m_OnActivated.Remove(Buy);
m_BuyAction = null;
m_wBuyButton = null;
```

Null-guard every lookup. A stale layout then degrades to a missing button rather
than a script error mid-menu.

### `m_OnActivated`, not `m_OnClicked`

`m_OnActivated` is invoked from `SCR_InputButtonComponent.OnInput()`, which is
reached from **both** input paths:

- mouse → `OnClick` → `OnInput`
- keybind → `InputManager` listener → `OnButtonPressed` → `OnInput`

So one subscription covers mouse, keyboard and pad. `m_OnClicked` is inherited
from `SCR_ButtonBaseComponent` and older Overthrow contexts still use it
(`OVT_BaseMenuContext`); it works for mouse and for `MenuSelect` on a focused
widget, but it is not the action path. **Use `m_OnActivated` for new code.**

### Handler signature

The invoker is called as `m_OnActivated.Invoke(this, m_sActionName)` — i.e.
`(SCR_InputButtonComponent, string)`. Overthrow's existing handlers are declared
as:

```cpp
void Buy(Widget src, float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
```

`ScriptInvoker` passes arguments positionally without type checking, so this
works, and it is the established shape across the codebase — match it. Just do
not rely on `src` actually being a `Widget`; it is the component. Handlers that
take no arguments at all (`PreviousPage()`, `CloseLayout()`) are also fine.

---

## Enable, Disable, Hide

```cpp
m_SellAction.SetEnabled(canSell);        // greyed out, keybind dead
m_wSellButton.SetVisible(sellMode);      // gone, keybind dead
m_SellAction.SetLabel("#OVT-Shop_Sell"); // relabel at runtime
m_SellAction.SetLabelColor(Color.FromInt(0xFFC26414));  // accent = selected
```

**Hiding or disabling a button also kills its shortcut.**
`SCR_InputButtonComponent.OnInput()` opens with:

```cpp
if (m_bCanBeDisabled && (!m_wRoot.IsVisibleInHierarchy() || !m_wRoot.IsEnabledInHierarchy()))
    return;
```

`m_bCanBeDisabled` defaults to `1`. Three consequences:

- **This is the supported way to retire a shortcut contextually.** Sell-mode
  buttons hidden in Buy mode cannot be triggered by their keys either.
- **It is what makes shared pad inputs safe.** `OverthrowShopBuy` and
  `OverthrowShopSellAll` both sit on `gamepad0:x` and never collide, because
  `RefreshActionButtons()` guarantees only one is visible. If you rely on this,
  say so in a comment at the call site — it is invisible from the conf.
- **A button inside a hidden parent is also dead.** It is
  `IsVisibleInHierarchy()`, so hiding a header row disables every shortcut in it.

For a toggle (Buy mode / Sell mode), do **not** disable the inactive one — a
greyed button reads as "you cannot do this". Overthrow's convention is opacity
plus label colour, keeping both live:

```cpp
protected void ApplyToggleVisual(Widget button, SCR_InputButtonComponent action, bool selected)
{
    if(button)
    {
        if(selected)
        {
            button.SetOpacity(1.0);
        }else{
            button.SetOpacity(0.6);
        }
    }

    if(!action) return;

    if(selected)
    {
        action.SetLabelColor(Color.FromInt(0xFFC26414));
    }else{
        action.SetLabelColor(Color.White);
    }
}
```

`0xFFC26414` is the Overthrow accent orange (`0.761 0.392 0.08` in layout float
colour) used for selection across every menu.

---

## Why Workspace-Created Layouts Work

`SCR_InputButtonComponent.OnInput()` also checks `IsParentMenuFocused()`. That
returns `true` when the button has no parent `MenuBase` — which is the case for
Overthrow, because `OVT_UIContext.ShowLayout()` builds the layout with
`workspace.CreateWidgets(m_Layout)` rather than through the menu manager. So the
check passes and everything works.

Do not "fix" this by moving a menu onto `MenuBase`; the whole `OVT_UIContext`
lifecycle assumes workspace ownership.

There is one live gate you do inherit: if a **modal** widget is up, the button
only fires when it is inside that modal's hierarchy.
