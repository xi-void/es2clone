# Earthsiege II: Resistance

A **native Linux mech-combat game** written in C++17 with SDL2 and OpenGL. This is a playable, independently implemented recreation of the Earthsiege-style combat loop, with original procedural models, terrain, interface, and sound. It is a small game, not a port of the complete commercial campaign.

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

Build dependencies: a C++17 compiler, CMake 3.16+, pkg-config, SDL2, SDL2_ttf, and OpenGL development libraries. These were already available on the development machine. Runtime dependencies are SDL2, SDL2_ttf, the C++ runtime, and an OpenGL 2.1-compatible graphics driver. SDL2 compatibility implementations backed by SDL3 work too.

## Play

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
| Escape | Pause and release the mouse |
| F1 | Field manual |
| F2 | Independent music / weapons / machinery / interface volumes |
| M | Mute / unmute |
| F11 | Fullscreen |

Deployment begins with a 6.2-second reactor ignition, systems check, hydraulic pressurization, and actuator activation. Movement and weapons unlock when the sequence completes; enemies and the mission clock wait too. Destroy all enemy waves, then walk into the green extraction beacon. The windshield radar shows hostiles in cyan; green brackets mark visible targets. Extraction banks your recovered salvage and a mission bonus. Defeat loses the current mission's unbanked salvage.

Shields regenerate five seconds after the last hit. Weapons shut down at maximum reactor heat until the reactor cools below 42%. Guided missiles home on a target in front of you; laser and cannon shots require aiming. Buildings block movement and fire. Disabling enemy legs preserves more salvage than destroying the torso. Walk within 19 meters of a green salvage crate to collect it and repair 12 armor.

Spend banked salvage in the hangar on shield capacitors and weapon amplifiers. Upgrades apply to all three chassis. The **65-tonne Nomad** is balanced, **100-tonne Bastion** trades speed for armor and firepower, and **40-tonne Strider** favors mobility. Three operations increase the size and strength of the enemy force. All use the same volcanic battlefield, with basalt ridges, glowing fissures, a red sky, and a ruined relay station. The beveled three-console cockpit follows the classic simulator layout.

## Machinery and music

Foot impacts, rendered leg motion, and cockpit suspension share one gait phase. Contacts alternate left/right and follow actual movement, including acceleration, reverse, strafing, turning, and collision stops. Each impact layers low bass, structural resonance, plate vibration, and a pneumatic release. Mass changes acceleration, resonance, and suspension response.

Turbo increases reactor output gradually, adds a turbine spool and sustained high-power hum, and drives stronger, faster hydraulic motion. Footstep intervals shorten as the HERC actually accelerates; impact samples retain their original pitch and duration. Releasing Shift produces a descending spool as the machine returns to cruise speed. Camera displacement stays within centimetres and leaves gun aim stable.

An original procedural ambient score continues across menus, missions, and pauses. Press **F2** to adjust its volume independently from weapons, machinery, and interface sounds. Music leaves the lowest bass frequencies to the HERC and dips gently around impacts. All channels mix together through SDL with output headroom and a soft limiter. **M** toggles master mute. Settings persist between launches.

Generate a 28-second audition of startup, walking, turbo, spool-down, and stopping:

```sh
./build/audio_preview --output /tmp/mech-demo.wav
./build/audio_preview --play --output /tmp/sdl-demo.wav
```

`--play` uses the actual SDL audio callback and records its output. Add `--machinery-only` to mute other buses or `--chassis 1` for the Bastion (`0` Nomad, `2` Strider). The packaged archive also includes `./audio_preview`. No audio assets are downloaded. See [audio design and verification](docs/audio.md).

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

Native smoke tests create actual SDL/OpenGL contexts, render all nine screens/scenarios, check GL errors, and exercise menu clicks, hotkeys, upgrades, pause, restart, independent music volume, and save persistence through SDL events.

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

The archive includes the game, audio audition tool, fonts, icon, and documentation. System libraries are dynamically linked; rebuild from source on Linux distributions with older/incompatible runtime libraries.

## Source layout

- `src/simulation.*`: graphics-independent combat, movement, collision, AI, and mission rules.
- `src/renderer.*`: procedural 3D models, terrain, cockpit HUD, menus, and font atlas.
- `src/locomotion.cpp`: shared foot trajectories, contact transitions, and damped cockpit suspension.
- `src/synthesis.*`: procedural impact/startup/turbo assets, reactor synthesis, music, and four-bus mixer.
- `src/audio.*`: SDL callback, device synchronization, and optional PCM capture.
- `tools/audio_preview.cpp`: audition using the actual simulation and audio engine.
- `src/main.cpp`: native window, input, fixed-step simulation, menu actions, and pilot saves.
- `tests/`: simulation and native rendering/input integration tests.

The simulation runs at 120 Hz independently of rendering. The renderer uses the OpenGL compatibility pipeline for broad support and a deliberately low-poly appearance. Audio uses no recordings or external assets.

## Screenshots

![Native title screen](docs/screenshots/menu.png)

![Native cockpit and combat](docs/screenshots/combat.png)

## Credits

Inspired by [Earthsiege 2 by Dynamix/Sierra](https://www.sierragamers.com/earthsiege-2/). This project is an unofficial fan recreation; it includes no original game code, models, textures, music, or campaign data. Earthsiege names belong to their respective owners.

Bundled Liberation Sans and Liberation Mono fonts are licensed under the SIL Open Font License. See `assets/fonts/LICENSE`.
