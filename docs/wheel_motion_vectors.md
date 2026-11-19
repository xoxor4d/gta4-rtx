# Wheels and motion vectors

Why vehicle wheels smeared under DLSS / frame generation and how that was fixed.  
The fix spans the mod and the runtime (dxvk-remix fork). Addresses are for `GTAIV.exe` 1.2.0.59 with image base `0x400000`.

## The problem
The runtime derives motion vectors from last frame's and this frame's state of an object. Wheels broke that in two ways:

1. **Spin.** At speed a wheel turns a large part of a revolution, or more than one, between two rendered frames. The motion vectors of such a step are useless and DLSS-RR / FG smear the tyre and the rim.
2. **Flat tyres.** The game flattens a burst tyre against the ground in the vertex shader. The mod does the same on the CPU, and because the shape changes while the wheel turns it built a new mesh every frame. A new mesh has no history, so no motion vectors at all, at any speed.

## How the game spins a wheel
- Every `CWheel` (stride `0x170`) holds its spin angle at `+0x7C` (radians, wrapped to ±π) and its angular velocity at `+0x80` (rad/s).
- Physics integrates the angle (`angle += dt * velocity`).
- `CWheel::ProcessWheelMatrix(CVehicle*)` at `0xACF2D0` is the only place the angle reaches the skeleton. The car and bike PreRender call it once per wheel per rendered frame.
- Everything drawn afterwards (instanced tyres, skinned tyres, the D3D9 fallback) reads that bone.

## Mod: limiting the spin
`wheel_spin` in `gta4.cpp` detours `ProcessWheelMatrix`:

1. The true step since the last frame is the wrapped angle difference plus the whole turns that got lost. The angular velocity times `CTimer::ms_fTimeStep` tells how many.
2. That step is clamped to `vehicle_wheel_spin_max_step` degrees and added to a shown angle that is kept per wheel.
3. The function runs with the shown angle, then the wheel gets its real angle back. Physics, audio and network never see the change.

Below the limit a wheel follows the game exactly. Above it the wheel turns at a constant rate, which scales with the frame rate because the limit is per frame. Vehicles under animation skip `ProcessWheelMatrix` and are not limited.

Settings (World page): `vehicle_wheel_spin_limit` and `vehicle_wheel_spin_max_step` (Advanced > Vehicles > Wheels).

## Mod: flat tyres
The tyre shader has a deform switch that the game turns on for **every** wheel of a vehicle once one tyre is burst. `try_draw_vehicle` in `renderer_api.cpp` now tells three cases apart by the per wheel deform constants (`tyreDeformParams2`):

| Case | Constants | Mesh |
| :-- | :-- | :-- |
| Intact wheel | radius 0 | the shared wheel mesh, like on a car without a burst tyre |
| Rubber gone (rim only) | amount 0 | one mesh per radius, the shape does not depend on the rotation |
| Flat with rubber | otherwise | one mesh per wheel, its vertices are sent again every frame |

For the last case the mesh needs a hash that stays the same for one wheel across frames. The instanced tyre draws carry no vehicle, so `vehicles::flat_wheel_id` finds a wheel again by where it should be now (last position plus last movement). Once per frame the vertices are deformed again and sent with `CreateMesh` under that hash.

A wrong match only costs a frame of history. It can happen when a tyre first shows up flat while the wheel travels more than about 4 m per frame.

## Runtime: updating a mesh in place
Upstream ignores a second `CreateMesh` for a handle that exists. On the fork a repeated registration with the same layout (surface count, vertex count, index count, bones, material) replaces the vertices:

- `AssetReplacer::registerExternalMesh` keeps the topology hashes of the mesh and takes the new position hash.
- The draw finds its BLAS entry again and `SceneManager::processGeometryInfo` takes the update path: the BLAS is refit and last frame's vertices become the previous positions the motion vectors are computed from.
- `externalDrawTryPreserve` compares the position hash, so an instance that does not move (a parked car) still gets the new vertices.

The bridge passes `CreateMesh` through unchanged and needed no change.

## Background
The first attempt at this was a pull request by spartaque12 that limited the spin where the wheels are submitted to the Remix API. Hooking the game instead covers every draw of a wheel with one function and knows the spin direction from the angular velocity.
