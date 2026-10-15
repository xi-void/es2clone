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

All one-shot assets are generated locally when the mixer is created. Three mass banks and two impact/hydraulic variants occupy about 8.6 MiB. No recording, original game asset, network service, or external synthesizer is required.

## Mixing and soundtrack

Music, weapons, machinery, and interface each have an independent volume. Default levels are 22%, 80%, 100%, and 55%. Changes are smoothed to avoid clicks, and M applies a short master fade. ESR2 saves store all four levels; ESR1 saves load with defaults.

The original ambient score uses slowly crossfaded chords and sparse metallic notes. Its fundamentals sit above the strongest impact frequencies. Contacts and explosions reduce music by approximately 3 dB with a short recovery. The score continues through pause, deployment, and mission changes. Pausing fades and holds world voices while music and interface feedback continue.

A shared soft limiter bounds the final stereo output below full scale. Each bus still has its own gain before summing, so adjusting music does not change weapon or mechanical volume. The default audition peaks around 0.47 full scale, leaving about 6.6 dB of headroom.

The callback performs no allocation, file access, asset generation, or mutex locking. It uses cached samples, a fixed 32-voice pool, and interpolated sine tables. Near-zero control values settle to zero to avoid slow floating-point subnormal arithmetic. Capture storage is allocated before playback, then the callback only copies into it.

## Verification and audition

```sh
ctest --test-dir build --output-on-failure
./tests/native-smoke.sh
./build/audio_preview --output /tmp/mech-demo.wav
./build/audio_preview --play --output /tmp/sdl-demo.wav
./build/audio_preview --machinery-only --chassis 1 --output /tmp/heavy-demo.wav
```

The audition runs the same simulation and mixer as gameplay: startup, walking at 7 seconds, turbo at 13, spool-down at 19, and stopping at 23. The default Nomad run produces 20 left and 21 right contacts over 28 seconds. `--play` records the actual SDL callback; without it, the tool renders deterministic PCM offline.

Automated checks cover rendered contact alignment, alternating sides, stop/replant precision, collision stops, gait directions, mass, startup, turbo transitions, camera limits, stereo energy, bass dominance, DC offset, every sound event, isolated volume controls, music continuity, mute, world pause, and saturated mixes. The SDL integration test opens a real callback device using the dummy driver and measures its PCM output while all four buses are active.

On the development host the layered mixer benchmark ran about 77 times faster than real time. This is a host measurement, not a guarantee for every Linux machine. Native rendering tests use Mesa's software renderer and report no GL errors.

Physical speaker/headphone playback could not be auditioned in the development sandbox: the host audio device returned `Operation not permitted`. Dummy-driver callback playback and exported waveform analysis passed. Run the `--play` command in a normal graphical desktop terminal to audition your own device; remove `SDL_AUDIODRIVER=dummy` from the environment if it was set for testing.
