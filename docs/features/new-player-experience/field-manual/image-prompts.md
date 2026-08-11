# Field Manual — Tile Art Prompts

**Created:** 2026-08-08
**Feature:** `new-player-experience/field-manual` (feature #2 of 5)
**Purpose:** Midjourney prompts for the twelve per-entry tiles, plus two optional body banners.

> **Nothing here blocks the feature.** `Tiles/default_ui.edds` (the Overthrow logo tile) already ships on all twelve entries. Every image below is an upgrade path (decision **D7**). Produce three or twelve; the rest keep the default and still look deliberate.

---

## 1. The target style, observed from the shipped base game

Taken from a screenshot of the base game's Editor field-manual tile grid on 2026-08-08, not from memory:

- **Black ink line art on off-white, subtly textured paper.** Hand-inked technical-illustration feel, not clean vector, not painterly.
- **Strictly monochrome.** No colour anywhere. Shading is **cross-hatching and stipple**; solid black fills appear only in small accents (a medical cross, a filled progress bar).
- **Subject isolated, generous margins.** No scenery, no horizon, no ground plane. Objects sit on the bare paper; some carry a soft grey ellipse shadow beneath.
- **Secondary elements are flat mid-grey silhouettes** — the eye behind "Intro", the arrow above "Placing", UI boxes and frames.
- **Medium-thin, fairly even line weight**, with a slightly rough inked edge.
- **Minimal internal detail on figures.** Soldiers read as contour plus a little hatching; faces are barely indicated.
- **1980s Cold War subject matter.** Period kit only. No modern optics, plate carriers, rails or drones.

---

## 2. The shared style block

**Paste this identically into every prompt.** Consistency across the set matters more than any single tile being perfect, and identical wording is most of how you get it.

```
black ink illustration, hand-inked technical field manual drawing, fine pen line art with cross-hatch shading, monochrome black ink on off-white textured paper, subject isolated with generous margins, no background scenery, no horizon, 1980s Cold War military instruction manual aesthetic
```

**Suggested parameters:**

```
--ar 4:3 --style raw --no color, colour, gradient, photo, background scenery
```

- `--ar 4:3` matches 400 x 300 exactly, so there is no crop guesswork.
- `--style raw` suppresses Midjourney's default beautification, which fights the flat manual look.
- The `--no` list is doing real work: the default pull toward a scenic background and warm tone is the main way these drift off-style.

**To lock the set together:** generate one tile you are happy with, then pass it as a style reference (`--sref <image url>`) on the other eleven. That holds line weight and paper tone far better than wording alone.

⚠️ Midjourney's parameter syntax moves between versions. Treat the flags as a starting point and adjust to whatever your current version accepts. The **wording** is the part worth keeping.

---

## 3. Composition rules that come from the code, not from taste

1. **Centre the subject, keep it clear of the edges.** `FieldManual_AssetCard.layout:87-92` sets `SizeMode Fill` with `AspectRatioForce 1.276` — wider than the 4:3 source, so **roughly 4% is shaved off the left and right**. Anything at an edge gets clipped.
2. **Final size is 400 x 300 PNG**, into `UI/Textures/FieldManual/Tiles/`, named exactly as listed below. Downscale from whatever Midjourney gives you.
3. **Keep the `.png` beside the imported `.edds`** — that is what `default_ui.png`/`.edds` already do, and it means the source survives.
4. **The card renders at 260 px wide.** Fine detail will not survive. Favour a bold, readable silhouette over intricate linework.
5. **No text in the image.** The tile draws its own title below the art, and generated lettering is reliably wrong.

---

## 4. The twelve tiles

Each entry lists the filename, what the manual page actually says (so the art matches the text), and two genuinely different concepts.

---

### 1. `welcome_ui.png` — Welcome to Overthrow

*Page says: a persistent occupied-island sandbox, no assigned objectives, the resistance grows from what the player chooses to do.*

**Option A — the irregular fighter**
```
A lone irregular resistance fighter in plain civilian clothes and a flat cap, worn rifle slung over one shoulder, standing and looking out across a small Mediterranean island town below, black ink illustration, hand-inked technical field manual drawing, fine pen line art with cross-hatch shading, monochrome black ink on off-white textured paper, subject isolated with generous margins, no background scenery, no horizon, 1980s Cold War military instruction manual aesthetic --ar 4:3 --style raw --no color, colour, gradient, photo, background scenery
```

**Option B — the island and the armband**
```
A folded paper map of a small Mediterranean island with a resistance armband laid across one corner, a worn rifle beside it, flat lay from above, black ink illustration, hand-inked technical field manual drawing, fine pen line art with cross-hatch shading, monochrome black ink on off-white textured paper, subject isolated with generous margins, no background scenery, no horizon, 1980s Cold War military instruction manual aesthetic --ar 4:3 --style raw --no color, colour, gradient, photo, background scenery
```

---

### 2. `main-menu_ui.png` — Main Menu

*Page says: the keybind that opens it, what the menu holds, and the resistance funds section.*

**Option A — the field notebook**
```
A hand holding open a worn military field notebook showing a tabbed list of sections, other hand turning a page, black ink illustration, hand-inked technical field manual drawing, fine pen line art with cross-hatch shading, monochrome black ink on off-white textured paper, subject isolated with generous margins, no background scenery, no horizon, 1980s Cold War military instruction manual aesthetic --ar 4:3 --style raw --no color, colour, gradient, photo, background scenery
```

**Option B — the pointing hand and the panel**
```
A simple rectangular menu panel drawn as a flat grey silhouette with blank list rows, a pointing hand approaching it from the lower right, black ink illustration, hand-inked technical field manual drawing, fine pen line art with cross-hatch shading, monochrome black ink on off-white textured paper, subject isolated with generous margins, no background scenery, no horizon, 1980s Cold War military instruction manual aesthetic --ar 4:3 --style raw --no color, colour, gradient, photo, background scenery
```

---

### 3. `your-home_ui.png` — Your Home

*Page says: what owned means, the starting house and car, respawn point and fast-travel destination.*

**Option A — house and car**
```
A small simple Mediterranean village house with shuttered windows and a tiled roof, a battered civilian sedan parked at the kerb outside, three quarter view, black ink illustration, hand-inked technical field manual drawing, fine pen line art with cross-hatch shading, monochrome black ink on off-white textured paper, subject isolated with generous margins, no background scenery, no horizon, 1980s Cold War military instruction manual aesthetic --ar 4:3 --style raw --no color, colour, gradient, photo, background scenery
```

**Option B — the key**
```
An old iron door key resting in an open palm, a small shuttered village house drawn smaller behind it as a flat grey silhouette, black ink illustration, hand-inked technical field manual drawing, fine pen line art with cross-hatch shading, monochrome black ink on off-white textured paper, subject isolated with generous margins, no background scenery, no horizon, 1980s Cold War military instruction manual aesthetic --ar 4:3 --style raw --no color, colour, gradient, photo, background scenery
```

---

### 4. `map-and-travel_ui.png` — The Map and Fast Travel

*Page says: what map info shows, the map-in-inventory requirement, what fast travel needs and that its cost is a difficulty setting.*

**Option A — hands and paper map**
```
Two hands holding an unfolded creased paper map, a dashed route line running between two marked settlements, small compass rose in one corner, black ink illustration, hand-inked technical field manual drawing, fine pen line art with cross-hatch shading, monochrome black ink on off-white textured paper, subject isolated with generous margins, no background scenery, no horizon, 1980s Cold War military instruction manual aesthetic --ar 4:3 --style raw --no color, colour, gradient, photo, background scenery
```

**Option B — the route**
```
A flat lay map fragment with two circled map markers joined by a long dashed travel line, a lensatic compass and a pencil resting on top, viewed from directly above, black ink illustration, hand-inked technical field manual drawing, fine pen line art with cross-hatch shading, monochrome black ink on off-white textured paper, subject isolated with generous margins, no background scenery, no horizon, 1980s Cold War military instruction manual aesthetic --ar 4:3 --style raw --no color, colour, gradient, photo, background scenery
```

---

### 5. `shops_ui.png` — Money and Shops

*Page says: where money comes from, that shops buy and sell, that prices move with the town and its stock.*

**Option A — the counter**
```
A small village shop counter seen from the customer side, shelves of tinned goods and supplies behind it, a few banknotes and coins lying on the counter top, black ink illustration, hand-inked technical field manual drawing, fine pen line art with cross-hatch shading, monochrome black ink on off-white textured paper, subject isolated with generous margins, no background scenery, no horizon, 1980s Cold War military instruction manual aesthetic --ar 4:3 --style raw --no color, colour, gradient, photo, background scenery
```

**Option B — the exchange**
```
One hand passing a folded bundle of banknotes to another hand receiving a small wooden supply crate, the two hands meeting in the centre, black ink illustration, hand-inked technical field manual drawing, fine pen line art with cross-hatch shading, monochrome black ink on off-white textured paper, subject isolated with generous margins, no background scenery, no horizon, 1980s Cold War military instruction manual aesthetic --ar 4:3 --style raw --no color, colour, gradient, photo, background scenery
```

---

### 6. `gun-dealers_ui.png` — Gun Dealers

*Page says: what a dealer is, how it differs from a general shop, one fixed spot per town, not in villages.*

**Option A — the crate**
```
An open wooden arms crate packed with straw and old bolt action and semi automatic rifles, lid leaning against its side, black ink illustration, hand-inked technical field manual drawing, fine pen line art with cross-hatch shading, monochrome black ink on off-white textured paper, subject isolated with generous margins, no background scenery, no horizon, 1980s Cold War military instruction manual aesthetic --ar 4:3 --style raw --no color, colour, gradient, photo, background scenery
```

**Option B — the back room deal**
```
A rifle laid across a plain wooden table beside a stack of banknotes and a few loose magazines, a bare hanging bulb above casting a small pool of light, black ink illustration, hand-inked technical field manual drawing, fine pen line art with cross-hatch shading, monochrome black ink on off-white textured paper, subject isolated with generous margins, no background scenery, no horizon, 1980s Cold War military instruction manual aesthetic --ar 4:3 --style raw --no color, colour, gradient, photo, background scenery
```

---

### 7. `wanted-system_ui.png` — The Wanted System

*Page says: most triggers need a witness, violence does not; escalation jumps, decay steps down only while unobserved.*

**Option A — being seen** *(recommended: "seen" is the mechanic)*
```
A soldier in 1980s Cold War uniform raising binoculars toward the viewer, a smaller civilian figure drawn as a flat grey silhouette caught in the distance, black ink illustration, hand-inked technical field manual drawing, fine pen line art with cross-hatch shading, monochrome black ink on off-white textured paper, subject isolated with generous margins, no background scenery, no horizon, 1980s Cold War military instruction manual aesthetic --ar 4:3 --style raw --no color, colour, gradient, photo, background scenery
```

**Option B — the eye and the fugitive**
```
A large flat grey silhouette of a watching eye, a small figure in civilian clothes hurrying away from it drawn in fine black ink, black ink illustration, hand-inked technical field manual drawing, fine pen line art with cross-hatch shading, monochrome black ink on off-white textured paper, subject isolated with generous margins, no background scenery, no horizon, 1980s Cold War military instruction manual aesthetic --ar 4:3 --style raw --no color, colour, gradient, photo, background scenery
```

---

### 8. `skills_ui.png` — Skills and Levelling

*Page says: how XP is earned, levels and points, the three skills, the character sheet. Not a stealth system — the sub-category name is a shelf label.*

**Option A — the record sheet**
```
An open personnel record sheet on a clipboard, ruled rows with hand marked tally chevrons filled in along each row, a pencil resting across it, black ink illustration, hand-inked technical field manual drawing, fine pen line art with cross-hatch shading, monochrome black ink on off-white textured paper, subject isolated with generous margins, no background scenery, no horizon, 1980s Cold War military instruction manual aesthetic --ar 4:3 --style raw --no color, colour, gradient, photo, background scenery
```

**Option B — the chevrons**
```
Three stacked military rank chevrons rising in size from bottom to top, the lowest inked solid and the highest drawn in outline only, black ink illustration, hand-inked technical field manual drawing, fine pen line art with cross-hatch shading, monochrome black ink on off-white textured paper, subject isolated with generous margins, no background scenery, no horizon, 1980s Cold War military instruction manual aesthetic --ar 4:3 --style raw --no color, colour, gradient, photo, background scenery
```

---

### 9. `recruits_ui.png` — Recruits

*Page says: civilians become recruits, they cost money, they take orders, they persist, up to sixteen.*

**Option A — the handover**
```
A resistance fighter handing an old rifle to a civilian in workers clothes who is reaching to take it, both figures full length facing each other, black ink illustration, hand-inked technical field manual drawing, fine pen line art with cross-hatch shading, monochrome black ink on off-white textured paper, subject isolated with generous margins, no background scenery, no horizon, 1980s Cold War military instruction manual aesthetic --ar 4:3 --style raw --no color, colour, gradient, photo, background scenery
```

**Option B — the file**
```
Four irregular fighters in mismatched civilian and surplus clothing walking in single file carrying rifles, seen from the side, black ink illustration, hand-inked technical field manual drawing, fine pen line art with cross-hatch shading, monochrome black ink on off-white textured paper, subject isolated with generous margins, no background scenery, no horizon, 1980s Cold War military instruction manual aesthetic --ar 4:3 --style raw --no color, colour, gradient, photo, background scenery
```

---

### 10. `camps_ui.png` — Camps and Placing

*Page says: what placing is, what a camp gives you, the hundred metre spacing rule, and that some placeables are illegal and can be seen.*

**Option A — the camp**
```
A small hidden camp with two canvas tents, a few stacked supply crates and a covered fire pit, drawn as a compact group, black ink illustration, hand-inked technical field manual drawing, fine pen line art with cross-hatch shading, monochrome black ink on off-white textured paper, subject isolated with generous margins, no background scenery, no horizon, 1980s Cold War military instruction manual aesthetic --ar 4:3 --style raw --no color, colour, gradient, photo, background scenery
```

**Option B — placement** *(mirrors the base game's own "Placing" tile)*
```
A single canvas tent viewed in three quarter, a bold arrow pointing down at it from above and a dashed rectangular outline marked on the ground beneath it, black ink illustration, hand-inked technical field manual drawing, fine pen line art with cross-hatch shading, monochrome black ink on off-white textured paper, subject isolated with generous margins, no background scenery, no horizon, 1980s Cold War military instruction manual aesthetic --ar 4:3 --style raw --no color, colour, gradient, photo, background scenery
```

---

### 11. `fobs_ui.png` — FOBs and Building

*Page says: what a FOB is, how it differs from a camp, that building needs supplies and a build zone, officer-only by default.*

**Option A — the emplacement**
```
A partly built forward position, stacked sandbag wall, a watch position and canvas shelter behind it, supply crates piled to one side, black ink illustration, hand-inked technical field manual drawing, fine pen line art with cross-hatch shading, monochrome black ink on off-white textured paper, subject isolated with generous margins, no background scenery, no horizon, 1980s Cold War military instruction manual aesthetic --ar 4:3 --style raw --no color, colour, gradient, photo, background scenery
```

**Option B — the supply truck**
```
A 1980s military cargo truck with its tailgate down and wooden supply crates being unloaded onto a stack beside a low sandbag wall, black ink illustration, hand-inked technical field manual drawing, fine pen line art with cross-hatch shading, monochrome black ink on off-white textured paper, subject isolated with generous margins, no background scenery, no horizon, 1980s Cold War military instruction manual aesthetic --ar 4:3 --style raw --no color, colour, gradient, photo, background scenery
```

---

### 12. `base-capture_ui.png` — Capturing Bases

*Page says: what a base is, what capturing involves, what changes on control change, and that the occupying faction answers back.*

**Option A — the flag change**
```
A military base guard tower and chain link perimeter gate, a flag on a tall pole beside it being raised, seen from outside the wire, black ink illustration, hand-inked technical field manual drawing, fine pen line art with cross-hatch shading, monochrome black ink on off-white textured paper, subject isolated with generous margins, no background scenery, no horizon, 1980s Cold War military instruction manual aesthetic --ar 4:3 --style raw --no color, colour, gradient, photo, background scenery
```

**Option B — the assault**
```
Three irregular fighters with rifles advancing low toward a fortified compound gate and guard tower drawn as a flat grey silhouette ahead of them, black ink illustration, hand-inked technical field manual drawing, fine pen line art with cross-hatch shading, monochrome black ink on off-white textured paper, subject isolated with generous margins, no background scenery, no horizon, 1980s Cold War military instruction manual aesthetic --ar 4:3 --style raw --no color, colour, gradient, photo, background scenery
```

---

## 5. Optional body banners

**1024 x 576 (16:9)**, into `UI/Textures/FieldManual/Body/`, same `<kebab>_ui.png` naming. These sit inside the page body rather than on the tile, so a wider scene with some setting is appropriate — the "no background scenery" rule can relax here. **Entirely optional.** Only Welcome and Capturing Bases are worth one.

Swap `--ar 4:3` for `--ar 16:9`.

**`Body/welcome_ui.png`**
```
A wide view across a small Mediterranean island, terraced fields and a coastal town below, a lone irregular fighter in civilian clothes standing in the near foreground looking out over it, black ink illustration, hand-inked technical field manual drawing, fine pen line art with cross-hatch shading, monochrome black ink on off-white textured paper, wide panoramic composition, 1980s Cold War military instruction manual aesthetic --ar 16:9 --style raw --no color, colour, gradient, photo
```

**`Body/base-capture_ui.png`**
```
A wide view of a military base compound with guard towers, vehicle revetments and a perimeter fence, thin smoke rising from one corner, small figures advancing from the treeline at the left, black ink illustration, hand-inked technical field manual drawing, fine pen line art with cross-hatch shading, monochrome black ink on off-white textured paper, wide panoramic composition, 1980s Cold War military instruction manual aesthetic --ar 16:9 --style raw --no color, colour, gradient, photo
```

---

## 6. Delivery checklist

| Step | Detail |
|---|---|
| 1 | Generate, upscale, crop to **4:3** with the subject centred and clear of the edges |
| 2 | Downscale to **400 x 300** PNG (banners: **1024 x 576**) |
| 3 | Name exactly as listed above, `<kebab>_ui.png` |
| 4 | Import in Workbench as `.edds` into `UI/Textures/FieldManual/Tiles/` (banners: `Body/`) |
| 5 | Keep the source `.png` in the repo beside the `.edds` |
| 6 | Tell me, and I will re-run Phase 6: read each new GUID from its `.meta` (never invented), wire it to its entry, and re-run the gates |

**Partial delivery is expected and fine.** Any entry without its own tile keeps `default_ui.edds`.
