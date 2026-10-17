# OpenEarthSiege 2 compatibility roadmap

Assessed 2026-10-16 against the current C++/SDL engine and the supplied Hi-Rez EarthSiege 2 disc. The target is an independent native reimplementation that runs original game content. The current executable is a compatibility prototype, not yet a replacement for the original game. Keep its playable path while replacing approximations in small, tested steps.

## Current assessment

| Area | Status before this increment | Evidence / remaining gap |
| --- | --- | --- |
| Original resource extraction | Supported subset | Bounds-checked, uncompressed `VOLN` reader; 3,004 archive members plus 65 loose recordings inventoried. Other compression/container variants unsupported. |
| Original sound playback | Supported | PCM WAV bytes retained; SDL resampling/mixing, simultaneous effects, loops and procedural fallbacks. Existing mappings are protected. |
| Turbo activation recording | Faithfully identified association | Original executable's `TURBO_POD` activation calls sound 44, `THROTL.WAV`. This does not establish original turbo performance values. |
| Other original sound associations | Working, identifier-based | Footsteps, engine, weapons, impacts, lock and warnings; preserve the accepted assignments. Not every original condition or voice ID has been reconstructed. |
| Optional remastered soundtrack | Working extension | Local discovery, boot track, streaming, separate volume, crossfades and warning ducking. Original music selection rules not reproduced. |
| Locomotion and cockpit suspension | Functional approximation | One simulation gait drives planted feet, alternating contact sounds and damped camera movement. Acceleration, stride, mass and suspension values have not been measured against the original. |
| Turbo, heat, shields and damage | Approximate | Working local simulation and tests; original equipment parameters, damage zones and timing remain unverified. |
| Targeting and weapons | Approximate | Target cycling, hitscan laser/cannon, homing missiles and obstruction checks. Original sensors, weapon roster, loadouts and detailed component damage missing. |
| Cockpit and HUD | Placeholder artwork / approximate instruments | Hand-drawn OpenGL frame and functional HUD. Original frames, gauge layouts, fonts, MFD modes and interaction were not loaded. |
| Terrain and atmosphere | Placeholder | Analytic height function and generated texture/scenery; not the original mission terrain, collision or palettes. |
| HERCs, buildings and effects | Placeholder | Procedural models; original `DTS` shapes, materials and animation not loaded. Nomad/Bastion/Strider are prototype chassis, not original specifications. |
| Missions and AI | Placeholder | Three custom operations, enemy waves, bespoke pursuit and extraction. No original mission interpreter, triggers, objectives or campaign progression. |
| Hangar and persistence | Placeholder | Custom salvage/upgrades and ESR pilot saves. Original hangar, equipment allocation, squad management and saves unsupported. |
| Native platform | Working foundation | C++17, SDL, OpenGL 2.1, libsndfile; offline play and modest hardware retained. This is platform support, not proof of gameplay fidelity. |

No complete gameplay subsystem is currently certified as an exact reimplementation. Distinguish verified resource decoding and individual associations from behavior established by comparison with the running original.

## Original resources present locally

The ignored inventory in `imported/from-iso/inventory.json` records provenance, offsets and hashes. It includes 65 DPL palettes, 10 DBM bitmaps, 253 DBA arrays, 51 HBA arrays, nine HB0 cockpit frames, 55 DTS shapes, 62 MSN and 62 ENG resources, and further binary/text resources. These counts include separate archives and do not imply unique assets or supported semantics.

Verified format references: Open Siege's [bitmap structures](https://github.com/open-siege/siege-studio/blob/53e7efb42a18bea053749f88d3c9059f76a613a0/siege-modules/content/siege-content-3space/include/siege/content/bmp/bitmap.hpp), [bitmap reader](https://github.com/open-siege/siege-studio/blob/53e7efb42a18bea053749f88d3c9059f76a613a0/siege-modules/content/siege-content-3space/src/bmp/bitmap.cpp), and [palette reader](https://github.com/open-siege/siege-studio/blob/53e7efb42a18bea053749f88d3c9059f76a613a0/siege-modules/content/siege-content-3space/src/pal/palette.cpp). Implementations here are independent; supported byte layouts are checked against local resources.

Not yet decoded semantically: HD/ED cockpit masks, VUE camera layouts, GAU instrument definitions, HFN fonts, DTS models, mission/terrain relationships, WLD settings, RMP tables, mission scripts and most DAT/DFN/BND/SNC resources. An extension is not sufficient evidence of a format. In particular, **do not treat RMP as a heightmap simply because of its name**. Preserve unknown resources and document experiments separately.

A concrete terrain starting point is `ZONES.VOL`: it contains paired `DAT/ZONE*.DAT` and `DBA/ZONE*.DBA` resources, as well as `MSN/TRAIN1.MSN` and its English companion. The zone DAT files inspected are 16 bytes; zone DBA records include 128×128 and 256×256 indexed grids that the new bitmap reader can decode. Their meaning, height scaling, world orientation and mission associations are not yet verified. Trace the small training mission's references before treating any grid values as heights or texture indices.

## Incremental plan and acceptance gates

1. **Original bitmap/palette foundation and first cockpit.** Read verified indexed DBM/DBA/HB0 and DPL data directly. Load the supplied Tomahawk frame (the reference screenshot), preserve its 640×480 proportions and indexed source artwork. Keep live prototype instruments, explicitly temporary, and drive frame motion from the existing contact suspension. Missing/corrupt assets fall back to the playable prototype. Gate: synthetic malformed-file tests, actual disc decode, native render, unchanged audio/movement hashes and existing regression suites.
2. **Complete cockpit resource layout.** Decode GAU/VUE/HD/ED and HFN with cross-checks against all nine original frames. Replace temporary instrument placements, implement original MFD modes, buttons and view switching, and associate cockpits with verified original HERC definitions. Gate: screenshot comparisons at 640×480, instrument bounds, target projection and mouse hit tests. Measure the original camera/contact motion before changing suspension constants.
3. **One original terrain and mission location, end to end.** Trace one MSN/ENG scene's dependencies to its terrain grid, height units, tile orientation, textures, palette and world settings. Record a bounded parser and provenance before changing geometry. Use the same verified heights for rendering, movement and collisions. Gate: landmarks, grid corners, slopes and routes match the original; no cracks; profiling on integrated graphics. Do not substitute a procedural landscape and call it original terrain.
4. **Original shapes and materials.** Verify this disc's DTS version, local transforms, scale, material references, detail levels and animation. Start with one static building, then one HERC. Gate: native resource inspector and known silhouette/scale; foot contact remains shared by animation and audio. Keep unsupported variants explicit.
5. **Original HERC and combat behavior.** Import verified equipment/chassis values, then movement, torso limits, shields, damage zones, targeting and individual weapons. Gate: repeatable side-by-side scenarios and measured travel, turn, shot, heat and recharge timing. Keep prototype saves/loadouts separate from original campaign state.
6. **Original missions and campaign.** Decode scripts/objectives, navigation, wingman orders/AI, briefing/debriefing, salvage/repair, hangar progression and persistence. Start with one fully playable original mission, then broaden coverage. Gate: mission outcomes and transitions agree with the reference, including failure cases.

Each step should be independently buildable and reviewable. Add bounded readers as needed; do not replace the SDL mixer, rewrite the simulation wholesale, add a framework, or change accepted sounds as a side effect. Fidelity is the default when supported original assets are installed; prototype artwork remains a fallback. Future visual enhancements must be optional.

## This increment's limits

Implemented now: the bounded bitmap/palette reader, a graphics-only importer, direct in-game Tomahawk artwork, preserved 4:3 presentation, contact-driven frame motion, and fallback rendering. Validation decoded 367 local bitmap resources and all nine HB0 frames. Seven public test suites, native screen/input smoke tests, and ASan/UBSan graphics checks pass. See [original graphics support](original-graphics.md) for reproduction steps and the one rejected inconsistent DBA resource.

The first integrated frame represents **Tomahawk artwork**, not a claim that the existing prototype chassis or its statistics have become a Tomahawk. Dynamic text, gauges, targeting, terrain, and missions remain approximations until their own compatibility work is complete. The original asset bytes stay outside the repository and packages. Runtime binary name and pilot-save location remain stable to preserve existing installations.

## Reference and project work still needed

Capture controlled original-game gameplay and cockpit sequences for behavioral comparisons; establish terrain/mission formats from actual files; select a project source-code license before presenting a public release as licensed open source. No original recordings or graphical resources should be relicensed as engine code. None of this blocks the first cockpit increment.

## Battlefield and HUD follow-up

The [battlefield increment](battlefield.md) adds a functional heading-up radar, 2.4 km-square custom mission layouts, startup-timed insertion, and mission/kill marquees. Computer kill voice IDs 50/51 are verified through SYSTEM.STR and original executable references. Every earlier sound association is retained. Cockpit target occlusion and coordinate scaling are corrected without changing the source artwork or gait.

These additions remain approximate gameplay: sensor occlusion/jamming, original terrain, missions and dropship scripts are still missing. Original Tomahawk pixels and the two newly identified voice recordings are reused; no original deployment animation or terrain resource is claimed. The next original-compatibility gates remain gauge/font/layout decoding and one verified original terrain/mission, as listed above.
