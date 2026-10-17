# Original HERC model proof of concept

Implemented and checked against the supplied disc on 2026-10-17. This increment
loads the **human Tomahawk** and **Cybrid Achilles** base chassis directly from
original resources. It is a first-pair compatibility milestone, not a claim that
the complete HERC roster, loadouts, rendering, or movement rules are restored.

## Import and play

Keep the ISO under ignored `original/`, then run from the project root:

```sh
python3 tools/import_original_models.py original/Earthsiege2_Freeware_HiRezStudios_1r0.iso
./run.sh
```

An installation directory, `SIMVOL0.VOL`, or the existing extracted
`imported/from-iso/resources/VOL/SIMVOL0.VOL` directory works too. ISO input uses
the already available `7z`. The importer copies seven resources unchanged into
`assets/original/models/` and writes SHA-256 provenance in `import.json`:

| Files | Use |
| --- | --- |
| `TOMAHAWK.DTS`, `ACHILLES.DTS` | Geometry, material references, hierarchy and animation |
| `TOMAHAWK.DAT`, `ACHILLES.DAT` | Verify original skin bindings |
| `MEDIUM.DBA`, `ENEMY.DBA` | Original indexed model texture tiles |
| `WORLD0.DPL` | Original palette and color ramps |

No Java, Blender, intermediate OBJ files, texture downloads, or new runtime
dependencies are required. The importer rejects conflicting existing files;
it neither modifies the originals nor reads/writes audio mappings. Runtime
parsers validate resource lengths and references before uploading to OpenGL.
Missing or unsupported assets retain the procedural HERC for that model.
`--prototype-models` forces the previous models. `--original-dir PATH` selects
an original-resource root containing `models/` and the existing cockpit files.

The Nomad definition uses Tomahawk artwork in the hangar. Standard type-0 enemy
HERCs use Achilles artwork. Their existing statistics, targeting names, collision
volumes, weapons and AI remain unchanged. Bastion, Strider and the other enemy
types retain the previous models pending separate verification. First-person
player rendering and the 4:3 cockpit/camera path are unchanged; no third-person
camera was added.

## HERC Model Test Range

Select **operation 05 / 05 — HERC Model Test Range** on the title screen. From
Dust to Dust, press Left once. Deploy normally. The flat 600 × 600 m range has a
Tomahawk to the left and an Achilles to the right, initially 75 m ahead. Both
are hostile training targets, including the human chassis, so existing movement,
targeting, weapon hits, leg disabling and torso destruction can be tested without
introducing a new faction/AI system. Their fire is reduced by mission data.

Destroy both and reach extraction, or use Escape → Restart Mission. The debrief's
Restart Test button repeats the mission. The test does not advance the campaign
or bank salvage. Waypoint Test Range remains operation 04 / 05.

## What was recovered and verified

The files use the early **3Space ANShape** object tree (`0x001e0003`), not the
later Torque DTS format. The local pair contains seven serialized shape/detail
records each. The highest detail record has 12 transform nodes and eight
sequences. Lower records have reduced rigs and animation tables; all seven are
parsed and checked, while the small highest-detail mesh is used for rendering.

| Highest detail | Tomahawk | Achilles |
| --- | ---: | ---: |
| Visible mesh groups | 18 | 18 |
| Visible polygons | 106 | 115 |
| Textured polygons | 76 | 79 |
| Texture tiles in associated DBA | 36 | 45 |
| Rendered standing height in current world | 11.56 m | 13.27 m |

Original integer vertex positions, polygon boundaries, normals, skin tile
indices and hierarchy are retained. Textures use nearest filtering without
resampling or generated replacements. DBA row orientation and the source
Z-up/+Y-forward basis are converted once at the renderer boundary. The common
0.00625 world scale preserves the pair's relative proportions while fitting
the current collision/camera envelope; original metric dimensions have **not**
been independently established.

Cell-animation groups contain alternative textured/untextured/proxy geometry.
Only the textured variant is drawn. Original camera and weapon locator planes
(IDs 8/9/10/11/66/77) are excluded from visible armour. Drawing these helpers
caused floating grey strips in the initial inspection renders; the source
artwork was not defective. Their records remain in the unchanged DTS files.

The original walk keyframes are sampled through their hierarchy. Each leg's
stance/swing interval follows the existing simulation gait phase, and planted
sole vertices are corrected to the ground under that model's feet. Turbo
therefore changes animation cadence through actual motion, without an extra
animation timer or any audio changes. The native torso nodes accept yaw/pitch.

## Remaining fidelity limits

- **Separate weapons:** chassis-integral pods and mount geometry are present.
  Interchangeable guns in `MECHWPNS.DTS`/`MECHWPN2.DTS` are not attached yet.
  Original `.GL` records were inspected (five Tomahawk and seven Achilles
  hardpoints), but a verified equipment-to-shape/loadout binding is still needed.
  No unrelated gun model is substituted. In particular, the Tomahawk currently
  shows its base chassis without those external guns.
- **Lighting/material behavior:** WORLD0's original palette and indexed textures
  are decoded. Shaded surfaces use its recorded color ramps under existing
  OpenGL lighting. The original software renderer's shade lookup, haze,
  per-vertex Gouraud behavior and world-specific palette selection have not been
  reproduced or compared frame-for-frame. This is not full material fidelity.
- **Animation:** walking and torso articulation are usable; source run, stop,
  fall and transition-state semantics are not fully restored. Reverse/strafe
  motion still uses the retimed walk clip. Ground correction is an adaptation
  to current physics. Existing collapse behavior remains in use on death.
- **Other HERCs:** the local archive also contains Outlaw, Raptor2, Samson,
  Colossus, Apocalypse, Ogre, Maverick, Razor, Mongoose, Stingray, Mirimac,
  Ramses, Hyperion, Pitbull, Spider, Diablo, Headhunter, Scarab and Cerberus.
  They are inventoried, not certified by this first-pair loader. Flying and
  non-biped rigs particularly need their own tests. Do not broaden the import
  allowlist merely because a file has a DTS extension.
- **Gameplay:** these are original visual resources on prototype physics and
  loadouts. Original HERC statistics, exact component collision, equipment
  selection and faction behavior are separate compatibility work.

Resolve attachment/loadout bindings and compare controlled footage from the
running original before expanding the verified roster. That work needs no new
downloads or replacement artwork; the original resources are already local.

## Verification and reproduction

```sh
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
build/model_tests assets/original/models
./tests/native-smoke.sh
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  build/model_render assets assets/original/models imported/herc-model-audit/views
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  build/earthsiege --smoke-test --scene models --size 1280x720 \
  --save-dir /tmp/es2-model-test --screenshot /tmp/es2-model-range.ppm
```

Public tests use synthetic geometry and check truncated records, invalid vertex
indices, cyclic hierarchy, duplicate cell variants, importer conflicts and package
exclusion. Local tests additionally validate both actual resources, animation
matrices, grounded stance feet, targeting, combat notifications and restart.
The inspector produces four angles × four poses for each HERC plus mission/menu
captures at 1920×1080 and 1280×720 through SDL/OpenGL. These are private local
artifacts under ignored `imported/`.

The initial verification passed all ten CTest suites, 499 local model checks and
ten native smoke scenarios, including operation selection, launch, restart,
movement and targeting in the new range. Original audio tests also passed.
All 34 audio-directory files and the audio mapping/configuration code were
byte-compared with the pre-change snapshot and remained unchanged. The existing
cockpit source, bitmap decoder, locomotion code and Git ignore rules are unchanged.
AddressSanitizer and UndefinedBehaviorSanitizer also passed the malformed and
original-file checks. Leak detection was disabled because LeakSanitizer cannot
operate under this sandbox's ptrace environment.

Rendering was tested with SDL's offscreen OpenGL driver, not by an interactive
desktop play session. Manual follow-up: inspect the pair while circling them,
test close-range torso/leg aiming, and compare material appearance and gait with
the original game. Automated renders do not establish complete original fidelity.

## Format references

Independent bounded readers here were cross-checked with these primary sources
and the actual disc bytes; upstream tools were not assumed to support everything:

- [Open Siege early 3Space definitions/reader](https://github.com/open-siege/siege-studio/blob/c1f397b1d4c73b943b95c030649564faea65d104/siege-modules/content/siege-content-3space/src/dts/3space.cpp).
- [Herc Works DTS reader](https://github.com/Subject9x/herc-works-mdk/blob/1f66699118d4c0032259e8db708b5bfed50f44c7/ES2Core/src/main/java/org/hercworks/core/io/transform/dbsim/DTSModelTransformer.java),
  [HERC DAT notes](https://github.com/Subject9x/herc-works-mdk/blob/1f66699118d4c0032259e8db708b5bfed50f44c7/ES2Core/src/main/java/org/hercworks/core/data/file/dat/sim/HercSimDat.java),
  [bone identifiers](https://github.com/Subject9x/herc-works-mdk/blob/1f66699118d4c0032259e8db708b5bfed50f44c7/ES2Core/src/main/java/org/hercworks/core/data/ref/constants/DTSBoneId.java), and
  [gun-layout notes](https://github.com/Subject9x/herc-works-mdk/blob/1f66699118d4c0032259e8db708b5bfed50f44c7/ES2Core/src/main/java/org/hercworks/core/data/file/dbsim/GunLayout.java).
- [ES2 Toolkit exporter](https://github.com/Subject9x/es2-toolkit/blob/f0f79a58bfac374c1114c796eef69918a742cb46/DTSExtract/src/main/java/org/es2tlk/dts/DTStoObj.java) and
  [its explicit animation/export limitations](https://github.com/Subject9x/es2-toolkit/blob/f0f79a58bfac374c1114c796eef69918a742cb46/DTSExtract/README.md).

Original ISO, extracted resources, copied models, rendered captures and soundtrack
remain excluded by `original/`, `imported/`, `assets/original/` and the existing
asset-extension ignore rules. Public staging and CMake installation exclude the
original-resource tree. Never commit these resources or captures.
