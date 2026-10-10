# Box2D (OpenWheels)

The Box2D every OpenWheels build links (`ow_box2d`, `CMakeLists.txt` here). It replaces the engine's
prebuilt `libbox2d` (`ext_box2d` of cocos2d-x's v3-deps-158) in the engine's `external` target, so
the game, the engine and its extensions all use this one.

## Where it comes from

`Box2D/` is [erincatto/box2d](https://github.com/erincatto/box2d) at
`f655c603ba9d83f07fc566d38d2654ba35739102` with cocos2d-x-3rd-party-libs-src's `cocos2d.patch`:
the source the v3-deps-158 prebuilts were built from (their headers are byte for byte these). The
files keep upstream's CRLF line endings; edit them without converting.

Built from source with `-ffp-contract=off -fno-fast-math` (every float operation rounded on its
own, like the arm64 prebuilt the original game shipped with). With the browser switch off it is
bit-identical to the prebuilt on the Linux harness used for this change (2000 steps of stacks,
ragdolls, prismatic / distance joints and fast bodies) and plays 14 of 16 sampled campaign levels
identically to a build linked with the old prebuilt; the two Irresponsible Dad levels that differ
(an extra gore body after frame 480) differ in the same way with unpatched source, i.e. from how
the old prebuilt was compiled, not from the patches below. Windows now uses an MSVC SSE build of it
instead of the x87 prebuilt.

## OpenWheels patches: the browser game's Box2D 2.0 solver

The browser Happy Wheels (Flash) ran Box2DFlash 2.0.2. OpenWheels' browser physics profile
(`src/online/FlashPhysics.h`) switches the solver to its rules while a browser level's world steps:
`g_flash20Solver` (`Common/b2Settings.*`), set only around `online::flashWorldStep`'s `Step`, like
the engine's own `g_blockSolve`. Everything below is behind that switch; with it off (campaign
levels, editor levels, browser physics off) the code paths are upstream's. No data member was added
to any Box2D class (the joint solvers keep their extra state in solver temporaries that the 2.3
path recomputes every step), so object layout and allocation order are unchanged.

| Box2D 2.0.2 rule | where |
| --- | --- |
| Velocity clamp before solving: 200 m/s, 250 rad/s (2.3: 2 m and pi/2 rad per step, after) | `b2Island.cpp`, `b2Settings.h` |
| Damping `v *= clamp(1 - dt * d, 0, 1)` (2.3: `1 / (1 + dt * d)`) | `b2Island.cpp` |
| Sleep: angular tolerance 2/180 rad/s (the 2.0 constant, no pi), no "position solved" requirement | `b2Island.cpp` |
| Velocity iterations solve contacts before joints | `b2Island.cpp` |
| Contacts: per-point normal + friction solve (friction bounded by the point's normal impulse before this iteration), no block solver, bias -60 x separation | `b2ContactSolver.*` |
| Contact position correction: world anchors, equalized masses for points on anchored bodies, accumulated position impulse >= 0, early out at -1.5 x linear slop | `b2ContactSolver.*` |
| Revolute joint: point, then motor, then limit; limit position correction with an accumulated impulse; equal limits hold the angle | `b2RevoluteJoint.*` |
| Prismatic joint: separate perpendicular, angular, motor and limit constraints with Jacobians frozen at init | `b2PrismaticJoint.*` |

Also, for drawing only: `b2Body::GetSweepForDrawing` / `SetStateForDrawing` let
`src/online/RenderInterpolation.cpp` put a body at an in-between pose while the level is painted
and put the simulated state back bit for bit afterwards.

Not patched (2.3 behaviour stays): continuous collision (TOI), the distance, weld, rope, pulley,
gear, mouse and wheel joints, the broad-phase, and the narrow-phase contact generation (FlashRuntime
covers the polygon skin and bounding-box wakes). Box2DFlash computed in doubles; this is float.
