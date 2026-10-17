# Battlefield, deployment and cockpit increment

This is a playable compatibility step, not a verified reproduction of original ES2 missions or sensor rules. The original Tomahawk artwork, accepted audio associations, volume settings, turbo behavior and contact-driven suspension are retained.

## Radar and navigation

The upper-left windshield has a heading-up radar. The white triangle is the chassis; the short green ray shows torso aim. Cyan contacts are living enemies, with a green bracket on the selected target. Amber diamonds point to the current defenders; the extraction diamond becomes green when available. Small muted squares mark four relay/outpost landmarks. North rotates around the range ring.

Enemy and structure contacts use actual horizontal positions and disappear outside the range. Navigation diamonds remain on the rim when their destinations are farther away; they are bearings, not false nearby contacts. The compass also shows the current waypoint bearing, and its readout gives distance. The default radius is 800 meters. Use `--radar-range 1200` at launch, or **[ / ]** in flight to adjust in 200-meter steps between 200 and 2400 meters. This session setting does not alter pilot saves or audio configuration.

`radar.hpp` separates coordinate projection and the range test from drawing. Terrain occlusion, active/passive sensors, jamming and component damage are not implemented. Current targeting still uses the existing target-selection rules.

## Scale and routes

The playable bounds are ±1200 meters on both horizontal axes: **2.4 × 2.4 km, 5.76 km²**, approximately 16 times the former 620 × 570-meter area. Three mission layouts separate landing zones, defender sectors and extraction. Landing-to-extraction straight-line distances exceed 1.5 km; the first sector is about 390 meters away, with later sectors over 500 meters apart. Existing enemy totals and loadouts remain unchanged.

Broad terrain folds, an outer basin rim, four antenna landmarks and separated outposts provide orientation. The old rubble line was moved east to clear the extraction corridor. Far defenders hold their sector outside 420 meters unless recently hit. Defenders for subsequent sectors appear through the existing wave progression. These are custom layouts, not decoded original missions.

Mech speeds, acceleration, turbo spool, weapon parameters and gait calculations retain their prior values. The terrain is a cached 140 × 140-cell mesh (39,200 triangles); the distant mountain mesh and prop counts remain bounded. Fog ends at 2300 meters and the far plane is 3600 meters. No per-frame terrain generation, new rendering framework or dependency was added.

## Insertion and messages

During the existing 6.2-second boot sequence, the viewpoint descends from 45 meters above the landing point, touching down at the existing 3.55-second mechanical activation cue. A small damped landing displacement settles before controls unlock. Movement, firing, enemy AI and the mission clock stay interlocked during startup, as before. The original boot soundtrack continues with its established timing.

This is a concise deployment presentation anchored over the ground spawn point. It does not yet simulate a dropship, airborne collisions or original deployment scripts. After initialization, the deployment offset is zero and camera movement uses the unchanged foot-contact suspension.

A cyan marquee enters, holds, and exits below the waypoint readout. It announces mission start and reports actual lethal hitbox results: **ENEMY TARGET DISABLED** for a leg kill, **ENEMY TARGET DESTROYED** for torso destruction. Its bounded queue prevents salvage/sector notices from replacing kill reports. Damaging a wreck cannot repeat the notification. Pause freezes it with the simulation.

The two new voice events use verified original `CVM_0051.WAV` and `CVM_0050.WAV` respectively. They accompany the existing explosion event, use the interface volume, and do not change existing mixing rules. Missing speech uses a quiet notification tone and readable text. See [identification evidence](original-audio.md#kill-notification-identification).

## Cockpit corrections and limits

The original frame and instruments now use one 640 × 480 logical coordinate system within a shared 4:3 scene viewport. Text, strokes and projected target marks scale with that viewport. Weapon row outlines are inset, schematic lines are thinner, and long lower messages fit within the windshield.

Projected targeting marks previously drew over cockpit pillars and panels. They now render before the original frame, whose alpha cutout masks them. Filtering uses premultiplied alpha and linear texture sampling to avoid dark interpolation fringes and uneven nearest-neighbor enlargement during fractional suspension movement. Source files and palette/index decoding are unchanged.

The decoded artwork itself contains alternating light/dark edge pixels. Inspection of the indexed bitmap and palette did not establish a corrupt texture or a channel-order/stride error. Filtering reduces enlargement shimmer; it deliberately retains the original edge detail. Original GAU/HFN/VUE/HD instrumentation and clipping semantics still need decoding before the whole cockpit can match the reference screenshot. Current fonts and instrument drawings remain approximations.

## Validation

```sh
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./tests/native-smoke.sh
SDL_AUDIODRIVER=dummy ./build/audio_assets_tests assets/audio
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy ./build/battlefield_render \
  assets assets/original imported/battlefield-review
```

The last two checks require the local original recordings and supplied soundtrack/artwork. The render check captures nine states at both 1920×1080 (the current display resolution) and 1280×720, compares opaque pillar pixels with/without overlapping enemy brackets, and sends both kill outcomes from simulation through the SDL callback into local WAV captures. Keep those screenshots and captures in ignored `imported/`.

Simulation tests cover radar orientation/range, mission spacing, insertion descent and control interlocks, actual leg/torso weapon hits, duplicate suppression, and completion of all three operations using ordinary weapons and movement. Existing gait/turbo and audio regressions remain in place. The local before/after audit compares every old event's rendered PCM and continuous layers, mappings, volumes and source asset hashes.

Offscreen rendering and SDL's dummy audio driver verify native rendering and audio sample delivery in the restricted environment. They do not constitute a physical speaker or integrated-GPU performance test. Closer fidelity needs original sensor/deployment behavior measurements and the undecoded graphics/terrain/mission resources; no missing asset blocks this increment.
