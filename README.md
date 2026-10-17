# OpenEarthSiege 2

A **native Linux reimplementation project** for the original 1996 EarthSiege 2, written in C++17 with SDL2 and OpenGL. The goal is original game compatibility, with engine code independent of copyrighted assets and optional remastered music. The current playable prototype supports original audio and a first original cockpit; its terrain, chassis statistics, combat rules and custom missions remain approximations. It does not yet run the original campaign.

See the [compatibility assessment and practical roadmap](docs/compatibility-roadmap.md) for verified systems, placeholders, undecoded resources, and the next implementation steps. Existing executable names and save locations are retained for compatibility.

## Run

If using the packaged archive, extract it and run `./earthsiege` from the extracted folder. Keep the `assets` directory beside the executable.

From this source directory:

```sh
./run.sh
```

The launcher builds the game and opens a native desktop window. To run the already compiled executable directly:

```sh
./build/earthsiege
```

No browser, web server, network connection, or original game files are required. Launch from a terminal in your graphical Linux desktop session. Both X11 and Wayland are supported through SDL. The window is resizable; **F11** toggles fullscreen.

Build dependencies: a C++17 compiler, CMake 3.16+, pkg-config, SDL2, SDL2_ttf, libsndfile 1.1+ (with MP3, FLAC and Vorbis codecs), Python 3, and OpenGL development libraries. These were already available on the development machine. Runtime dependencies are SDL2, SDL2_ttf, libsndfile, the C++ runtime, and an OpenGL 2.1-compatible graphics driver. SDL2 compatibility implementations backed by SDL3 work too.

## Original cockpit

Keep the ISO in ignored `original/`. Import the original Tomahawk cockpit shown in the reference image:

```sh
python3 tools/import_original_graphics.py original/Earthsiege2_Freeware_HiRezStudios_1r0.iso
./run.sh
```

This reads only the needed archive and copies two graphics resources into ignored `assets/original/`; it leaves every audio association unchanged. The game loads the original bitmap and palette directly, preserving 4:3 proportions, and moves the frame/instruments with the existing foot-contact suspension. Current instrument text and behavior remain approximate. `--prototype-cockpit` selects the previous artwork; missing or invalid graphics also use it. See [formats, import paths, inspection and limitations](docs/original-graphics.md).

## Original HERC proof pair

Import the original human Tomahawk and Cybrid Achilles base chassis:

```sh
python3 tools/import_original_models.py original/Earthsiege2_Freeware_HiRezStudios_1r0.iso
./run.sh
```

The engine reads their original geometry, textures and articulated walk poses.
Select **operation 05 / 05 — HERC Model Test Range** to inspect and fight both on
flat terrain. Nomad uses Tomahawk artwork; type-0 enemies use Achilles artwork.
Other chassis retain fallback models. `--prototype-models` restores all previous
models. The cockpit, 4:3 presentation and audio mappings are unchanged.

This is a base-chassis proof of concept: external interchangeable gun attachments,
full animation semantics and exact original lighting remain unfinished. See
[formats, test controls, verification and remaining limits](docs/original-models.md).
Original files stay in ignored `assets/original/models/` and are not packaged.

## Play the current prototype

Select an operation on the title screen, or open **Hangar** to choose a HERC. Press **Enter** or click **Deploy HERC**. See the in-game field manual with **F1**.

| Control | Action |
| --- | --- |
| W / S | Move forward / reverse |
| A / D | Turn the chassis |
| Q / E | Strafe |
| Mouse | Aim the torso independently of the legs |
| Arrow keys | Aim without a mouse |
| C | Center torso and aim |
| Shift | Turbo reactor; spools up/down and generates heat |
| Left mouse / Space | Fire selected weapon |
| Right mouse | Launch a guided missile |
| 1 / 2 / 3 | Laser / autocannon / missiles |
| T / Tab | Cycle hostile target |
| [ / ] | Decrease / increase radar range |
| Escape | Pause and release the mouse |
| F1 | Field manual |
| F2 | Independent music / weapons / machinery / interface volumes |
| M | Mute / unmute |
| F11 | Fullscreen |

Deployment begins with a brief air-drop presentation during the existing 6.2-second reactor boot. Movement and weapons unlock when the sequence completes; enemies and the mission clock wait too. Clear the separated patrol sectors across a 2.4 × 2.4 km battlefield, then reach the green extraction beacon. The heading-up radar shows enemies, waypoint/extraction bearings and relay landmarks. Set its radius with `--radar-range 800` or **[ / ]** in flight. Extraction banks your recovered salvage and a mission bonus. Defeat loses the current mission's unbanked salvage.

Mission start and kills appear in a cyan cockpit marquee. Leg kills report **ENEMY TARGET DISABLED**; torso kills report **ENEMY TARGET DESTROYED**, each with its verified original computer voice when installed. See [battlefield behavior, cockpit corrections and validation](docs/battlefield.md) for current approximations and testing instructions. Use `--size 1280x720` to select a window size.

Shields regenerate five seconds after the last hit. Weapons shut down at maximum reactor heat until the reactor cools below 42%. Guided missiles home on a target in front of you; laser and cannon shots require aiming. Buildings block movement and fire. Disabling enemy legs preserves more salvage than destroying the torso. Walk within 19 meters of a green salvage crate to collect it and repair 12 armor.

Spend banked salvage in the hangar on shield capacitors and weapon amplifiers. Upgrades apply to all three chassis. The **65-tonne Nomad** is balanced, **100-tonne Bastion** trades speed for armor and firepower, and **40-tonne Strider** favors mobility. These are prototype loadouts, not original HERC specifications. Three custom operations increase the size and strength of the enemy force. All use the same generated volcanic battlefield; original terrain is a separate compatibility milestone.

**Waypoint Test Range** is operation **04 / 05**, after Last Light in the title-screen selector (press **Right** three times from Dust to Dust). Deploy into a flat, enemy-free range and follow WP1–WP5 on the compass, radar and ground beacon. Each arrival plays the installed original “Waypoint Reached” voice once and activates the next point; WP5 completes the test. **Restart Test** repeats the route without changing campaign progress or awarding salvage. See [diagnosis, route and verification](docs/waypoint-test-range.md).

## Machinery and music

Foot impacts, rendered leg motion, and cockpit suspension share one gait phase. Contacts alternate left/right and follow actual movement, including acceleration, reverse, strafing, turning, and collision stops. Each impact layers low bass, structural resonance, plate vibration, and a pneumatic release. Mass changes acceleration, resonance, and suspension response.

Turbo increases reactor output gradually, adds a turbine spool and sustained high-power hum, and drives stronger, faster hydraulic motion. Footstep intervals shorten as the HERC actually accelerates; impact samples retain their original pitch and duration. Releasing Shift produces a descending spool as the machine returns to cruise speed. Camera displacement stays within centimetres and leaves gun aim stable.

Music comes from your own soundtrack files. Put them in `assets/audio/music/`, or link the supplied folder:

```sh
python3 tools/import_music.py Earthsiege2RemasteredSoundtrack_MP3_320kbps
```

Track `001_Power-Up Initiated.mp3` plays at the start of each level. The other tracks play during missions, with sequential/random selection, crossfades, warning ducking, and optional menu playback. WAV, FLAC, OGG/Vorbis and MP3 are supported by libsndfile. No replacement music is generated; an empty music folder is silent.

Keep the original ISO in **`original/`** and import its effects with:

```sh
python3 tools/import_original_audio.py original/Earthsiege2_Freeware_HiRezStudios_1r0.iso \
  --output imported/from-iso --map tools/original-audio-map.csv
```

ISO input requires `7z`; an extracted installation directory also works. The importer preserves the source and creates a detailed inventory. Existing mappings load original footsteps, engine, startup, weapons, impacts, explosions, lock and warning sounds. Turbo activation uses the verified original `THROTL.WAV`; turbo release and hydraulic gaps retain procedural layers. See the [import workflow and format findings](docs/original-audio.md).

Edit `assets/audio/audio.ini` for music mode, menu playback, crossfade, ducking, volumes and event mappings. Imported mappings are in ignored `assets/audio/local.ini`. Press **F2** for the existing music / weapons / machinery / interface sliders. Explicit INI volume values override saved pilot levels on launch; blank settings use the saved levels. **M** mutes every bus. Assets missing or failing to decode fall back safely.

**`original/`, extracted resources, soundtrack files and local mappings stay out of Git and public packages.** Packages include the importer and procedural fallback, and accept local assets afterward.

Audition the actual imported sounds and album through SDL:

```sh
./build/audio_preview --audio-dir assets/audio --play --output /tmp/sdl-demo.wav
```

The 28-second sequence uses the same simulation: startup, walk, turbo, spool-down, stop. Add `--machinery-only` or `--chassis 1` for the Bastion. Omit `--audio-dir` to check procedural fallback with no music. Captures containing imported audio are private local files. See [audio design and verification](docs/audio.md).

## Save data

Pilot data is saved atomically to `$XDG_DATA_HOME/earthsiege-resistance/pilot.sav`, or `~/.local/share/earthsiege-resistance/pilot.sav` if `XDG_DATA_HOME` is unset. Chassis choice, operation, mute, four audio volumes, salvage, completed operation count, and upgrades persist. Earlier ESR1 pilot saves remain readable. Missions restart from the beginning; there is no mid-mission save.

For a portable save location:

```sh
./build/earthsiege --save-dir ./pilot-data
```

If no audio device is available, the game continues silently. If a display is unavailable, run the offscreen tests below; interactive play requires a graphical session.

## Build and test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./tests/native-smoke.sh
```

Simulation tests exercise combat and a simulated pilot completing all three operations. Locomotion tests compare contact events to foot poses, check alternating steps, stops, walls, reverse/strafe/pivot, startup interlocks, weight, turbo transitions, and camera bounds. Audio tests measure stereo balance, bass energy, bus isolation, continuity, mute/pause, headroom, voice limits, and actual SDL callback output. CTest uses SDL's dummy audio driver, so it needs no speakers or display.

Native smoke tests create actual SDL/OpenGL contexts, render ten screens/scenarios, check GL errors, and exercise menu clicks, hotkeys, upgrades, pause, restart, independent music volume, and save persistence through SDL events. The model scenario selects and restarts the new range, then checks movement and targeting.

Graphics tests use synthetic DBM/DBA/DPL data, validate bounds and alignment, and check import/package isolation. With the supplied extraction, `./build/original_graphics_tests imported/from-iso/resources/VOL/SIMVOL0.VOL` also checks all nine original cockpit frames. Native smoke tests automatically use the imported Tomahawk frame when present.

For a native rendering screenshot without a display:

```sh
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  ./build/earthsiege --smoke-test --scene combat --frames 120 \
  --screenshot /tmp/earthsiege.ppm
```

Other scenes: `menu`, `hangar`, `startup`, `pause`, `controls`, `audio`, `debrief`, and `flow` (scripted UI integration test). PPM screenshots can be opened by standard image tools. `--help` lists command-line options.

Optional installation into a prefix:

```sh
cmake --install build --prefix ./build/install
./build/install/bin/earthsiege
```

To make a relocatable tar archive for a compatible Linux system:

```sh
./package.sh
```

The archive includes the game, audio audition tool, bitmap inspection tool, import scripts, fonts, icon, and documentation. Original assets are excluded. System libraries are dynamically linked; rebuild from source on Linux distributions with older/incompatible runtime libraries.

## Source layout

- `src/simulation.*`: graphics-independent combat, movement, collision, AI, and mission rules.
- `src/renderer.*`: procedural 3D models, terrain, cockpit HUD, menus, and font atlas.
- `src/original_graphics.*`, `src/original_cockpit.cpp`: bounded original bitmap/palette decoding and the first native cockpit integration.
- `tools/import_original_graphics.py`, `tools/graphics_probe.cpp`: isolated graphics import and bitmap inspection.
- `src/locomotion.cpp`: shared foot trajectories, contact transitions, and damped cockpit suspension.
- `src/synthesis.*`: procedural fallback effects, reactor synthesis, and four-bus mixer.
- `src/audio_assets.*`, `src/music_stream.*`: configuration, cached effects, decoders and bounded music streaming.
- `src/audio.*`: SDL callback, device synchronization, and optional PCM capture.
- `tools/audio_preview.cpp`: audition using the actual simulation and audio engine.
- `src/main.cpp`: native window, input, fixed-step simulation, menu actions, and pilot saves.
- `tests/`: simulation and native rendering/input integration tests.

The simulation runs at 120 Hz independently of rendering. The renderer uses the OpenGL compatibility pipeline for broad support and a deliberately low-poly appearance. Audio supports local recordings and remains playable with procedural effects alone.

## Screenshots

These show the previous prototype artwork. Screenshots containing locally imported original artwork remain private.

![Native title screen](docs/screenshots/menu.png)

![Native cockpit and combat](docs/screenshots/combat.png)

## Credits

An unofficial, independent reimplementation of [Earthsiege 2 by Dynamix/Sierra](https://www.sierragamers.com/earthsiege-2/). The public source and package include no original game code, models, textures, recordings, or campaign data. Locally imported assets remain outside version control. Earthsiege names belong to their respective owners.

Bundled Liberation Sans and Liberation Mono fonts are licensed under the SIL Open Font License. See `assets/fonts/LICENSE`.
