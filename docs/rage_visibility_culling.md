# Visibility and culling in GTA IV (RAGE)

How the engine decides which map entities end up on screen each frame.  
Addresses are for `GTAIV.exe` 1.2.0.59 (Complete Edition) with image base `0x400000`.

## The short version
1. The frame is rendered by several **render phases** (main scene, shadows, reflections, ...). Every phase owns one bit in all visibility masks.
2. The world is split into **map cells**. Every phase projects its view frustum onto the ground and marks the cells it covers.
3. **Worker jobs** visit only the marked cells. Everything linked into an unmarked cell does not exist for the renderer this frame.
4. Each visited entity goes through **LOD/distance selection**, a **frustum test** and an **occlusion test**, once per phase.
5. The survivors form the visible list. Missing models get **streamed in** from that list, and **LOD parents hide** once their children are drawn.

## Render phases
- The current phase is stored at `0x12FB1B8`. Its viewport (`grcViewport`) sits at `phase+0xB0`.
- `phase+0x938` is the phase index, the bit the phase uses in every mask below.
- `phase+0x1A` marks the primary phase (the main camera). It drives lod fading and the camera position used for lod distances.
- `phase+0x8E8` holds flags. Among other things they decide which cell grids the phase marks (`0x200087` fine grid, `0x100` coarse grid).
- `0xAE4880` sets up each phase. It calls `0xAE95E0` for perspective phases and `0xAEA170` for orthographic ones (`phase+0x3C` is 0 or 1, e.g. shadows).

## Map cells
The playable area is clamped to -3000..3000 m and covered by two grids:

| Grid | Cells | Cell size | World to cell | Used by |
|---|---|---|---|---|
| Fine | 120 x 120 | 50 m | `world * 0.02 + 60` | high detail (HD) entities |
| Coarse | 30 x 30 | 200 m | `world * 0.005 + 15` | LOD entities (`CEntity+0x24 & 0x40`) |

- Adding an entity to the world (`0xA30090`) links it into **every** cell its bounding box touches. Removing it (`0xA336A0`) unlinks it again.
- The covered cell rect is stored packed in `CEntity+0x04`: x and y take 11 bits each (both biased by 1024), width and height take 5 bits each.
- Fine cell lists live at `0x11A9D20` (12 bytes per cell, lookup `0x4A48C0`). Coarse lists live at `0x11D4028`.
- Dynamic entity types (objects, peds, vehicles, ...) are kept in separate 16 x 16 hashed buckets at `0x11A8918`. `0xAE47B0` returns them together with a cell's static list.

## Marking visible cells
Once per frame `0xAE64D0` clears two **visibility grids**. Each cell in them holds a 32-bit mask with one bit per phase:
- **near grid** `0x15C1610`: a 48 x 48 window of fine cells centered on the camera (about ±1200 m).
- **far grid** `0x15B30F0`: holds the whole coarse 30 x 30 grid.

Both also keep the min/max marked x per row and the overall bounds. Near grid layout:

| Offset | Content |
|---|---|
| `+0x0000` | origin x, origin y (camera cell - 24) |
| `+0x0008` | `uint32 cells[48][48]` |
| `+0x2408` | per row min x `[48]` |
| `+0x24C8` | per row max x `[48]` |
| `+0x2588` | min x, max x, min y, max y |

For every phase, `0xAE95E0` builds a 2D polygon from the camera position and the four far corners of the frustum. `AddMapSectionsInFrustum` (`0xD62DC0`) rasterizes it and calls back once per covered cell: `0xAE95B0` for the near grid, `0xAE93C0` for the far grid. Each callback ORs the phase bit into that cell.

**Important:** for the fine (HD) grid, the frustum is cut off at **about 300 m** (`300 + (45 - fov) * 0.25` when FOV < 45). The coarse (LOD) grid uses the full far clip.  
So HD entities are only considered roughly 300 m in front of the camera. Beyond that, only LODs are.

## Traversal jobs
Once all phases have marked their cells, `0xAE7530` walks the marked span of every row in both grids. It splits each span into chunks of 10 cells (16 when the main camera is above 100 m) and queues a `"MultiThreadProcessRenderListOnePass"` job for each (`0xAE28D0`, worker `0xAE6CA0`).  
The worker calls `0xAE9260` for coarse rows and `0xAE9400` for fine rows. Cells outside every row span are never touched.

## Per-entity pipeline
For every entity in a visited cell:

1. **Once per frame:** `CEntity+0x3C` is swapped (interlocked) with the global visibility stamp at `0x11A8908`. An entity linked into several cells is therefore processed once.
2. **Phase mask:** `0xAE5E40` ORs the grid cells under the entity's cell rect and stores the result in `CEntity+0x08`, the phases that may see it. `CEntity+0x0C = ~mask` holds the "culled in phase" bits.  
   From here on, a phase sees the entity when its bit is set in `+0x08` and clear in `+0x0C`.
3. **Deferred entities** (`CEntity+0x28 & 0x8000`) are queued and handled later by `0xAE8390`.
4. **LOD / distance selection** `0xAE7990`:
   - It compares the camera distance (measured to the LOD parent if there is one) with the entity's lod distance `CEntity+0x50` times the phase lod scale `phase+0x934`.
   - Entities past their draw range are dropped.
   - An HD entity past its lod distance is kept but flagged `0x40000000` in `+0x08/+0x0C`, so it is not drawn and its LOD takes over.
   - This step also drives the HD ↔ LOD cross-fade: alpha `CEntity+0x63`, and flag `0x80000` in the parent's `CEntity+0x28`.
5. **Frustum test** `0xAE77F0`, which calls `0xAE67A0` per phase:
   - Reflection phases (`phase+0x19 & 6`) first reject entities that lie on the wrong side of the reflection plane.
   - The standard test is `0xA31C20`: the bounding sphere (virtual `+0x50` center, `+0x58` radius) against the phase viewport, using the generic sphere test `0x431E40` (0 outside, 1 intersecting, 2 inside).
   - Phases with an active portal/room context use the plane test `0xA0FCF0` instead.
   - A culled entity gets the phase bit set in `+0x0C`.
6. **Occlusion test** `0xAE8680`, which calls `0xB03B40` per phase:
   - The entity is tested against the phase's occluder set (lists at `0x16010AC`, selected by `phase+0x8F8` / `+0x8FC`).
   - Small entities are tested with their bounding sphere, large ones with their oriented model box. Some phases only test entities with radius ≤ 10.
   - An occluded entity gets the phase bit set in `+0x0C`.
7. `0xAE2520` appends the entity and its sort distance to the frame's visible list.

Entities whose mask ended up 0 still pass step 4 and get appended. They are simply not drawn by any phase.

## After traversal
`0xAE4E80` waits for all jobs, then runs on the main thread:
- `0xAE6080` handles every entity visible in at least one phase. If its drawable is missing (`CEntity+0x34 == 0`), it calls virtual `+0x40`, which requests the model from streaming.  
  **Map model streaming is driven by visibility.** An entity in a never-visited cell is never requested.
- `0xAE8390` processes the deferred entities, and `0xAE8460` submits each phase (`0xA10090`).
- `0xAE8740` builds the render lists of a phase through `0xAE8800`, `0xAE6FA0`, `0xAE8510` and `0xAE7FD0`.

## LOD hierarchy: when LODs hide
- Entities form a tree: HD → LOD → SLOD. `CEntity+0x4C` points to the parent, and `CEntity+0x61` counts its children.  
  The links are made when an IPL loads (`0x9D2650`). The entity destructor (`0xA2FD80`) decrements the parent's child count.
- An HD entity fades in by +16 alpha per frame (`0xAE6FA0`). Entities culled by the frustum or occlusion test in the primary phase get alpha 255, so turning the camera does not make them fade in again.
  - While fading, it ORs its visible phases into the parent's `+0x58` ("children fading in").
  - Once alpha reaches ≥ 239, it ORs them into the parent's `+0x54` ("children drawn"), and into the grandparent's `+0x54` too.
- `0xAE7FD0` runs per phase for parents. It **culls the parent in a phase as soon as any child is fully drawn there and no child is still fading in**.  
  Children that are not drawn (culled, outside the ~300 m HD range, or past their lod distance) do not keep the parent alive. A fading-out parent loses 17 alpha per frame (`0x2000` in the word at `CEntity+0x5C`).
- **Consequence:** a big LOD that stands for several buildings disappears as soon as one of its children is drawn, even if the other children are not drawn at that moment. A large building can blink out this way when its own HD is not processed while a sibling is on screen.
- So a swap is a cross fade: the HD fades in over the opaque LOD, then the LOD fades out over the opaque HD. Entities that just streamed in fade in the same way (`0xAE25A0`).
- The steps are per frame, not per second: `add al, 10h` at `0xAE7243` (HD in), `0xAE8329` (LOD back in) and `0xAE2627` (after streaming), `sub eax, 11h` at `0xAE826B` (LOD out).
- The fade-in sites snap every alpha from 239 up to 255 (`cmp eax, 0EFh`), and 239 = 256 - 17: the first fade-out step lands on 238, just below the snap. A LOD that gets the fade-out and a fade-in in the same frame then drains by one per frame. With a smaller fade-out step the first step lands at or above 239 and the fade-in snaps the LOD back to 255 every frame - it never leaves (seen as a superlod stuck at alpha 255 right next to the camera). The mod leaves all steps as they are.
- A leaf child whose model is not loaded marks its parent "children fading in" every frame (`g_DrawLodIfAnyHighDetailUnloaded`), so a superlod stays until its last child streamed in.
- **Streaming pop of a parent.** `0xAE8800` (`CRenderer::ProcessStreamingOnList`) requests the missing models of visible entities and sets the alpha of an unloaded entity to 0 every frame, so it fades in once its model arrives. But `0xAE7FD0` then forces a parent whose children are unloaded or fading in back to alpha 255 (`0xAE82C0`) and sets `0x2000` in `+0x5C`, which also skips the fade-in step in `0xAE25A0`. The engine prefers a pop over a hole: a LOD must cover its missing HD at once. Streaming loads superlods, then lods, then HD (`g_stream_slods_then_lods_then_hd`), so a LOD always arrives while its children are still missing and pops in at 255 the moment it is loaded, then fades out normally once the HD is drawn. It only happens when the area streams in from scratch, never while the models are resident.  
  The mod hooks `0xAE82C0` (`gta4.cpp`, `lod_fade_in`) and ramps instead: below 239 add 16, else 255. The LOD then enters through the alpha list like every other fade, and while it is below 255 it does not count as drawn for its own parent, so the superlod cross fades out instead of being culled at once. While the model is still missing the alpha idles at 16 (zeroed by `0xAE8800`, stepped by the hook); nothing is drawn without a drawable.
- Within 20 m past its lod distance an HD entity's alpha, and its parent's, are computed from the distance every frame (`0xAE6FA0`), not stepped: a parked camera parks the alpha.

### Drawing a fading entity
An entity below alpha 239 goes to the alpha render list. `0xAE38B0` draws that list:
- It queues `CShaderLib::SetGlobalAlpha` (`0xAC6EF0`), which writes x of the `globalScalars` shader global (`0x103F260`) that the shaders multiply their alpha by.
- Buildings whose models are all loaded (`CLights::CanSplitAlphaObject`, `0xABE080`, true for nearly every building) are drawn geometry by geometry with the area lights of each geometry (`CLights::SplitAndRenderAlphaObject` `0xAC1B30` → `0xAC1900`). They never reach `CEntity::AddToDrawListBuckets`.
- Everything else goes through `CEntity::AddToDrawListBuckets` (`0xA2FE70`) with the alpha. The draw commands it creates (e.g. `CDrawEntityDC` `0x8DCBF0`) set the global alpha again before they draw.
- Only the alpha render list carries the entity's alpha; the split draw renders the geometries of the bucket being drawn (`RenderSplitDrawable` matches each shader's draw bucket). An entity with opaque and alpha buckets (a tree: trunk and canopy) keeps its opaque buckets in the gbuffer list, which calls `AddToDrawListBuckets` with alpha 255 while the entity fades, so the trunk pops in the engine. For the emissive buckets (4 and 5) of an entity whose lod parent fades, `0xAE38B0` passes `255 - parent alpha` instead (the engine blends the HD windows in while the lod's blend out, with the HDR intensity at 0), so the alpha of a call is per bucket and not the entity's. The `AddToDrawListBuckets` hook therefore gives an entity with a lod parent its own alpha (`CEntity+0x63`) below 239 and 255 otherwise, to the engine's command and to the api entity command, so the api instances fade whole and the HD windows stay opaque while the lod dithers out. The opaque side of the cross fade (the HD while its lod parent fades in, the lod while a child that marked it fades out) is sent as the fade partner: the runtime gives its hits to the kept dither cells of the fading side on a shared plane, as the engine's alpha pass blends the fading entity over it. The engine's own draws of a partner (refresh frames, drawables not fully owned) find the mark on the drawable's geometry records.

## Interiors
Interior entities are not visited through the map cells. They are added room by room through the portal traversal (`0xA10140`), which uses the same sphere test `0xA31C20`.

## CEntity fields used by visibility

| Offset | Meaning |
|---|---|
| `+0x04` | packed cell rect (see above) |
| `+0x08` | phases that may see the entity (bit 30: past lod distance) |
| `+0x0C` | phases the entity got culled in |
| `+0x20` | world matrix, translation at `+0x30` |
| `+0x24` | flags: `0x20` visible, `0x40` LOD (coarse grid), `0x40000` frustum-culled in the primary phase this frame, `0x40000000` has a lod parent |
| `+0x28` | flags: bits 6-9 entity type (1 = building), `0x8000` deferred, `0x80000` cross-fading with a child |
| `+0x2E` | model index |
| `+0x34` | drawable, 0 = not loaded |
| `+0x3C` | visibility stamp |
| `+0x4C` | lod parent |
| `+0x50` | lod distance |
| `+0x54` / `+0x58` | on parents: per phase "children drawn" / "children fading in" |
| `+0x5C` | flags word: `0x2000` parent fades out (also blocks the fade-in step), `0x4000` superlod state for the stream boxes |
| `+0x61` | number of lod children |
| `+0x63` | alpha |

## Function index

| Address | Purpose |
|---|---|
| `0x431E40` | grcViewport sphere vs frustum test |
| `0x4A48C0` | fine cell list lookup |
| `0x9D2650` | link lod parents after an IPL loads |
| `0xA0FCF0` | sphere vs phase planes (portal/room context) |
| `0xA10140` | add interior room entities |
| `0xA2FD80` | CEntity destructor (unlinks from lod parent) |
| `0xA2FE70` | CEntity::AddToDrawListBuckets |
| `0xA30090` | link entity into map cells |
| `0xA31C20` | per-entity frustum test (bounding sphere) |
| `0xA336A0` | unlink entity from map cells |
| `0xABE080` | CLights::CanSplitAlphaObject |
| `0xAC1B30` | CLights::SplitAndRenderAlphaObject |
| `0xAC6EF0` | CShaderLib::SetGlobalAlpha (`globalScalars.x`) |
| `0xAE2520` | append to visible list |
| `0xAE25A0` | add entity to a phase's render list (fade in after streaming) |
| `0xAE28D0` | queue a traversal job |
| `0xAE38B0` | draw an entity render list (alpha list: global alpha, split draws) |
| `0xAE4880` | per phase visibility setup |
| `0xAE4E80` | wait for jobs, build render lists |
| `0xAE5E40` | phase mask from visibility grids |
| `0xAE6080` | request missing models of visible entities |
| `0xAE64D0` | reset visibility grids (once per frame) |
| `0xAE67A0` | frustum test for one phase |
| `0xAE6CA0` | traversal job worker |
| `0xAE6FA0` | render list: HD fade-in, child → parent masks |
| `0xAE7530` | dispatch traversal jobs |
| `0xAE77F0` | frustum test over all phases |
| `0xAE7990` | LOD / distance selection |
| `0xAE7FD0` | render list: hide parents with drawn children |
| `0xAE8680` | occlusion test over all phases |
| `0xAE8740` | build render lists of a phase |
| `0xAE8800` | render list: request missing models, alpha 0 for unloaded entities (`ProcessStreamingOnList`) |
| `0xAE9260` | visit coarse (LOD) cells |
| `0xAE9400` | visit fine (HD) cells |
| `0xAE93C0` | far grid mark callback |
| `0xAE95B0` | near grid mark callback |
| `0xAE95E0` | mark cells for a perspective phase |
| `0xAEA170` | mark cells for an orthographic phase |
| `0xB03B40` | occlusion test for one phase |
| `0xD62DC0` | rasterize a 2D polygon into cells |
