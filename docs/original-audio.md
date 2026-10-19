# Original audio import

The importer reads a local copy of the original game. It never mounts the ISO writable, patches an executable, downloads recordings, or writes into the source directory. Keep the ISO in `original/`; that entire directory is ignored by Git.

## Import and run

```sh
# Requires Python 3 and 7-Zip's `7z` command for ISO input.
python3 tools/import_original_audio.py original/Earthsiege2_Freeware_HiRezStudios_1r0.iso \
  --output imported/from-iso --map tools/original-audio-map.csv

# Alternatively, read an already extracted installation (no 7z needed).
python3 tools/import_original_audio.py /path/to/installation \
  --output imported/from-install --map tools/original-audio-map.csv

# Optional convenience: link your existing soundtrack into assets/audio/music/.
python3 tools/import_music.py Earthsiege2RemasteredSoundtrack_MP3_320kbps
# Use --copy instead of links if moving the game away from that source folder.

./run.sh
```

You can also put your soundtrack files directly in `assets/audio/music/`. The game searches recursively for WAV, FLAC, OGG/Vorbis and MP3, skipping dot files and `__MACOSX`. No replacement music is generated. With the supplied album, `001_Power-Up Initiated.mp3` plays once at each deployment; the other nine tracks form the mission playlist. The boot cue pauses with the startup simulation. Background music begins when the 6.2-second deployment sequence completes.

Paths in configuration are relative to the audio directory. Launch from anywhere with `--audio-dir /path/to/assets/audio`. Source builds automatically find the project's sibling `assets/audio`; packaged builds find the directory beside their executable. Links made by the music importer are relative and remain local.

## Verified findings on the supplied Hi-Rez disc

The game archives use the `VOLN` signature, with a folder string block, 18-byte directory records (13-byte name, one-byte folder index, little-endian offset), and a nine-byte payload header. All 3,004 archive records on this disc are uncompressed type `0x02`. Loose voice files add 65 records: **3,069 inventoried resources, 794 valid PCM WAV entries, 351 distinct WAV hashes**. Duplicate language archives and low/high-rate copies explain the repeated entries.

`SIMSOUND.VOL` contains 85 WAVs and 556 `.SNC` resources; `SHLSOUND.VOL` contains five WAVs. Each of `SIMVOICE.VOL`, `SIMVOICF.VOL`, and `SIMVOICG.VOL` contains 213 WAVs. The loose `SIMVOICE` directory supplies another 65 WAVs. See the inventory for the exact file list; counts are checked by the importer rather than inferred from extensions.

The effect recordings already contain ordinary RIFF/WAVE PCM (mostly unsigned 8-bit, with some 16-bit), commonly at 22,050 Hz in `hmi` and 11,025 Hz in `hmx`. The importer copies verified WAV bytes unchanged, retaining metadata, source rate, and sample values. Runtime libsndfile decoding and SDL resampling convert to float stereo at 48 kHz without altering playback pitch. There is no lossy re-encoding, normalization, or fabricated raw-audio header.

The reviewed mapping CSV uses identifiers found in the original `SIMVOL0.VOL/str/SOUNDS.STR`. It selects the higher-rate `hmi` version when available:

| Clone event | Original resource | Basis |
| --- | --- | --- |
| Left/right contact | `FOOT2.WAV` | Foot identifier; shared recording, separate contact timing and stereo position |
| Reactor loop | `HERCENG1.WAV` | HERC engine identifier |
| Turbo activation | `THROTL.WAV` | Original `TURBO_POD` activation method calls sound 44; `SOUNDS.STR[44]` names this recording |
| Startup, when album cue is absent | `START3.WAV` | Startup identifier |
| Laser / autocannon / missile | `LASER1.WAV` / `BACANN4.WAV` / `MISSLE.WAV` | Weapon identifiers |
| Impacts | `IMPACTS2/3/5.WAV` | Impact identifiers; cycling variants |
| Explosions | `EXPLO1D/2/3.WAV` | Explosion identifiers; cycling variants |
| Target lock | `TRGLOC.WAV` | Target-lock identifier |
| Low armor / overheat | `WARN1.WAV` / `WRNWOOP2.WAV` | Generic warning identifiers, assigned to the clone's warnings; no claim of original condition-specific speech |
| Enemy torso destruction | `CVM_0050.WAV` | `SYSTEM.STR` message 46 and verified CVM filename construction |
| Enemy leg disable | `CVM_0051.WAV` | `SYSTEM.STR` message 47 and verified CVM filename construction |

These are identifier-based assignments, not a reconstruction of every original sound ID. `GNRACT`, `GNRDACT`, `MISSIN`, `GM_69`, and numbered voices are retained without assigning a specific meaning. Turbo activation was subsequently verified directly in the supplied `DBSIM.EXE`: the `TURBO_POD` vtable at `0x498bec` references method `0x40f1f0`, which calls sound 44 at `0x40f244` when activating. That zero-based sound-table entry is `throtl.wav`. Generic activation is not assumed to mean hydraulic movement. Turbo release, hydraulic actuation, shutdown, and unidentified notifications retain procedural fallback. The original FOOT2 recording has one timbre; left/right distinction comes from placement, not invented alternate originals or pitch shifting.

## Kill notification identification

The supplied `SIMVOL0.VOL/str/SYSTEM.STR` ties message 46, `ENEMY TARGET DESTROYED`, to metadata `2e 00 00 03 06 00 14 32`; message 47, `ENEMY TARGET DISABLED`, has `2f 00 00 03 06 00 14 33`. The final byte is the voice number, not the message index. This was verified in the supplied `DBSIM.EXE` (SHA-256 `33600748810578667ca513841406bbb4f4990eee463a8a041ea09ebd12730df3`):

- At `0x434fa2`, message construction copies metadata byte 7 into the runtime message's byte `+0x2c`.
- At `0x436c8d`, computer speech starts with the filename template `CVM_0000`; `0x436ca4` reads that voice byte, and the following decimal division fills the last two digits before WAV playback.
- Thus `0x32` / `0x33` select **50 / 51**, not recordings 46 / 47.

The already extracted `SIMVOICE.VOL/SIMVOICE/CVM_0050.WAV` and `CVM_0051.WAV` are unsigned 8-bit mono PCM at 22,050 Hz, approximately 2.27 seconds each. Only these two files were copied unchanged into the audio directory; the library was not regenerated or re-extracted. Their SHA-256 values are `3eeb7a6a2f8bdef668e7df9fded70444731e9626e006b26ade9b96d62852f4d8` and `d43b04514cf7b5469ce387a1f667ba6acc98db55e15d58d348aa1c833cb12803` respectively.

The `enemy_destroyed` and `enemy_disabled` associations are additions. Existing explosion, footstep, turbo, weapon, warning, reactor and music associations and settings are unchanged. Future imports use the two additional reviewed CSV rows. Other unidentified numbered voices remain unmapped.

## Inventory and manual mapping

`imported/<output>/inventory.json` and `.csv` record each source path, source-archive SHA-256, payload SHA-256, offset, compression type, status, and WAV rate/width/channels/duration. `resources/` preserves extracted members, including unknown files. The game only receives mapped WAVs under `assets/audio/sfx/original/`. Inspect or audition the rest locally before mapping them.

Edit a copy of `tools/original-audio-map.csv`: `event,resource,evidence`. Repeat an event row for variants; use `loop.reactor`, `loop.servo`, or `loop.turbo` for continuous layers. Rerun with `--map your-map.csv`. The importer validates the WAV and requires an evidence note. It creates ignored `assets/audio/local.ini`; if an existing config differs, it writes `local.import.ini` for review instead of replacing your changes. Files with conflicting contents are never overwritten.

The script recognizes **only verified uncompressed VOLN** records and PCM WAVs. Unknown compression is inventoried and left in the original archive. Unsupported RBX/WAX/RMF containers are preserved, not falsely reported as decoded. A truncated header, unsafe path, or out-of-bounds payload stops import with an error. The ISO extraction excludes unrelated demos, DirectX installers, and AVI movies. Reusing an ISO staging directory requires a matching source hash and completed extraction marker.

## Format research and limits

[Open Siege / Siege Studio](https://github.com/open-siege/siege-studio) provides a [3Space resource reader](https://github.com/open-siege/siege-studio/blob/master/siege-modules/content/siege-content-3space/src/three_space_resource.cpp) and [VOL documentation](https://github.com/open-siege/siege-studio/blob/master/siege-modules/content/siege-content-3space/src/VOL.md). Its VOLN layout was checked against the supplied files; this project implements a small bounds-checked reader rather than bundling that application's source. The reader distinguishes uncompressed records from `0x09` LZSS/Huffman records. This importer does not implement that compressed variant. RMF describes resources across volume files and is not itself an audio codec.

[juanitogan/rbxit](https://github.com/juanitogan/rbxit) includes RBX extraction and WAX conversion tools under `tools/`, with format-specific handling. Those tools were inspected as alternatives, but no RBX, WAX, or RMF container was needed for this disc's game audio. Compatibility with another disc or WAX codec has not been claimed or tested. The `.SNC` members are retained as non-WAV resources; they are not relabeled as sound samples.

## Publication

`original/`, `imported/`, the supplied soundtrack folder, recordings, original container extensions, and local audio configs are ignored. No recording is required to build or test the project. Packages and installation use public asset allowlists, excluding user recordings and links. `tools/check_public_files.py` checks tracked/staged files before the publish helper commits anything.

The repository and public package contain the importer, configuration, mapping metadata, documentation, and procedural fallback code. Supplying a local copy does not add it to Git. Do not force-add ignored media.

Remaining manual work is optional: audition ambiguous original recordings and provide better mappings. The supplied ISO and album are enough for the implemented original effects, startup cue, and mission soundtrack.


## Waypoint arrival notification

`SYSTEM.STR` message 29, `WAYPOINT REACHED`, carries voice byte 31 and maps to `SIMVOICE.VOL/SIMVOICE/CVM_0031.WAV` through the verified message loader. The event uses `waypoint_reached`; the mapping is recorded in `tools/original-audio-map.csv`.
