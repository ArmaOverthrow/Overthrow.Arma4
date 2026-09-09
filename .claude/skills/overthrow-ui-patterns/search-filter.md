# Search / Live-Filter Boxes

A text field that narrows a dynamic list as the player types — first shipped
on the Jobs menu (`OVT_JobsContext.c`, `UI/Layouts/Menu/JobsMenu.layout`),
filtering by job title and by town name. Copy that pair of files for the next
screen that needs the same thing (a recruit roster, a shop's item list, a
warehouse inventory).

---

## The Widget

Inherit `WLib_EditBox.layout` and reuse its component GUID verbatim — see
`layouts.md`'s "Inherited component GUIDs" rule. Every `SCR_EditBoxComponent`
instance in this codebase carries the same GUID because it must match the
base layout's own:

```
ButtonWidgetClass "{<fresh-instance-guid>}" : "{0022F0B45ADBC5AC}UI/layouts/WidgetLibrary/EditBox/WLib_EditBox.layout" {
 Name "SearchBox"
 Slot LayoutSlot "{<any-slot-guid>}" {
  SizeMode Fill
 }
 components {
  SCR_EditBoxComponent "{547290FFBD5B33E9}" {
   m_sLabel "#OVT-<Screen>_SearchLabel"
   m_bShowWriteIcon 0
  }
 }
}
```

No new keybinding or `ActionContext` entry is needed. A gamepad activates the
box like any other widget and gets the on-screen keyboard automatically —
this is the same reason `WLib_SpinBox` needs no binding either (see
`ui-contexts.md` if the screen also has a spinner).

---

## Wiring

```cpp
//! Wires the search box's live-change event. SCR_EditBoxComponent.m_OnChanged fires on every
//! keystroke, on-screen keyboard included, so a controller player typing with the pad keyboard
//! gets the same live filtering as a mouse-and-keyboard player.
protected void BuildSearchBox()
{
    SCR_EditBoxComponent editBox = SCR_EditBoxComponent.GetEditBoxComponent("SearchBox", m_wRoot);
    if(!editBox)
        return;

    editBox.m_OnChanged.Insert(OnSearchChanged);
}

protected void OnSearchChanged(SCR_EditBoxComponent comp, string value)
{
    value.ToLower();
    m_sSearchFilter = value;
    m_SelectedJob = null;   // force reselection of the first surviving row
    Refresh();
}
```

`GetEditBoxComponent(name, root)` is the same static accessor every rename/
save-name dialog already uses to *read* a box on confirm (see
`OVT_RenameStorageAction.c`). What's new here is subscribing to `m_OnChanged`
instead of reading the value once — every prior usage in this codebase was
read-on-confirm only, so this was the first live-filter wiring and is now the
one to copy.

Call `BuildSearchBox()` from `OnShow()`, after the button wiring and before
the first `Refresh()`. No `OnClose` teardown is needed: `SCR_EditBoxComponent`
is widget-local and dies with the layout, same as the `SCR_InputButtonComponent`
subscriptions already on the screen.

### `ToLower()` returns `void` — it mutates in place

This is the one real trap. `string.ToLower()` does **not** return a
lowercased copy; it lowercases the string it's called on and returns nothing.

```cpp
// WRONG — compiles to "Incompatible parameter" errors, does not lowercase anything:
m_sSearchFilter = value.ToLower();

// RIGHT:
value.ToLower();
m_sSearchFilter = value;
```

Confirmed by every other `ToLower()` call already in the codebase
(`OVT_TownManagerComponent.c`, `OVT_Global.c`, `OVT_PrefabUtils.c`) — all
mutate a local on its own line, never assign the call's result.

---

## Filtering a Rebuild-Style List

The list-building `Refresh()` most menus already have (see
`widget-components.md`'s dynamic-list pattern) gets one more `continue` guard
per candidate field, evaluated only when the filter is non-empty:

```cpp
if(m_sSearchFilter != "")
{
    string title = WidgetManager.Translate(config.m_sTitle);   // resolve the loc key first
    title.ToLower();

    string townName = "";
    int effectiveTownId = GetEffectiveTownId(job);
    if(effectiveTownId != -1)
        townName = OVT_Global.GetTowns().GetTownName(effectiveTownId);
    townName.ToLower();

    if(title.IndexOf(m_sSearchFilter) == -1 && townName.IndexOf(m_sSearchFilter) == -1) continue;
}
```

Points that generalize to any screen:

- **Resolve loc keys before matching.** A config field like `m_sTitle` is
  usually a localization key (`"#OVT-Job_RaiseSupport"`), not display text.
  Matching the raw key against typed text silently never matches anything;
  resolve it with `WidgetManager.Translate(key)` first.
- **OR together every field the player would reasonably expect to search by**
  (title, location name, item name, owner...). One field per `IndexOf` check,
  joined with `||`-via-`&&`-on-the-negation as above.
- **`IndexOf` substring match is enough** — this codebase has no fuzzy match
  and doesn't need one for menu-length lists.
- **Reset the selection when a filter changes** (`m_SelectedJob = null` before
  `Refresh()`), mirroring the existing "clear then refresh" idiom menus already
  use after a destructive action (e.g. `Decline()` in `OVT_JobsContext.c`).
  Otherwise the detail pane keeps showing a row the filter just hid.

---

## Empty Results Need a Second Message

A list can be empty two different ways, and they read differently to a
player: no items exist at all, versus items exist but nothing matches the
filter. Keep them as two branches with two messages — collapsing them into
one "nothing here" string reads as a bug ("didn't I just see jobs in this
town a second ago?").

```cpp
int shownCount = 0;
foreach(... : ...)
{
    if(/* filtered out */) continue;
    ...
    shownCount++;
}

if(shownCount == 0)
    ShowNoMatchesPanel();   // "#OVT-<Screen>_NoMatches" — distinct from the true-empty-list message
```

Leave the pre-existing true-empty-list branch (`m_aJobs.Count() == 0` in the
Jobs menu) untouched — it already has its own message and early return.

---

## Related Gotcha This Pattern Surfaced

Filtering forced every job through a "what town is this in" resolution up
front (for matching), not just for the one selected job as before. That
exposed a latent bug: a job can be tied to neither a town nor a base
(`townId == -1 && baseId == -1` — `OVT_JobManagerComponent` already treats
this as a valid "global" job). Any location-resolution helper must check for
that and return a sentinel (`-1`) rather than indexing a base array with `-1`
or calling `GetTownName(-1)`. If you're adding search to a screen with a
similar "sometimes there's no location" field, check for the equivalent gap
before you copy the filter loop.
