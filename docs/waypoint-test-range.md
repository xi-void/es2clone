# Waypoint Test Range

Select **operation 04 / 05 — Waypoint Test Range** with the existing title-screen arrows. From Dust to Dust, press **Right** three times. Deploy normally, wait for the existing reactor boot, and drive toward the marked WP1. The range has no enemies, structures, combat objectives, extraction task or campaign rewards. The original cockpit, movement, startup, soundtrack and audio settings are retained.

The flat playable area is 600 × 600 meters. Insertion is `(0, 0, 200)`; the route is `(0, 0, 100)`, `(0, 0, 0)`, `(100, 0, 0)`, `(100, 0, -100)`, `(0, 0, -100)`. Consecutive legs are 100 meters. These use the existing world axes: X/Z are ground coordinates and Y is height, in meters. The compass, radar, distance readout and ground beacon all show the active navigation point.

WP1 is active immediately. Entering its unchanged **24-meter radius** plays the existing `waypoint_reached` event and activates WP2; the same applies through WP5, which completes the mission. The check uses the same collision-resolved outside-to-inside crossing and once-only latch as ordinary missions, with three-dimensional distance between ground positions. Displaying or selecting a waypoint does not play speech. Remaining inside, moving around inside or returning to an already reported waypoint cannot repeat it. Restart Test on the debrief, or Restart Mission in pause, resets to WP1.

Test definitions are isolated in `src/test_missions.hpp` using the existing C++ `Mission` and `MissionLayout` data structures. The layout has five coordinate slots and optional route count, flat-ground flag and boundary; campaign entries retain their original coordinates, terrain, bounds, enemy counts and progression. The project does not yet decode original ES2 mission files, so this route is original test content for the engine, not a claimed original ES2 mission.

## Confirmed failure

The previous `navigationPoint()` started with a mission sector position but replaced it with the nearest living enemy's position. Both the cockpit and arrival detector consumed that moving target. Wave/objective completion, meanwhile, was based on kills and extraction rather than navigation arrival.

A temporary instrumented run of the actual native executable reproduced the mismatch while driving from insertion toward the first sector with the normal enemy AI active. At simulation time **24.6414 s**, the detector emitted arrival near an enemy, approximately **96 m before the mission sector**. The event reached the main loop and `Audio::play()` with one mapped recording. At **31.0079 s**, the HERC was only **13.2765 m** from the sector, but the displayed enemy-based point was **53.7699 m** away and the latch was already consumed. No arrival event was emitted there. The former tests removed enemies or positioned the HERC beside the moving marker, hiding this defect.

The fix keeps the active navigation anchor at its mission-defined coordinates. It does not enlarge the radius, retune audio or change combat stage completion: defeating a patrol still activates the next patrol stage, and clearing the final patrol still unlocks extraction. Route-only missions advance on arrival. Both modes use the single arrival block in `Combat::update()` and the existing `Event::WaypointReached` → main loop → `Audio::play()` → notification/interface bus path. The arrival voice continues through the final transition to debrief.

The original `SYSTEM.STR` message 29, `WAYPOINT REACHED`, has metadata `1d0000030600141f`. The verified executable message loader uses the final byte as the voice number: `0x1f` = 31, selecting `CVM_0031.WAV`. The installed file matches the extracted PCM byte for byte. See [audio identification](original-audio.md#waypoint-arrival-notification). All audio mappings, volumes and recordings, including disabled/destroyed speech, are unchanged by this fix. No placeholder voice was added.

Temporary diagnostic logging was removed from production sources. Local traces and captures are retained only in ignored `imported/waypoint-runtime-audit/`.

## Verification

```sh
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./tests/native-smoke.sh
SDL_AUDIODRIVER=dummy ./build/waypoint_tests assets/audio imported/waypoint-arrival.wav
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy ./build/earthsiege \
  --smoke-test --scene waypoints --size 1280x720 \
  --save-dir imported/waypoint-pilot --audio-capture imported/waypoint-route.wav
```

The waypoint regression drives all three chassis through the real test mission using normal throttle and turning, without teleporting or manually advancing the route. It covers the first arrival, ordering, silence before arrival and while stationary, single notifications, final completion, restart, collisions, fallback and original playback. A campaign test retains moving enemies and checks that speech only fires at the fixed sector anchor. Existing leg/torso combat notification and audio regressions remain in place.

The native `waypoints` smoke scene selects the range through the existing interface, checks selection persistence, launches, uses the pause-menu restart button, drives the full route, forwards all five events through the normal audio path, and stays in debrief for three seconds to let the final speech finish. `--audio-capture` runs it in real time and records SDL callback output alongside machinery and the supplied soundtrack. Captures contain private original media and must remain outside Git/packages.

Offscreen/dummy-driver execution and callback recordings verify the rendered game's event and mixing paths. They do **not** establish audible speaker playback. The development sandbox cannot connect to the desktop PulseAudio/PipeWire server. Manually launch `./run.sh` in the desktop session, select the range, and confirm five clearly audible notifications at your saved volume settings. Also check turning toward each beacon and restarting from debrief. No original assets are needed for navigation testing; authentic speech requires the existing local recordings.

Local verification results: build and all eight CTest suites passed; the optional original-asset run passed 71 waypoint checks and 52 asset/stream/SDL checks. All nine existing native smoke scenes passed. The actual executable completed the full range at 1280×720 and 1920×1080, reporting five arrivals in each run. The second run used a temporary snapshot of the pilot's saved volumes (music 0.82, effects 0.8, machinery 1, interface 1), without changing the pilot save or audio configuration. Both callback captures contain five matches for the original speech amid the existing machinery/music, including WP5 in debrief, with finite output bounded at 0.97. Menu, WP1–WP5 and debrief screenshots were captured at both sizes; the selector, WP1 and debrief were visually inspected. A separate attempt to open SDL's PulseAudio driver failed with `Operation not permitted`; live speaker audibility is not claimed.

All 34 existing audio-directory files, the reviewed mapping CSV, and every audio loading/mixing source file compared byte-identical with the pre-change snapshot. Original recordings and captures remain ignored. The previous enemy disabled/destroyed tests still pass.

Files changed: `src/simulation.hpp`, `src/simulation.cpp`, `src/main.cpp`, `src/renderer.cpp`, `src/original_cockpit.cpp`, `tests/waypoint_tests.cpp`, `README.md`, `docs/battlefield.md`, and `docs/original-audio.md`. Files added: `src/test_missions.hpp` and this document. No audio mapping, audio implementation, dependency or original resource was changed.
