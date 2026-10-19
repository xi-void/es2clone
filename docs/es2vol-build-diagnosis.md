# ES2Vol compilation diagnosis — 2026-10-18

**The 31 compilation errors are fixed.** The isolated MDK directory was the
partial source cache used in the earlier [Maven dependency preflight](tomahawk-java-validation.md),
not a complete checkout. ES2Vol contained only three of its seven Java source
files. Restoring the four missing files from the pinned upstream commit clears
the compiler errors without changing any source semantics or Maven settings.

## Root cause and error chain

The failing directory is:

```text
imported/tooling-validation/java-20261018/sources/herc-works-mdk/
```

Maven's compiler input list and reproduced build both identified only
`VolEntry.java`, `io/VolFileReader.java` and `io/VolFileWriter.java`. The missing
definitions exist upstream at commit
`1f66699118d4c0032259e8db708b5bfed50f44c7`, all in
`ES2Vol/src/main/java/org/hercworks/voln/`:

| Missing source | Role and consequence |
| --- | --- |
| `DataFile.java` | Handwritten abstract superclass of `VolEntry` and `Voln`; declares the inherited file-name, raw-byte, size, path, extension and header accessors. Its absence causes the missing-method errors. |
| `FileType.java` | Handwritten enum imported by the reader and referenced by `DataFile`/`Voln`. |
| `VolDir.java` | Handwritten directory class used by the reader/writer and `Voln`; includes its own getters/setters. |
| `Voln.java` | Handwritten archive class, nested builder and enums used by the reader/writer. |

The reproduced **31 diagnostics** comprise:

- **7 primary missing-class references**, including the absent superclass and
  imports of these four classes.
- **23 cascading missing-method errors**, such as `getFileName`, `getRawBytes`,
  `getFileSize`, `getFilePath`, `getExt`, `setFileName` and `setHeader`, whose
  implementations are in `DataFile`.
- **1 cascading `@Override` error** in `VolEntry.toString`, following failure to
  resolve its superclass.

No generated-source directory, package-name mismatch, source exclusion or
nonstandard source root accounts for the absence. The package declarations are
`org.hercworks.voln`, matching the normal Maven source paths.

## Lombok, processors and Java compatibility

ES2Vol and its parent POM declare no Lombok dependency, processor path or source
generator. ES2Vol's only declared library dependency is `at.favre.lib:bytes:1.6.1`.
The accessors are ordinary methods in the Java files; adding Lombok or inventing
accessors would address the wrong cause.

The parent uses `maven-compiler-plugin:3.8.0` and
`maven.compiler.release=9`; its retained `jdk.version=1.8` property is not an
ES2Vol source exclusion. With the same OpenJDK 27 / Maven 3.10.0 installation:

- Maven successfully compiles all **7 source files** after restoration.
- An independent compile of those same seven files with
  `javac --release 9 -proc:none`, using the existing bytes 1.6.1 JAR, exits **0**.
  It produces 11 class files, including nested classes, with Java 9 class-file
  version 53.

This confirms that ES2Vol needs no annotation processing and that the reported
failure was not caused by this JDK. It does not establish JDK compatibility for
every other MDK/Toolkit module.

## Minimal resolution applied

Restored **only** `DataFile.java`, `FileType.java`, `VolDir.java` and `Voln.java`
under the existing isolated directory above. They were fetched through the
connected GitHub reader at the exact pinned commit; each restored file's Git
blob SHA-1 matches the upstream commit tree. This restores missing source rather
than patching upstream logic.

The three pre-existing sources, both relevant POMs and root MIT license were
also compared with the pinned originals. Each cached copy differs only by one
additional trailing newline. These pre-existing bytes were left unchanged.
The upstream copyright/license remains in the tooling root.

No POM edits, dependency upgrades, processor settings, accessor implementations
or unrelated refactoring were needed. Game code, original assets, extraction
paths and validator/tests match their pre-task hash baseline. Restored source
and build evidence remain inside ignored `imported/`.

## Maven package retry and remaining blocker

The package command was retried, first offline against an isolated copy of the
user's Maven cache, then with downloads enabled:

```sh
mvn -B -ntp \
  -f imported/tooling-validation/java-20261018/sources/herc-works-mdk/ES2Vol/pom.xml \
  -Dmaven.repo.local="$PWD/imported/tooling-validation/es2vol-compilation-20261018/m2" \
  -Dmaven.test.skip=true package
```

**Compilation succeeds, but `package` still exits 1 later in the test phase.**
The uncached dependency is
`org.apache.maven.surefire:maven-surefire-common:2.22.1`. The online retry's Maven
resolver record confirms:

```text
repo.maven.apache.org: Temporary failure in name resolution
```

Maven must resolve the Surefire plugin dependencies even to execute its skip
check. `maven.test.skip=true` therefore does not eliminate this prerequisite.
This is a separate dependency-download failure, not a remaining Java error.
No package JAR, successful full MDK build, upstream test pass, or actual
Tomahawk conversion is claimed. Other MDK modules in this audit cache are still
incomplete; this focused repair restores ES2Vol only.

Run the same command where Maven Central is reachable (or after populating this
isolated cache with the remaining plugin dependencies). Preserve the full
upstream source inventory before attempting the entire reactor. No additional
source fix is justified by the observed packaging failure.

Evidence in `imported/tooling-validation/es2vol-compilation-20261018/`:

- `logs/before-package.log`: reproduced 31 compiler errors from 3 sources.
- `logs/after-package.log` and `logs/online-package.log`: successful 7-source
  compilation followed by the separate Surefire resolution failure.
- `logs/no-processors.log`: successful explicit compile without processors.
- `verified-hashes.json`: upstream file/blob verification.
- `evidence.json`: diagnostic counts, changes and log hashes.

Pinned upstream references: [ES2Vol sources][sources], [module POM][pom],
[parent POM][parent].

[sources]: https://github.com/Subject9x/herc-works-mdk/tree/1f66699118d4c0032259e8db708b5bfed50f44c7/ES2Vol/src/main/java/org/hercworks/voln
[pom]: https://github.com/Subject9x/herc-works-mdk/blob/1f66699118d4c0032259e8db708b5bfed50f44c7/ES2Vol/pom.xml
[parent]: https://github.com/Subject9x/herc-works-mdk/blob/1f66699118d4c0032259e8db708b5bfed50f44c7/pom.xml
