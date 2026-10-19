# Tomahawk Java validation attempt — 2026-10-18

Follow-up: [ES2Vol's 31 compilation errors were diagnosed and fixed](es2vol-build-diagnosis.md)
by restoring four missing upstream files. The newer attempt compiles ES2Vol,
then stops resolving a Surefire dependency. The report below records the earlier
preflight, before the user populated the initial Maven cache.

**Status: blocked before compilation by unavailable downloads.** Java and Maven
are now installed, but this execution session cannot resolve GitHub or Maven
Central. No exporter was built or executed and no actual DAT JSON output was
produced. No conversion discrepancy can yet be attributed to either the exporter
or the validator.

This resumes the [DAT audit](tomahawk-dat-audit.md) and
[synthetic validation](tomahawk-dat-validation.md) using the previously selected
references; it does not repeat the repository-wide investigation.

## Environment and recorded results

| Check | Observed result |
| --- | --- |
| Java / javac | OpenJDK 27, Arch Linux; both commands available |
| Maven | Apache Maven 3.10.0, running on Java 27 |
| Declared release target | Standalone `javac --release 9` probe compiled successfully, exit 0; this does not establish full-project JDK compatibility |
| Source download | `curl` to the pinned MDK archive on `codeload.github.com` failed with exit 6, could not resolve host |
| Dependency endpoint | `curl` to `https://repo.maven.apache.org/maven2/` likewise failed with exit 6 |
| Existing Maven artifacts | No JARs in the user's `.m2`; a new isolated Maven repository was used |
| MDK Maven dependency preflight | Exit 1; install plugin resolution failed before module compilation |
| Toolkit Maven dependency preflight | Exit 1; same plugin resolution failure before module compilation |
| Existing Python validator suite | **22 tests passed**, unchanged |
| Actual DAT → JSON conversion / comparison | **Not run**: no built exporter or real DAT JSON output available |

The previous audit caches are **partial source collections**, not complete
checkouts: MDK has 31 Java files and all 5 POMs; Toolkit has 11 Java files and all
9 POMs. Copies were used only to exercise the existing Maven configurations and
record dependency-resolution failures. These attempts are not evidence of
complete source builds. Recovering only the missing source files would not
resolve the missing Maven artifacts.

References remain MDK `1f66699118d4c0032259e8db708b5bfed50f44c7` and Toolkit
`f0f79a58bfac374c1114c796eef69918a742cb46`. No branch was substituted and no
upstream source/POM patch was applied.

For each copied root POM, the attempted command was:

```sh
mvn -B -ntp -Dmaven.repo.local=/absolute/path/to/isolated/m2 \
  -Dmaven.test.skip=true install
```

Test compilation/execution was deliberately skipped: upstream tests depend on
external game fixtures and author-specific paths. This does not bypass main
source compilation, which was never reached. The isolated MDK install would
supply the Toolkit's matching MDK dependency versions once a build can run.

Both attempts failed resolving:

```text
org.apache.maven.plugins:maven-install-plugin:pom:2.5.2
Could not transfer artifact ... from/to central
(https://repo.maven.apache.org/maven2): repo.maven.apache.org
```

The preceding Maven warning explicitly reports temporary failure in name
resolution. This is an **environment/dependency access failure**, not a Java
compilation error or converter defect. All Java modules were skipped. Maven also
reported unversioned `maven-dependency-plugin` declarations in Excavator and the
Toolkit modules; these are reproducibility warnings, not the terminating error.
Changing plugin versions would not repair unavailable network access.

Local evidence is retained under ignored
`imported/tooling-validation/java-20261018/`:

- `logs/herc-works-mdk-build.log`
- `logs/es2-toolkit-build.log`
- `logs/javac-release9.log`
- `evidence.json`

## Comparison with the validator

`python3 tests/tomahawk_dat_validator_tests.py` still passes all **22** original
synthetic tests. No validator or test changes were made. Signed integers, scalar
scaling, JSON types and error handling therefore retain their previous synthetic
coverage, but this adds no real Java conversion evidence.

No exporter/validator discrepancy was observed **because the comparison could
not run**, not because the actual exporter passed. Earlier source-derived
warnings about extra properties, string encoding, skipped bytes, input lengths
and swallowed conversion errors remain awaiting execution. Neither parser should
be patched based on this dependency failure.

## Resume once downloads are available

Required: an execution environment able to reach GitHub and Maven Central, or
complete pinned source checkouts plus a populated Maven repository containing
the declared build plugins and dependencies. No further Java/Maven installation
has been shown necessary. Do not reuse the partial audit caches as full sources.

The following is a **not-yet-executed resume recipe**, from the project root. It
uses fresh ignored directories, existing POMs and copies of the original DAT.
It stops on build/validation failure; any resulting compiler error should be
recorded before making a narrowly scoped tooling patch.

```sh
set -eu
es2_project="$PWD"
mkdir -p "$es2_project/imported/tooling-validation"
es2_run=$(mktemp -d "$es2_project/imported/tooling-validation/retry-XXXXXX")
git clone --no-checkout https://github.com/Subject9x/herc-works-mdk.git "$es2_run/mdk"
git -C "$es2_run/mdk" checkout --detach 1f66699118d4c0032259e8db708b5bfed50f44c7
git clone --no-checkout https://github.com/Subject9x/es2-toolkit.git "$es2_run/toolkit"
git -C "$es2_run/toolkit" checkout --detach f0f79a58bfac374c1114c796eef69918a742cb46

mvn -B -ntp -f "$es2_run/mdk/pom.xml" \
  -Dmaven.repo.local="$es2_run/m2" -Dmaven.test.skip=true install \
  > "$es2_run/mdk-build.log" 2>&1
mvn -B -ntp -f "$es2_run/toolkit/pom.xml" \
  -Dmaven.repo.local="$es2_run/m2" -Dmaven.test.skip=true install \
  > "$es2_run/toolkit-build.log" 2>&1

sha256sum "$es2_run/mdk/ES2Excavator/target/ES2UnVol-0.3.0.jar" \
  "$es2_project/assets/original/models/TOMAHAWK.DAT" > "$es2_run/input-hashes.txt"
for es2_pass in one two; do
  mkdir "$es2_run/$es2_pass"
  cp "$es2_run/mdk/ES2Excavator/target/ES2UnVol-0.3.0.jar" "$es2_run/$es2_pass/"
  cp "$es2_project/assets/original/models/TOMAHAWK.DAT" "$es2_run/$es2_pass/"
  (cd "$es2_run/$es2_pass" && java -jar ES2UnVol-0.3.0.jar TOMAHAWK.DAT) \
    > "$es2_run/$es2_pass/export.log" 2>&1
  python3 "$es2_project/tools/validate_tomahawk_dat.py" \
    "$es2_project/assets/original/models/TOMAHAWK.DAT" \
    "$es2_run/$es2_pass/UNPACK/DAT/TOMAHAWK.json" \
    > "$es2_run/$es2_pass/validation.log" 2>&1
  cmp "$es2_project/assets/original/models/TOMAHAWK.DAT" "$es2_run/$es2_pass/TOMAHAWK.DAT"
done
sha256sum -c "$es2_run/input-hashes.txt"
python3 - "$es2_run" <<'PY'
import json, pathlib, sys
root = pathlib.Path(sys.argv[1])
one = json.loads((root / 'one/UNPACK/DAT/TOMAHAWK.json').read_text())
two = json.loads((root / 'two/UNPACK/DAT/TOMAHAWK.json').read_text())
assert one == two, 'Nondeterministic semantic JSON output'
print('Two real exports validate and match semantically')
PY
python3 "$es2_project/tests/tomahawk_dat_validator_tests.py"
```

The CLI/output paths above come from the pinned implementation and remain
unconfirmed by execution. A zero Java exit code alone is insufficient: the
validator must find and validate the fresh output. After a successful ordinary
record, exercise a **scratch copy** truncated to 215 bytes to test the exporter's
error behavior; never truncate the original. Keep raw JSON and all generated
assets in ignored storage, and retain upstream MIT and dependency notices if
distributing any built tooling.

This attempt changes documentation only. Hash checks of existing source, tools,
tests, assets, original resources and key configuration files match the task's
baseline. No engine, extraction-path, presentation, audio or gameplay change was
made, and no copyrighted resource was added to version control.
