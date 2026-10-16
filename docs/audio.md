# Mechanical audio

The machine is driven by the 120 Hz simulation. SDL mixes stereo floating-point audio at 48 kHz, with a requested 512-frame buffer (10.7 ms). SDL can convert this stream to the device's supported format. Sound events reach the callback after the next rendered frame and device buffer; they are generated at contact, with no independent footstep timer.

## Contact and weight

`Gait::foot()` and `Gait::advance()` share the same phase. Each leg spends 60% of its cycle planted and 40% swinging. Crossing a swing boundary starts a hydraulic actuator cue; crossing a landing boundary emits a left or right impact and excites the cockpit suspension. The renderer samples these same trajectories and places the foot on the terrain.

Cadence comes from resolved movement after collision handling, including strafing and pivoting. A blocked HERC finishes any raised foot's placement, then stops making contacts. Stride length grows slightly with speed and mass. The 40, 65, and 100-tonne chassis have different acceleration, resonances, and suspension loads. Camera displacement uses a damped spring with bounded travel; no random shaking is applied to aiming.

## Sound layers

| Layer | Source and behavior |
| --- | --- |
| Foot load transfer | 36–62 Hz damped impact, selected by mass; independent L/R timbres and moderate stereo placement |
| Frame resonance | 31/74 Hz modes with longer decay beneath the impact |
| Metal vibration | Damped 137/219/351/583 Hz plate modes, filtered contact noise, short pneumatic release |
| Leg actuation | Lift-triggered pressure noise, motor and gear tones; separately generated shorter/stronger assets at higher movement speed |
| Reactor | Continuous oscillator driven by startup power, mass, and load |
| Servo/pump | Continuous motor frequency and filtered pressure noise following actual movement and acceleration |
| Turbo | Engagement and release transients plus a sustained turbine oscillator and pressure resonance following the simulation's spool state |
| Startup | Ignition, system relays, hydraulic pressure/locking, and actuator-ready cues at 0, 2.1, 3.55, and 5.2 seconds |

Impact samples always play at their original sample rate. Higher speed changes contact spacing, intensity, hydraulic activity, and engine layers. Turbo power rises over roughly 1.2 seconds and falls over roughly 1.5 seconds; chassis inertia takes longer to reach the new speed. Turbo can spool while stationary and disengages on reactor overheat.

Fallback one-shot assets are generated locally when the mixer is created. Imported effects replace their corresponding fallback voices. Three mass banks and two impact/hydraulic variants occupy about 8.6 MiB. No recording, original game asset, network service, or external synthesizer is required.

## Mixing and soundtrack

Music, weapons, machinery, and interface each have an independent volume. Default levels are 22%, 80%, 100%, and 55%. Changes are smoothed to avoid clicks, and M applies a short master fade. ESR2 saves store all four levels; ESR1 saves load with defaults.

Background music streams user-supplied WAV, FLAC, OGG/Vorbis or MP3 files through libsndfile and SDL's resampler. A worker decodes into a two-second SPSC ring; the callback performs no decoding. Track one is a bounded preloaded startup cue, excluded from the playlist. The remaining tracks play sequentially or in shuffled cycles. Missing music stays silent. Optional menu playback defaults off.

Transitions hold a short decoded tail and crossfade at actual EOF. Reported MP3 duration is not trusted: embedded artwork in the supplied remaster causes libsndfile to overestimate it. The worker uses a linear crossfade for headroom, default 0.75 seconds (configurable 0–5). Bad tracks are skipped. A depleted ring supplies silence until data arrives, preserving all effect voices.

Foot contacts/explosions briefly lower background music to 72%. Important warnings lower it to the configurable `warning_duck` level, default 32%, for at least two seconds or the warning's duration, with a smooth recovery. World voices fade and hold during pause; mission music and interface sounds continue. Startup music follows the paused startup simulation.

A linked stereo limiter uses unity gain below 0.97 full scale, instant attenuation of overloads, and a gradual release. It avoids the earlier nonlinear soft saturation so original recordings retain their timbre. Each bus has an independent smoothed gain. M fades the combined output to digital silence.

Imported effects are decoded once to 48 kHz stereo, with a 30-second per-clip and 64 MiB total cache limit. Duplicate mappings share buffers. Loaded PCM keeps the original pitch/duration and dynamics. Loop gain follows reactor, motion and turbo state; short loop-boundary crossfades avoid hard discontinuities. The supplied original engine replaces the reactor oscillator; procedural servo/turbine layers remain where no original is positively identified. See [configuration, inventory and mappings](original-audio.md).

The callback performs no allocation, file access, asset generation, or mutex locking. It uses cached samples, a fixed 32-voice pool, and interpolated sine tables. Near-zero control values settle to zero to avoid slow floating-point subnormal arithmetic. Capture storage is allocated before playback, then the callback only copies into it.

## Verification and audition

```sh
ctest --test-dir build --output-on-failure
./tests/native-smoke.sh
./build/audio_preview --output /tmp/mech-demo.wav
./build/audio_preview --audio-dir assets/audio --play --output /tmp/sdl-demo.wav
./build/audio_preview --machinery-only --chassis 1 --output /tmp/heavy-demo.wav
```

The audition runs the same simulation and mixer as gameplay: startup, walking at 7 seconds, turbo at 13, spool-down at 19, and stopping at 23. `--play` records the actual SDL callback; without it, the tool renders deterministic PCM offline.

Automated checks cover rendered contact alignment, alternating sides, stop/replant precision, collision stops, gait directions, mass, startup, turbo transitions, camera limits, stereo energy, bass dominance, DC offset, every sound event, isolated volume controls, music continuity, mute, world pause, and saturated mixes. The SDL integration test opens a real callback device using the dummy driver and measures its PCM output while all four buses are active.

On the development host the fallback mixer benchmark ran approximately 100 times faster than real time. This is a host measurement, not a guarantee for every Linux machine. Native rendering tests use Mesa's software renderer and report no GL errors.

Physical speaker/headphone playback could not be auditioned in the development sandbox: SDL reported `Pipewire: Failed to connect hotplug detection context (1)`. Dummy-driver callback playback and exported waveform analysis passed. Run the `--play` command in a normal graphical desktop terminal to audition your own device; remove `SDL_AUDIODRIVER=dummy` from the environment if it was set for testing.

Additional checks cover malformed archive bounds and paths, lossless extraction, config overrides, shared effect caching, decoder formats, corrupt music, track-one exclusion, warning ducking, gapless buffered transitions and the real ISO/album through SDL. Native capture can verify the complete rendered game path:

```sh
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy ./build/earthsiege \
  --smoke-test --scene startup --frames 1260 --audio-dir assets/audio \
  --audio-capture /tmp/es2-in-game.wav
SDL_AUDIODRIVER=dummy ./build/audio_assets_tests assets/audio
```

The capture prints counts of file-backed events, fallbacks, startup cues, music frames and the PCM peak. Gait/contact tests remain independent of the source of the sample.

Latest local verification with the supplied assets: the 28-second SDL audition produced 20 left / 21 right contacts, one boot cue, 42 file-backed events and simultaneous streamed music, with a 0.916 peak. The rendered-game capture also played one boot cue and original effects, with finite PCM bounded at 0.97. Package and install audits found no original resources or recordings. Address/undefined-behavior sanitizer checks run with leak detection disabled because LeakSanitizer cannot operate under this sandbox's tracing restrictions; leak checking is not claimed.
