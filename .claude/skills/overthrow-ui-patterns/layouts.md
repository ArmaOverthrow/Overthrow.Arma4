# Layout Files

`.layout` files are the Enfusion widget tree in a plain-text, brace-delimited
format. They are hand-editable, and for Overthrow's menus that is usually faster
than the Workbench Layout Editor — but the GUID rules below are unforgiving.

Location: `UI/Layouts/Menu/`, `UI/Layouts/HUD/`, `UI/Layouts/Dialogs/`,
`UI/Layouts/Map/`. Sub-layouts for one menu go in a folder named after it
(`UI/Layouts/Menu/ShopMenu/ShopMenu_Tab.layout`).

---

## GUID Rules

Three different kinds of GUID live in a layout and they behave differently:

| Kind | Example | Rule |
|---|---|---|
| **Widget instance** | `ButtonWidgetClass "{6A7C4E1B39D0A5C2}"` | **Unique within the file.** Verified: `ShopMenu.layout` has 57 widgets, 57 distinct GUIDs. |
| **Slot** | `Slot LayoutSlot "{56EEDE01982D053A}"` | **Repeats freely.** Identifies the slot *type* binding. `ShopMenu.layout` reuses 28 slot GUIDs across 57 slots. Copy whichever one the sibling widgets use. |
| **Inherited component** | `SCR_InputButtonComponent "{5D346C3DD81D95CD}"` | **Must equal the GUID in the base layout you inherited from.** This is how the override binds. A fresh GUID adds a second, unconfigured component and the widget goes dead. |

To mint widget GUIDs: take an unused 16-hex-digit base and increment the last
digits (`6A7C4E1B39D0A5C1`, `...C2`, `...C3`). Check before you commit:

```bash
python3 - <<'EOF'
import re, collections, sys
p = "UI/Layouts/Menu/YourMenu.layout"
g = re.findall(r'^\s*\w+WidgetClass "\{([0-9A-F]+)\}"', open(p).read(), re.M)
d = [k for k, v in collections.Counter(g).items() if v > 1]
print("duplicate widget GUIDs:", d or "none")
EOF
```

---

## Inheriting a Base Layout

```
TextWidgetClass "{598EF9FB202E31DD}" : "{9D643D214767D616}UI/layouts/WidgetLibrary/TextWidgets/Text_Heading2.layout" {
 Name "Title"
 Slot LayoutSlot "{56EEDE046853BD3D}" {
  Padding 0 0 0 12
  SizeMode Fill
 }
 Text "#OVT-Shop"
}
```

The `: "{GUID}path"` suffix inherits everything from that layout; the body
overrides. Common bases:

| Base | GUID + path |
|---|---|
| Navigation button | `{08CF3B69CB1ACBC4}UI/layouts/WidgetLibrary/Buttons/WLib_NavigationButton.layout` |
| Heading text | `{9D643D214767D616}UI/layouts/WidgetLibrary/TextWidgets/Text_Heading2.layout` |
| Body text | `{3132F64DB8B4CB44}UI/layouts/WidgetLibrary/TextWidgets/Text_Body.layout` |

Browse `/mnt/n/Projects/Arma 4/ArmaReforger/UI/layouts/WidgetLibrary/` for more,
and read the base layout before overriding it — that is where you find the
component GUID you must reuse.

---

## The .meta File

**Every new `.layout` needs a sibling `.layout.meta`, or the engine will not
resolve it.** Copy this and change the GUID and path:

```
MetaFileClass {
 Name "{6A7C4E1C77B31E40}UI/Layouts/Menu/ShopMenu/ShopMenu_Tab.layout"
 Configurations {
  LayoutResourceClass PC {
  }
  LayoutResourceClass XBOX_ONE : PC {
  }
  LayoutResourceClass XBOX_SERIES : PC {
  }
  LayoutResourceClass PS4 : PC {
  }
  LayoutResourceClass HEADLESS : PC {
  }
 }
}
```

The GUID in `Name` is the layout's resource GUID — the one you reference from
`m_Layout` on the prefab or from a `ResourceName` attribute. The console
configurations inherit from `PC`; keep all five so the mod builds for console.

---

## Structural Conventions

Overthrow menus share a skeleton. Copy `ShopMenu.layout` rather than starting
blank:

```
FrameWidgetClass                 rootFrame
 └ HorizontalLayoutWidgetClass   "Window"      Anchor 0 0 1 1, SizeX/Y negative insets
    └ OverlayWidgetClass         "Content"
       ├ ImageWidgetClass        "Background"  black, Opacity 0.5
       ├ BlurWidgetClass         "Blur"        Intensity 0.7
       └ HorizontalLayoutWidgetClass "SpaceLayout"
          └ SizeLayoutWidgetClass "Alignment"  WidthOverride 1344, HeightOverride 936
             └ VerticalLayoutWidgetClass "ContentLayout"
                ├ HorizontalLayout  Title + PlayerMoney
                ├ HorizontalLayout  "HeaderRow"    mode toggle, tabs, steppers
                ├ ImageWidgetClass  "UpperStripe"  accent divider
                ├ OverlayWidgetClass "ContentSegment"
                │  └ UniformGridLayoutWidgetClass "BrowserGrid"
                └ HorizontalLayout  action buttons + CloseButton
```

- **Accent colour** `0.761 0.392 0.08 1` (`0xFFC26414` from script) for dividers,
  borders and selection.
- **Name every widget you touch from script.** `FindAnyWidget("PlayerMoney")`
  needs `Name "PlayerMoney"`. Names must be unique enough to find; grid cards use
  an indexed convention (`ShopMenu_Card0` … `ShopMenu_Card14`) so script can walk
  them in a loop.
- **Fixed grids over dynamic ones** for paged content — author N cards in the
  layout, then show/hide per page. Dynamic instantiation is for variable-length
  rows (see `widget-components.md`).
- **`style blank` / `style outline_2px`** are the engine's named widget styles;
  reuse rather than restyling from scratch.

---

## Localization

Labels are `#OVT-` keys, resolved from `Language/localization_Overthrow.st`:

```
  CustomStringTableItem "{6A7C4E1E00000005}" {
   Id "OVT-Shop_ModeBuy"
   Target_en_us "Buy Mode"
   Comment "Shop menu header - switch the browser to the shop's stock"
   DevNote ""
   Repro ""
   ActorGender NONE
   Terminology NONE
   Status DEVELOPMENT_PENDING
   NonTranslatable 0
   MaxLength 0
   Modified 1706579111
   Author "Aaron Static"
   LastChanged "parkj_c3c9hji"
   ImageLink ""
   Hidden 0
  }
```

- Naming: `OVT-<Screen>_<Thing>` (`OVT-Shop_SellAll`, `OVT-ShopCategory_All`).
- `Target_en_us` only for new strings; other `Target_*` locales are added by
  translators.
- **Fill in `Comment`** — it is the only context a translator gets.
- Unique GUID per item, same minting approach as layouts.
- From script, `WidgetManager.Translate(key)` resolves a key when you need the
  *text* (e.g. to sort a list alphabetically by display name);
  `TextWidget.SetText("#OVT-Key")` resolves at draw time.
- `TextWidget.SetTextFormat(key, p1, p2)` for keys with `%1` / `%2` placeholders.

---

## What You Cannot Verify Locally

`tools/compile-check.sh` compiles EnforceScript. It does **not** parse layouts,
`.meta` files, `.conf` files or the string table. A malformed layout shows up as
a missing menu at runtime, and a missing `.meta` as an unresolvable resource.

So: get the GUID rules right by construction, run the duplicate check above, and
hand the user a specific list of what to look at in the Workbench.
