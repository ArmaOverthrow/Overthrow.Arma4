# OVT_UIContext — Lifecycle and Wiring

Every Overthrow menu is a subclass of `OVT_UIContext` (`Scripts/Game/UI/OVT_UIContext.c`),
registered in the `m_aContexts` array of `OVT_UIManagerComponent` on the player
prefab. Contexts are **client-side and per-player** — they live on the player
character, they never run on a dedicated server, and everything they do to game
state must go through a server call.

New files go in `Scripts/Game/UI/Context/OVT_<Name>Context.c`.

---

## Base Class Attributes

Set these on the prefab, not in code:

| Attribute | Meaning |
|---|---|
| `m_Layout` | `ResourceName` of the `.layout` to instantiate |
| `m_sContextName` | `ActionContext` name from `chimeraInputCommon.conf`, activated each frame while open |
| `m_sOpenAction` | Action that opens the menu. Empty for menus opened from code / a `UserAction` |
| `m_sCloseAction` | Almost always `"MenuBack"` |
| `m_bOpenActionCloses` | `1` = open action toggles. `0` when a separate close action exists |
| `m_bHideHUDOnShow` | Default `1`; hides the HUD while open |

Add your own with `[Attribute(...)]` as usual — e.g. `OVT_ShopContext` declares
`m_TabLayout` for its runtime-instantiated tabs.

---

## Registration

`Prefabs/Characters/Factions/INDFOR/FIA/Character_Player.et`, inside
`OVT_UIManagerComponent.m_aContexts`:

```
    OVT_ShopContext "{598EF9F13B9F3106}" {
     m_Layout "{846B87563CCE7DA1}UI/Layouts/Menu/ShopMenu.layout"
     m_sContextName "OverthrowShopContext"
     m_sCloseAction "MenuBack"
     m_bOpenActionCloses 0
     m_TabLayout "{6A7C4E1C77B31E40}UI/Layouts/Menu/ShopMenu/ShopMenu_Tab.layout"
    }
```

**A context not in this array never initialises** — no `Init`, no input
registration, and `OVT_Global.GetUI().GetContext(OVT_YourContext)` returns null.
This is the single most common "my menu does nothing" cause. The instance GUID
must be unique within the prefab.

---

## Lifecycle

```
OnPostInit          Init(owner, uimanager) → PostInit()      one-shot setup
controlled          OnControlledByPlayer()                   m_sPlayerID / m_iPlayerID resolve here
                    RegisterInputs()                         binds m_sOpenAction / m_sCloseAction
every frame         EOnFrame → ActivateContext(m_sContextName) while open, then OnActiveFrame()
open                ShowLayout() → CanShowLayout() → CreateWidgets → Enable() → OnShow()
close               CloseLayout() → RemoveFromHierarchy → Disable() → OnClose()
death / unpossess   CloseLayout() + UnregisterInputs()
```

Override points, and what belongs in each:

- **`PostInit()`** — subscriptions to long-lived managers that must survive
  between openings. Guard with `if(SCR_Global.IsEditMode()) return;`. Note the
  player ID is *not* resolved yet.
- **`CanShowLayout()`** — return `false` to refuse to open (wrong state, missing
  data). Cheaper and clearer than opening an empty menu.
- **`OnShow()`** — reset browsing state, find and wire widgets, subscribe to
  per-session invokers, then do the first refresh.
- **`OnClose()`** — remove **everything** `OnShow` inserted.
- **`OnActiveFrame(timeSlice)` / `OnFrame(timeSlice)`** — per-frame work. Avoid;
  prefer invokers and `CallLater`.

### PostInit vs OnShow subscriptions

`PostInit` subscriptions persist for the character's lifetime, so the handler
must tolerate being called while the menu is closed:

```cpp
override void PostInit()
{
    if(SCR_Global.IsEditMode()) return;
    m_Economy.m_OnPlayerMoneyChanged.Insert(OnPlayerMoneyChanged);
}

protected void OnPlayerMoneyChanged(string playerId, int amount)
{
    if(playerId != m_sPlayerID) return;
    if(!m_bIsActive || !m_wRoot) return;   // both guards required

    TextWidget money = TextWidget.Cast(m_wRoot.FindAnyWidget("PlayerMoney"));
    if(money) money.SetText("$" + amount);
}
```

---

## Teardown Is Not Optional

The layout is rebuilt from scratch on every open, so widget handlers are
recreated anyway. **Invokers on managers and controllers are not** — they outlive
the menu and accumulate one subscription per opening, so the tenth shop visit
runs `OnSellResult` ten times.

`OnClose` must:

```cpp
override void OnClose()
{
    // 1. cancel queued callbacks
    GetGame().GetCallqueue().Remove(HideMessage);
    GetGame().GetCallqueue().Remove(Refresh);

    // 2. remove button handlers
    if(m_BuyAction) m_BuyAction.m_OnActivated.Remove(Buy);
    if(m_CloseAction) m_CloseAction.m_OnActivated.Remove(CloseLayout);

    // 3. remove long-lived invoker subscriptions — the ones that actually leak
    if(m_Transactions && m_Transactions.m_OnSellResult)
        m_Transactions.m_OnSellResult.Remove(OnSellResult);

    // 4. null every cached widget and component reference
    m_Transactions = null;
    m_BuyAction = null;
    m_CloseAction = null;
    m_wBuyButton = null;
}
```

Cache the component you subscribed to (`m_Transactions`) so you unsubscribe from
**the same instance** — re-resolving through `OVT_Global` at close time can
return a different one and leak the original.

Pending `CallLater` callbacks must be cancelled too, or a timer fires into a
destroyed layout.

---

## Opening a Menu from Code

Resolve the manager from whichever entity you already have — in that order of
preference:

```cpp
// From a ScriptedUserAction: use the entity that performed the action.
OVT_UIManagerComponent uimanager = OVT_UIManagerComponent.Cast(pUserEntity.FindComponent(OVT_UIManagerComponent));
if(!uimanager) return;

// From inside another OVT_UIContext: the base class already holds it.
m_UIManager.ShowContext(OVT_ResistanceMenuContext);

// From anywhere else, for the local player only.
OVT_UIManagerComponent ui = OVT_Global.GetUI();
```

`OVT_Global.GetUI()` resolves `SCR_PlayerController.GetLocalControlledEntity()`
without a null check, so it is the fallback, not the default — from a user
action, `pUserEntity` is both cheaper and correct for the right player.

To open with state:

```cpp
OVT_ShopContext context = OVT_ShopContext.Cast(uimanager.GetContext(OVT_ShopContext));
if(context)
{
    context.SetShop(shop);       // pass state BEFORE showing
    context.ShowLayout();
}
```

Pass parameters through a setter before `ShowLayout()`, never as arguments —
`ShowLayout` is also the input-listener target and takes none. `OnShow()` then
reads that state.

`uimanager.ShowContext(OVT_SomeContext)` is the shorthand when there is nothing
to pass.

---

## Refresh Pattern

One `Refresh()` that rebuilds everything from current state, called after every
mutation. Guard the preconditions and bail early:

```cpp
void Refresh()
{
    if(!m_Shop) return;
    if(!m_wRoot) return;
    if(!m_bIsActive) return;
    if(!m_Economy) return;
    ...
}
```

For state that arrives from the server after a click, queue one late redraw
rather than polling:

```cpp
protected void ScheduleRefresh()
{
    GetGame().GetCallqueue().Remove(Refresh);
    GetGame().GetCallqueue().CallLater(Refresh, TRANSACTION_RECHECK_MS, false);
}
```

`Remove` before `CallLater` keeps it to one pending callback.

---

## Server Calls

A context runs on the client. It may read replicated state freely, but any
change goes through the server, and the UI updates when the result comes back —
never optimistically:

```cpp
OVT_ShopTransactionComponent transactions = OVT_ControllerComponent<OVT_ShopTransactionComponent>.Get();
if(!transactions) return;
transactions.SellItems(m_Shop, m_SelectedResource, quantity);
```

New client→server operations belong on a component of `OVT_OverthrowController`,
reached with `OVT_ControllerComponent<T>.Get()` — and **not** through a new
`OVT_Global` getter, which is never added for a controller component. (The legacy
comms monolith a context used to call is deleted, so there is no second option.) See
`overthrow-controller.md` in the `overthrow-architecture` skill.

Client-side prediction is worth avoiding: derive what the menu shows from the
same rule set the server will apply. `OVT_ShopContext` asks
`OVT_ShopTransactionComponent.ShopBuysResource` / `GetSellUnitPrice` for both the
grey-out and the price, so "what the menu offered" and "what the server does"
cannot diverge.

---

## Feedback

```cpp
ShowHint("#OVT-CannotAfford");        // SCR_HintManagerComponent, local
ShowNotification("TAG");              // OVT notification system, cheaper over network
```

For a message inside the menu, use a named `TextWidget` plus a `CallLater`
timeout — see `ShowMessage` / `HideMessage` in `OVT_ShopContext`.
